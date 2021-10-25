/*--------------------------------------------------------------------*/
/*--- Callgrind                                                    ---*/
/*---                                                       bbcc.c ---*/
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

#include "pub_tool_threadstate.h"

/*------------------------------------------------------------*/
/*--- BBCC operations                                      ---*/
/*------------------------------------------------------------*/

#define N_BBCC_INITIAL_ENTRIES  10437

/* BBCC table (key is BB/Context), per thread, resizable */
bbcc_hash current_bbccs;

void CLG_(init_bbcc_hash)(bbcc_hash* bbccs)
{
   Int i;

   CLG_ASSERT(bbccs != 0);

   bbccs->size    = N_BBCC_INITIAL_ENTRIES;
   bbccs->entries = 0;
   bbccs->table = (BBCC**) CLG_MALLOC("cl.bbcc.ibh.1",
                                      bbccs->size * sizeof(BBCC*));

   for (i = 0; i < bbccs->size; i++) bbccs->table[i] = NULL;
}

void CLG_(copy_current_bbcc_hash)(bbcc_hash* dst)
{
   CLG_ASSERT(dst != 0);

   dst->size    = current_bbccs.size;
   dst->entries = current_bbccs.entries;
   dst->table   = current_bbccs.table;
}

bbcc_hash* CLG_(get_current_bbcc_hash)()
{
   return &current_bbccs;
}

void CLG_(set_current_bbcc_hash)(bbcc_hash* h)
{
   CLG_ASSERT(h != 0);

   current_bbccs.size    = h->size;
   current_bbccs.entries = h->entries;
   current_bbccs.table   = h->table;
}


/* All BBCCs for recursion level 0 are inserted into a
 * thread specific hash table with key
 * - address of BB structure (unique, as never freed)
 * - current context (includes caller chain)
 * BBCCs for other recursion levels are in bbcc->rec_array.
 *
 * The hash is used in setup_bb(), i.e. to find the cost
 * counters to be changed in the execution of a BB.
 */

static __inline__
UInt bbcc_hash_idx(BB* bb, Context* cxt, UInt size)
{
   CLG_ASSERT(bb != 0);
   CLG_ASSERT(cxt != 0);

   return ((Addr)bb + (Addr)cxt) % size;
}


/* Lookup for a BBCC in hash. */
static
BBCC* lookup_bbcc(BB* bb, Context* cxt)
{
   BBCC* bbcc = bb->last_bbcc;
   UInt  idx;

   /* check LRU */
   if (bbcc->cxt == cxt) {
      if (bbcc->tid == CLG_(current_tid)) return bbcc;
   }

   idx = bbcc_hash_idx(bb, cxt, current_bbccs.size);
   bbcc = current_bbccs.table[idx];
   while (bbcc &&
         (bb      != bbcc->bb ||
         cxt     != bbcc->cxt)) {

      bbcc = bbcc->next;
   }

   return bbcc;
}


/* double size of hash table 1 (addr->BBCC) */
static void resize_bbcc_hash(void)
{
   Int i, new_size, conflicts1 = 0, conflicts2 = 0;
   BBCC** new_table;
   UInt new_idx;
   BBCC *curr_BBCC, *next_BBCC;

   new_size = 2*current_bbccs.size+3;
   new_table = (BBCC**) CLG_MALLOC("cl.bbcc.rbh.1",
                                   new_size * sizeof(BBCC*));

   for (i = 0; i < new_size; i++) {
      new_table[i] = NULL;
   }

   for (i = 0; i < current_bbccs.size; i++) {
      if (current_bbccs.table[i] == NULL) continue;

      curr_BBCC = current_bbccs.table[i];
      while (NULL != curr_BBCC) {
         next_BBCC = curr_BBCC->next;

         new_idx = bbcc_hash_idx(curr_BBCC->bb,
               curr_BBCC->cxt,
               new_size);

         curr_BBCC->next = new_table[new_idx];
         new_table[new_idx] = curr_BBCC;
         if (curr_BBCC->next) {
            conflicts1++;
            if (curr_BBCC->next->next) {
               conflicts2++;
            }
         }

         curr_BBCC = next_BBCC;
      }
   }

   VG_(free)(current_bbccs.table);

   current_bbccs.size = new_size;
   current_bbccs.table = new_table;
}


