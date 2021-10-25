/*--------------------------------------------------------------------*/
/*--- Callgrind                                                    ---*/
/*---                                               ct_callstack.c ---*/
/*--------------------------------------------------------------------*/

/*
   This file is part of Callgrind, a Valgrind tool for call tracing.

   Copyright (C) 2002-2017, Josef Weidendorfer (Josef.Weidendorfer@gmx.de)

   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public License as
   published by the Free Software Foundation; either version 2 of the
   License, or (at your option) any later version.

   This program is distributed in the hope that it will be useful, but
   WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA
   02111-1307, USA.

   The GNU General Public License is contained in the file COPYING.
*/

#include "clg_global.h"
#include "fg_global.h"

/*------------------------------------------------------------*/
/*--- Call stack, operations                               ---*/
/*------------------------------------------------------------*/

/* Stack of current thread. Gets initialized when switching to 1st thread.
 *
 * The artificial call stack is an array of call_entry's, representing
 * stack frames of the executing program.
 * Array call_stack and call_stack_esp have same size and grow on demand.
 * Array call_stack_esp holds SPs of corresponding stack frames.
 *
 */

#define N_CALL_STACK_INITIAL_ENTRIES 500

call_stack CLG_(current_call_stack);

void CLG_(init_call_stack)(call_stack* s)
{
   CLG_ASSERT(s != 0);

   s->size = N_CALL_STACK_INITIAL_ENTRIES;
   s->entry = (call_entry*) CLG_MALLOC("cl.callstack.ics.1",
         s->size * sizeof(call_entry));

   s->sp = 0;
   s->entry[0].cxt = 0; /* for assertion in push_cxt() */
}

call_entry* CLG_(get_call_entry)(Int sp)
{
   CLG_ASSERT(sp <= CLG_(current_call_stack).sp);
   return &(CLG_(current_call_stack).entry[sp]);
}

void CLG_(copy_current_call_stack)(call_stack* dst)
{
   CLG_ASSERT(dst != 0);

   dst->size  = CLG_(current_call_stack).size;
   dst->entry = CLG_(current_call_stack).entry;
   dst->sp    = CLG_(current_call_stack).sp;
}

void CLG_(set_current_call_stack)(call_stack* s)
{
   CLG_ASSERT(s != 0);

   CLG_(current_call_stack).size  = s->size;
   CLG_(current_call_stack).entry = s->entry;
   CLG_(current_call_stack).sp    = s->sp;
}


static __inline__
void ensure_stack_size(Int i)
{
   call_stack *cs = &CLG_(current_call_stack);

   if (i < cs->size) return;

   cs->size *= 2;
   while (i > cs->size) cs->size *= 2;

   cs->entry = (call_entry*) VG_(realloc)("cl.callstack.ess.1",
         cs->entry, cs->size * sizeof(call_entry));
}



/* Called when function entered nonrecursive */
static void function_entered(fn_node* fn)
{
   CLG_ASSERT(fn != 0);

   if (!VG_(strcmp)(fn->name, "main")) {
      FG_(in_main) = True;
   }

   if (fn->toggle_alloc_fail) {
      FG_(alloc_fail) = !FG_(alloc_fail);
   }
   if (fn->toggle_syscall_fail) {
      FG_(syscall_fail) = !FG_(syscall_fail);
   }
}

/* Called when function left (no recursive level active) */
static void function_left(fn_node* fn)
{
   CLG_ASSERT(fn != 0);

   if (fn->toggle_alloc_fail) {
      FG_(alloc_fail) = !FG_(alloc_fail);
   }
   if (fn->toggle_syscall_fail) {
      FG_(syscall_fail) = !FG_(syscall_fail);
   }
}


/* Push call on call stack.
 *
 * Increment the usage count for the function called.
 * A jump from <from> to <to>, with <sp>.
 * If <skip> is true, this is a call to a function to be skipped;
 * for this, we set jcc = 0.
 */