static __inline
BBCC** new_recursion(int size)
{
   BBCC** bbccs;
   int i;

   bbccs = (BBCC**) CLG_MALLOC("cl.bbcc.nr.1", sizeof(BBCC*) * size);
   for (i=0; i<size; i++) {
      bbccs[i] = 0;
   }

   return bbccs;
}


/*
 * Allocate a new BBCC
 *
 * Uninitialized:
 * cxt, next_bbcc, next1, next2
 */
static __inline__
BBCC* new_bbcc(BB* bb)
{
   BBCC* bbcc;
   Int i;

   /* We need cjmp_count+1 JmpData structs:
    * the last is for the unconditional jump/call/ret at end of BB
    */
   bbcc = (BBCC*)CLG_MALLOC("cl.bbcc.nb.1",
         sizeof(BBCC) + (bb->cjmp_count+1) * sizeof(JmpData));

   bbcc->bb  = bb;
   bbcc->tid = CLG_(current_tid);

   for (i=0; i<=bb->cjmp_count; i++) {
      bbcc->jmp[i].jcc_list = 0;
   }

   /* Init pointer caches (LRU) */
   bbcc->lru_next_bbcc = 0;
   bbcc->lru_from_jcc  = 0;
   bbcc->lru_to_jcc  = 0;

   return bbcc;
}


/**
 * Inserts a new BBCC into hashes.
 * BBCC specific items must be set as this is used for the hash
 * keys:
 *  fn     : current function
 *  tid    : current thread ID
 *  from   : position where current function is called from
 *
 * Recursion level doesn't need to be set as this is not included
 * in the hash key: Only BBCCs with rec level 0 are in hashes.
 */
static
void insert_bbcc_into_hash(BBCC* bbcc)
{
   UInt idx;

   CLG_ASSERT(bbcc->cxt != 0);

   /* check fill degree of hash and resize if needed (>90%) */
   current_bbccs.entries++;
   if (100 * current_bbccs.entries / current_bbccs.size > 90) {
      resize_bbcc_hash();
   }

   idx = bbcc_hash_idx(bbcc->bb, bbcc->cxt, current_bbccs.size);
   bbcc->next = current_bbccs.table[idx];
   current_bbccs.table[idx] = bbcc;
}

/* String is returned in a dynamically allocated buffer. Caller is
   responsible for free'ing it. */
static HChar* mangled_cxt(const Context* cxt)
{
   Int i, p;

   if (!cxt) return VG_(strdup)("cl.bbcc.mcxt", "(no context)");

   /* Overestimate the number of bytes we need to hold the string. */
   SizeT need = 20;   // rec_index + nul-terminator
   for (i = 0; i < cxt->size; ++i) {
      need += VG_(strlen)(cxt->fn[i]->name) + 1;   // 1 for leading '
   }

   HChar *mangled = CLG_MALLOC("cl.bbcc.mcxt", need);
   p = VG_(sprintf)(mangled, "%s", cxt->fn[0]->name);
   for (i=1; i<cxt->size; i++) {
      p += VG_(sprintf)(mangled+p, "'%s", cxt->fn[i]->name);
   }

   return mangled;
}


/* Create a new BBCC as a copy of an existing one,
 * but with costs set to 0 and jcc chains empty.
 *
 * This is needed when a BB is executed in another context than
 * the one at instrumentation time of the BB.
 *
 * Use cases:
 *  clone from a BBCC with differing tid/cxt
 *  and insert into hashes
 */
static BBCC* clone_bbcc(BBCC* orig, Context* cxt)
{
   BBCC* bbcc;

   bbcc = new_bbcc(orig->bb);

   /* hash insertion is only allowed if tid or cxt is different */
   CLG_ASSERT((orig->tid != CLG_(current_tid)) || (orig->cxt != cxt));

   bbcc->cxt = cxt;

   insert_bbcc_into_hash(bbcc);

   /* update list of BBCCs for same BB */
   bbcc->next_bbcc = orig->bb->bbcc_list;
   orig->bb->bbcc_list = bbcc;

   HChar *mangled_orig = mangled_cxt(orig->cxt);
   HChar *mangled_bbcc = mangled_cxt(bbcc->cxt);
   CLG_FREE(mangled_orig);
   CLG_FREE(mangled_bbcc);

   return bbcc;
}


/* Get a pointer to the cost centre structure for given basic block
 * address. If created, the BBCC is inserted into the BBCC hash.
 * Also sets BB_seen_before by reference.
 *
 */
BBCC* CLG_(get_bbcc)(BB* bb)
{
   BBCC* bbcc;

   bbcc = bb->bbcc_list;

   if (!bbcc) {
      bbcc = new_bbcc(bb);

      /* initialize BBCC */
      bbcc->cxt       = 0;

      bbcc->next_bbcc = bb->bbcc_list;
      bb->bbcc_list = bbcc;
      bb->last_bbcc = bbcc;
   }

   return bbcc;
}


/*
 * Helper function called at start of each instrumented BB to setup
 * pointer to costs for current thread/context/recursion level
 */