void CLG_(push_call_stack)(BBCC* from, UInt jmp, BBCC* to, Addr sp, Bool skip)
{
   jCC* jcc;
   UInt* pdepth;
   call_entry* current_entry;
   Addr ret_addr;

   /* Ensure a call stack of size <current_sp>+1.
    * The +1 is needed as push_cxt will store the
    * context at [current_sp]
    */
   ensure_stack_size(CLG_(current_call_stack).sp +1);
   current_entry = &(CLG_(current_call_stack).entry[CLG_(current_call_stack).sp]);

   if (skip) {
      jcc = 0;
   } else {
      fn_node* to_fn = to->cxt->fn[0];

      if (CLG_(current_state).nonskipped) {
         /* this is a jmp from skipped to nonskipped */
         CLG_ASSERT(CLG_(current_state).nonskipped == from);
      }

      /* As push_cxt() has to be called before push_call_stack if not
       * skipping, the old context should already be saved on the stack */
      CLG_ASSERT(current_entry->cxt != 0);

      jcc = CLG_(get_jcc)(from, jmp, to);
      CLG_ASSERT(jcc != 0);

      pdepth = CLG_(get_fn_entry)(to_fn->number);
      (*pdepth)++;

      if (*pdepth == 1) function_entered(to_fn);
   }

   /* return address is only is useful with a real call;
    * used to detect RET w/o CALL */
   if (from->bb->jmp[jmp].jmpkind == jk_Call) {
      UInt instr = from->bb->jmp[jmp].instr;
      ret_addr = bb_addr(from->bb) +
            from->bb->instr[instr].instr_offset +
            from->bb->instr[instr].instr_size;
   } else {
      ret_addr = 0;
   }

   /* put jcc on call stack */
   current_entry->jcc = jcc;
   current_entry->sp = sp;
   current_entry->ret_addr = ret_addr;
   current_entry->nonskipped = CLG_(current_state).nonskipped;

   CLG_(current_call_stack).sp++;

   /* To allow for above assertion we set context of next frame to 0 */
   CLG_ASSERT(CLG_(current_call_stack).sp < CLG_(current_call_stack).size);
   current_entry++;
   current_entry->cxt = 0;

   if (!skip) {
      CLG_(current_state).nonskipped = 0;
   } else if (!CLG_(current_state).nonskipped) {
      /* a call from nonskipped to skipped */
      CLG_(current_state).nonskipped = from;
   }
}


/* Pop call stack and update inclusive sums.
 * Returns modified fcc.
 *
 * If the JCC becomes inactive, call entries are freed if possible
 */
void CLG_(pop_call_stack)()
{
   jCC* jcc;
   Int depth = 0;
   call_entry* lower_entry;

   if (CLG_(current_state).sig >0) {
      /* Check if we leave a signal handler; this can happen when
       * calling longjmp() in the handler */
      CLG_(run_post_signal_on_call_stack_bottom)();
   }

   lower_entry = &(CLG_(current_call_stack).entry[CLG_(current_call_stack).sp-1]);

   /* jCC item not any more on real stack: pop */
   jcc = lower_entry->jcc;
   CLG_(current_state).nonskipped = lower_entry->nonskipped;

   if (jcc) {
      fn_node* to_fn  = jcc->to->cxt->fn[0];
      UInt* pdepth =  CLG_(get_fn_entry)(to_fn->number);
      /* only decrement depth if another function was called */
      if (jcc->from->cxt->fn[0] != to_fn) (*pdepth)--;
      depth = *pdepth;

      /* restore context */
      CLG_(current_state).cxt  = lower_entry->cxt;
      CLG_(current_fn_stack).top =
            CLG_(current_fn_stack).bottom + lower_entry->fn_sp;
      CLG_ASSERT(CLG_(current_state).cxt != 0);

      if (depth == 0) function_left(to_fn);
   }

   /* To allow for an assertion in push_call_stack() */
   lower_entry->cxt = 0;

   CLG_(current_call_stack).sp--;
}


/* Unwind enough CallStack items to sync with current stack pointer.
 * Returns the number of stack frames unwinded.
 */
Int CLG_(unwind_call_stack)(Addr sp, Int minpops)
{
   Int csp;
   Int unwind_count = 0;

   /* We pop old stack frames.
    * For a call, be p the stack address with return address.
    *  - call_stack_esp[] has SP after the CALL: p-4
    *  - current sp is after a RET: >= p
    */

   while( (csp=CLG_(current_call_stack).sp) >0) {
      call_entry* top_ce = &(CLG_(current_call_stack).entry[csp-1]);

      if ((top_ce->sp < sp) || ((top_ce->sp == sp) && minpops>0)) {
         minpops--;
         unwind_count++;
         CLG_(pop_call_stack)();
         csp=CLG_(current_call_stack).sp;
         continue;
      }
      break;
   }

   return unwind_count;
}