VG_REGPARM(1)
void CLG_(setup_bbcc)(BB* bb)
{
   BBCC *bbcc, *last_bbcc;
   Bool  call_emulation = False, delayed_push = False, skip = False;
   Addr sp;
   BB* last_bb;
   ThreadId tid;
   ClgJumpKind jmpkind;
   Int passed = 0, csp;
   Bool ret_without_call = False;
   Int popcount_on_return = 1;

   /* This is needed because thread switches can not reliable be tracked
    * with callback CLG_(run_thread) only: we have otherwise no way to get
    * the thread ID after a signal handler returns.
    * This could be removed again if that bug is fixed in Valgrind.
    * This is in the hot path but hopefully not to costly.
    */
   tid = VG_(get_running_tid)();

   /* CLG_(switch_thread) is a no-op when tid is equal to CLG_(current_tid).
    * As this is on the hot path, we only call CLG_(switch_thread)(tid)
    * if tid differs from the CLG_(current_tid).
    */
   if (UNLIKELY(tid != CLG_(current_tid))) {
      CLG_(switch_thread)(tid);
   }

   sp = VG_(get_SP)(tid);
   last_bbcc = CLG_(current_state).bbcc;
   last_bb = last_bbcc ? last_bbcc->bb : 0;

   if (last_bb) {
      passed = CLG_(current_state).jmps_passed;
      CLG_ASSERT(passed <= last_bb->cjmp_count);
      jmpkind = last_bb->jmp[passed].jmpkind;
   } else {
      jmpkind = jk_None;
   }

   /* Manipulate JmpKind if needed, only using BB specific info */

   csp = CLG_(current_call_stack).sp;

   /* A return not matching the top call in our callstack is a jump */
   if ( (jmpkind == jk_Return) && (csp >0)) {
      Int csp_up = csp-1;
      call_entry* top_ce = &(CLG_(current_call_stack).entry[csp_up]);

      /* We have a real return if
       * - the stack pointer (SP) left the current stack frame, or
       * - SP has the same value as when reaching the current function
       *   and the address of this BB is the return address of last call
       *   (we even allow to leave multiple frames if the SP stays the
       *    same and we find a matching return address)
       * The latter condition is needed because on PPC, SP can stay
       * the same over CALL=b(c)l / RET=b(c)lr boundaries
       */
      if (sp < top_ce->sp) {
         popcount_on_return = 0;
      } else if (top_ce->sp == sp) {
         while(1) {
            if (top_ce->ret_addr == bb_addr(bb)) break;
            if (csp_up>0) {
               csp_up--;
               top_ce = &(CLG_(current_call_stack).entry[csp_up]);
               if (top_ce->sp == sp) {
                  popcount_on_return++;
                  continue;
               }
            }
            popcount_on_return = 0;
            break;
         }
      }
      if (popcount_on_return == 0) {
         jmpkind = jk_Jump;
         ret_without_call = True;
      }
   }

   /* Should this jump be converted to call or pop/call ? */
   if ((jmpkind != jk_Return) && (jmpkind != jk_Call) && last_bb) {

      /* We simulate a JMP/Cont to be a CALL if
       * - jump is in another ELF object or section kind
       * - jump is to first instruction of a function (tail recursion)
       */
      if (ret_without_call ||
         /* This is for detection of optimized tail recursion.
          * On PPC, this is only detected as call when going to another
          * function. The problem is that on PPC it can go wrong
          * more easily (no stack frame setup needed)
          */
#if defined(VGA_ppc32)
            (bb->is_entry && (last_bb->fn != bb->fn)) ||
#else
            bb->is_entry ||
#endif
            (last_bb->sect_kind != bb->sect_kind) ||
            (last_bb->obj->number != bb->obj->number)) {

         jmpkind = jk_Call;
         call_emulation = True;
      }
   }

   /* Handle CALL/RET and update context to get correct BBCC */

   if (jmpkind == jk_Return) {
      if ((csp == 0) ||
            ((CLG_(current_fn_stack).top > CLG_(current_fn_stack).bottom) &&
            (*(CLG_(current_fn_stack).top-1)==0))) {

         /* Should never get here because instrumentation is always on or
          * always off. */
         CLG_ASSERT(0);
      } else {
         CLG_ASSERT(popcount_on_return >0);
         CLG_(unwind_call_stack)(sp, popcount_on_return);
      }
   } else {
      Int unwind_count = CLG_(unwind_call_stack)(sp, 0);
      if (unwind_count > 0) {
         /* if unwinding was done, this actually is a return */
         jmpkind = jk_Return;
      }

      if (jmpkind == jk_Call) {
         delayed_push = True;

         csp = CLG_(current_call_stack).sp;
         if (call_emulation && csp>0) {
            sp = CLG_(current_call_stack).entry[csp-1].sp;
         }
      }
   }

   /* Change new context if needed, taking delayed_push into account */
   if (delayed_push || (CLG_(current_state).cxt == 0)) {
      CLG_(push_cxt)(CLG_(get_fn_node)(bb));
   }
   CLG_ASSERT(CLG_(current_fn_stack).top > CLG_(current_fn_stack).bottom);

   /* If there is a fresh instrumented BBCC, assign current context */
   bbcc = CLG_(get_bbcc)(bb);
   if (bbcc->cxt == 0) {
      bbcc->cxt = CLG_(current_state).cxt;
      insert_bbcc_into_hash(bbcc);
   } else {
      /* get BBCC with current context */

      /* first check LRU of last bbcc executed */

      if (last_bbcc) {
         bbcc = last_bbcc->lru_next_bbcc;
         if (bbcc && ((bbcc->bb != bb) ||
               (bbcc->cxt != CLG_(current_state).cxt))) {

            bbcc = 0;
         }
      } else {
         bbcc = 0;
      }

      if (!bbcc) {
         bbcc = lookup_bbcc(bb, CLG_(current_state).cxt);
      }
      if (!bbcc) {
         bbcc = clone_bbcc(bb->bbcc_list, CLG_(current_state).cxt);
      }

      bb->last_bbcc = bbcc;
   }

   /* save for fast lookup */
   if (last_bbcc) {
      last_bbcc->lru_next_bbcc = bbcc;
   }

   if (delayed_push) {
      if (CLG_(current_state).nonskipped) {
         /* a call from skipped to nonskipped */
         CLG_(current_state).bbcc = CLG_(current_state).nonskipped;
         /* FIXME: take the real passed count from shadow stack */
         passed = CLG_(current_state).bbcc->bb->cjmp_count;
      }
      CLG_(push_call_stack)(CLG_(current_state).bbcc, passed, bbcc, sp, skip);
   }

   CLG_(current_state).bbcc = bbcc;
   /* Even though this will be set in instrumented code directly before
    * side exits, it needs to be set to 0 here in case an exception
    * happens in first instructions of the BB */
   CLG_(current_state).jmps_passed = 0;
}
