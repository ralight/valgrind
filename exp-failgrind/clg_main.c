
/*--------------------------------------------------------------------*/
/*--- Callgrind                                                    ---*/
/*---                                                       main.c ---*/
/*--------------------------------------------------------------------*/

/*
   This file is part of Callgrind, a Valgrind tool for call graph
   profiling programs.

   Copyright (C) 2002-2017, Josef Weidendorfer (Josef.Weidendorfer@gmx.de)

   This tool is derived from and contains code from Cachegrind
   Copyright (C) 2002-2017 Nicholas Nethercote (njn@valgrind.org)

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

#include "config.h"
#include "clg_global.h"

#include "pub_tool_threadstate.h"
#include "pub_tool_gdbserver.h"
#include "pub_tool_transtab.h"       // VG_(discard_translations_safely)

/*------------------------------------------------------------*/
/*--- Global variables                                     ---*/
/*------------------------------------------------------------*/

/* for all threads */
Statistics CLG_(stat);

/* thread and signal handler specific */
exec_state CLG_(current_state);

/*------------------------------------------------------------*/
/*--- Statistics                                           ---*/
/*------------------------------------------------------------*/

static void CLG_(init_statistics)(Statistics* s)
{
   s->distinct_objs       = 0;
   s->distinct_files      = 0;
   s->distinct_fns        = 0;
}


/*------------------------------------------------------------*/
/*--- Instrumentation structures and event queue handling  ---*/
/*------------------------------------------------------------*/

/* A struct which holds all the running state during instrumentation.
   Mostly to avoid passing loads of parameters everywhere. */
typedef struct {
   /* The array of InstrInfo's is part of BB struct. */
   BB* bb;

   /* BB seen before (ie. re-instrumentation) */
   Bool seen_before;

   /* Number InstrInfo bins 'used' so far. */
   UInt ii_index;

   // current offset of guest instructions from BB start
   UInt instr_offset;

   /* The output SB being constructed. */
   IRSB* sbOut;
} ClgState;


/* Initialise or check (if already seen before) an InstrInfo for next insn.
   We only can set instr_offset/instr_size here. The required event set and
   resulting cost offset depend on events (Ir/Dr/Dw/Dm) in guest
   instructions. The event set is extended as required on flush of the event
   queue (when Dm events were determined), cost offsets are determined at
   end of BB instrumentation. */
static
InstrInfo* next_InstrInfo ( ClgState* clgs, UInt instr_size )
{
   InstrInfo* ii;
   tl_assert(clgs->ii_index >= 0);
   tl_assert(clgs->ii_index < clgs->bb->instr_count);
   ii = &clgs->bb->instr[ clgs->ii_index ];

   if (clgs->seen_before) {
      CLG_ASSERT(ii->instr_offset == clgs->instr_offset);
      CLG_ASSERT(ii->instr_size == instr_size);
   } else {
      ii->instr_offset = clgs->instr_offset;
      ii->instr_size = instr_size;
      ii->cost_offset = 0;
   }

   clgs->ii_index++;
   clgs->instr_offset += instr_size;

   return ii;
}

/*------------------------------------------------------------*/
/*--- Instrumentation                                      ---*/
/*------------------------------------------------------------*/

#if defined(VG_BIGENDIAN)
# define CLGEndness Iend_BE
#elif defined(VG_LITTLEENDIAN)
# define CLGEndness Iend_LE
#else
# error "Unknown endianness"
#endif

static
Addr IRConst2Addr(IRConst* con)
{
   Addr addr;

   if (sizeof(RegWord) == 4) {
      CLG_ASSERT( con->tag == Ico_U32 );
      addr = con->Ico.U32;
   } else if (sizeof(RegWord) == 8) {
      CLG_ASSERT( con->tag == Ico_U64 );
      addr = con->Ico.U64;
   } else {
      VG_(tool_panic)("Callgrind: invalid Addr type");
   }

   return addr;
}

/* First pass over a BB to instrument, counting instructions and jumps
 * This is needed for the size of the BB struct to allocate
 *
 * Called from CLG_(get_bb)
 */
void CLG_(collectBlockInfo)(IRSB* sbIn,
      /*INOUT*/ UInt* instrs,
      /*INOUT*/ UInt* cjmps,
      /*INOUT*/ Bool* cjmp_inverted)
{
   Int i;
   IRStmt* st;
   Addr instrAddr =0, jumpDst;
   UInt instrLen = 0;
   Bool toNextInstr = False;

   // Ist_Exit has to be ignored in preamble code, before first IMark:
   // preamble code is added by VEX for self modifying code, and has
   // nothing to do with client code
   Bool inPreamble = True;

   if (!sbIn) return;

   for (i = 0; i < sbIn->stmts_used; i++) {
      st = sbIn->stmts[i];
      if (Ist_IMark == st->tag) {
         inPreamble = False;

         instrAddr = st->Ist.IMark.addr;
         instrLen  = st->Ist.IMark.len;

         (*instrs)++;
         toNextInstr = False;
      }
      if (inPreamble) continue;
      if (Ist_Exit == st->tag) {
         jumpDst = IRConst2Addr(st->Ist.Exit.dst);
         toNextInstr =  (jumpDst == instrAddr + instrLen);

         (*cjmps)++;
      }
   }

   /* if the last instructions of BB conditionally jumps to next instruction
    * (= first instruction of next BB in memory), this is a inverted by VEX.
    */
   *cjmp_inverted = toNextInstr;
}

static
void addConstMemStoreStmt( IRSB* bbOut, UWord addr, UInt val, IRType hWordTy)
{
   addStmtToIRSB( bbOut,
                  IRStmt_Store(CLGEndness,
                               IRExpr_Const(hWordTy == Ity_I32 ?
                                            IRConst_U32( addr ) :
                                            IRConst_U64( addr )),
                               IRExpr_Const(IRConst_U32(val)) ));
}


/* add helper call to setup_bbcc, with pointer to BB struct as argument
 *
 * precondition for setup_bbcc:
 * - jmps_passed has number of cond.jumps passed in last executed BB
 * - current_bbcc has a pointer to the BBCC of the last executed BB
 *   Thus, if bbcc_jmpkind is != -1 (JmpNone),
 *     current_bbcc->bb->jmp_addr
 *   gives the address of the jump source.
 *
 * the setup does 2 things:
 * - trace call:
 *   * Unwind own call stack, i.e sync our ESP with real ESP
 *     This is for ESP manipulation (longjmps, C++ exec handling) and RET
 *   * For CALLs or JMPs crossing objects, record call arg +
 *     push are on own call stack
 *
 * - prepare for cache log functions:
 *   set current_bbcc to BBCC that gets the costs for this BB execution
 *   attached
 */
static
void addBBSetupCall(ClgState* clgs)
{
   IRDirty* di;
   IRExpr *arg1, **argv;

   arg1 = mkIRExpr_HWord((HWord)clgs->bb);
   argv = mkIRExprVec_1(arg1);
   di = unsafeIRDirty_0_N(1, "setup_bbcc",
                          VG_(fnptr_to_fnentry)(&CLG_(setup_bbcc)),
                          argv);
   addStmtToIRSB(clgs->sbOut, IRStmt_Dirty(di));
}


IRSB* CLG_(instrument)( VgCallbackClosure* closure,
                        IRSB* sbIn,
                        const VexGuestLayout* layout,
                        const VexGuestExtents* vge,
                        const VexArchInfo* archinfo_host,
                        IRType gWordTy, IRType hWordTy )
{
   Int i;
   IRStmt* st;
   Addr origAddr;
   InstrInfo* curr_inode = NULL;
   ClgState clgs;
   UInt cJumps = 0;

   if (gWordTy != hWordTy) {
      /* We don't currently support this case. */
      VG_(tool_panic)("host/guest word size mismatch");
   }

   /* Set up SB for instrumented IR */
   clgs.sbOut = deepCopyIRSBExceptStmts(sbIn);

   // Copy verbatim any IR preamble preceding the first IMark
   i = 0;
   while (i < sbIn->stmts_used && sbIn->stmts[i]->tag != Ist_IMark) {
      addStmtToIRSB( clgs.sbOut, sbIn->stmts[i] );
      i++;
   }

   // Get the first statement, and origAddr from it
   CLG_ASSERT(sbIn->stmts_used >0);
   CLG_ASSERT(i < sbIn->stmts_used);
   st = sbIn->stmts[i];
   CLG_ASSERT(Ist_IMark == st->tag);

   origAddr = st->Ist.IMark.addr + st->Ist.IMark.delta;
   CLG_ASSERT(origAddr == st->Ist.IMark.addr
                          + st->Ist.IMark.delta);  // XXX: check no overflow

   /* Get BB struct (creating if necessary).
    * JS: The hash table is keyed with orig_addr_noredir -- important!
    * JW: Why? If it is because of different chasing of the redirection,
    *     this is not needed, as chasing is switched off in callgrind
    */
   clgs.bb = CLG_(get_bb)(origAddr, sbIn, &(clgs.seen_before));

   addBBSetupCall(&clgs);

   // Set up running state
   clgs.ii_index = 0;
   clgs.instr_offset = 0;

   for (/*use current i*/; i < sbIn->stmts_used; i++) {

      st = sbIn->stmts[i];
      CLG_ASSERT(isFlatIRStmt(st));

      switch (st->tag) {
         case Ist_NoOp:
         case Ist_AbiHint:
         case Ist_Put:
         case Ist_PutI:
         case Ist_MBE:
         case Ist_WrTmp:
         case Ist_Store:
         case Ist_StoreG:
         case Ist_LoadG:
         case Ist_Dirty:
         case Ist_LLSC:
         case Ist_CAS:
            break;

         case Ist_IMark: {
            Addr   cia   = st->Ist.IMark.addr + st->Ist.IMark.delta;
            UInt   isize = st->Ist.IMark.len;
            CLG_ASSERT(clgs.instr_offset == cia - origAddr);
            // If Vex fails to decode an instruction, the size will be zero.
            // Pretend otherwise.
            if (isize == 0) isize = VG_MIN_INSTR_SZB;

            // Sanity-check size.
            tl_assert( (VG_MIN_INSTR_SZB <= isize && isize <= VG_MAX_INSTR_SZB)
                  || VG_CLREQ_SZB == isize );

            // Init the inode, record it as the current one.
            // Subsequent Dr/Dw/Dm events from the same instruction will
            // also use it.
            curr_inode = next_InstrInfo (&clgs, isize);
            break;
         }

         case Ist_Exit: {
            Bool guest_exit, inverted;

            /* VEX code generation sometimes inverts conditional branches.
             * As Callgrind counts (conditional) jumps, it has to correct
             * inversions. The heuristic is the following:
             * (1) Callgrind switches off SB chasing and unrolling, and
             *     therefore it assumes that a candidate for inversion only is
             *     the last conditional branch in an SB.
             * (2) inversion is assumed if the branch jumps to the address of
             *     the next guest instruction in memory.
             * This heuristic is precalculated in CLG_(collectBlockInfo)().
             *
             * Branching behavior is also used for branch prediction. Note that
             * above heuristic is different from what Cachegrind does.
             * Cachegrind uses (2) for all branches.
             */
            if (cJumps+1 == clgs.bb->cjmp_count) {
                inverted = clgs.bb->cjmp_inverted;
            } else {
                inverted = False;
            }

            // call branch predictor only if this is a branch in guest code
            guest_exit = (st->Ist.Exit.jk == Ijk_Boring) ||
                         (st->Ist.Exit.jk == Ijk_Call) ||
                         (st->Ist.Exit.jk == Ijk_Ret);

            if (guest_exit) {
               /* Stuff to widen the guard expression to a host word, so
                  we can pass it to the branch predictor simulation
                  functions easily. */
               IRType   tyW    = hWordTy;
               IROp     widen  = tyW==Ity_I32  ? Iop_1Uto32  : Iop_1Uto64;
               IROp     opXOR  = tyW==Ity_I32  ? Iop_Xor32   : Iop_Xor64;
               IRTemp   guard1 = newIRTemp(clgs.sbOut->tyenv, Ity_I1);
               IRTemp   guardW = newIRTemp(clgs.sbOut->tyenv, tyW);
               IRTemp   guard  = newIRTemp(clgs.sbOut->tyenv, tyW);
               IRExpr*  one    = tyW==Ity_I32 ? IRExpr_Const(IRConst_U32(1))
                                              : IRExpr_Const(IRConst_U64(1));

               /* Widen the guard expression. */
               addStmtToIRSB( clgs.sbOut,
                              IRStmt_WrTmp( guard1, st->Ist.Exit.guard ));
               addStmtToIRSB( clgs.sbOut,
                              IRStmt_WrTmp( guardW,
                                            IRExpr_Unop(widen,
                                                        IRExpr_RdTmp(guard1))) );
               /* If the exit is inverted, invert the sense of the guard. */
               addStmtToIRSB(
                       clgs.sbOut,
                       IRStmt_WrTmp(
                               guard,
                               inverted ? IRExpr_Binop(opXOR, IRExpr_RdTmp(guardW), one)
                                   : IRExpr_RdTmp(guardW)
                                   ));
            }

            CLG_ASSERT(clgs.ii_index>0);
            if (!clgs.seen_before) {
               ClgJumpKind jk;

               if      (st->Ist.Exit.jk == Ijk_Call) jk = jk_Call;
               else if (st->Ist.Exit.jk == Ijk_Ret)  jk = jk_Return;
               else {
                  if (IRConst2Addr(st->Ist.Exit.dst) ==
                        origAddr + curr_inode->instr_offset + curr_inode->instr_size) {

                     jk = jk_None;
                  } else {
                     jk = jk_Jump;
                  }
               }

               clgs.bb->jmp[cJumps].instr = clgs.ii_index-1;
               clgs.bb->jmp[cJumps].jmpkind = jk;
            }

            /* Update global variable jmps_passed before the jump
             * A correction is needed if VEX inverted the last jump condition
             */
            UInt val = inverted ? cJumps+1 : cJumps;
            addConstMemStoreStmt( clgs.sbOut,
                  (UWord) &CLG_(current_state).jmps_passed,
                  val, hWordTy);
            cJumps++;

            break;
         }

         default:
            tl_assert(0);
            break;
      }

      /* Copy the original statement */
      addStmtToIRSB( clgs.sbOut, st );
   }

   /* Update global variable jmps_passed at end of SB.
    * As CLG_(current_state).jmps_passed is reset to 0 in setup_bbcc,
    * this can be omitted if there is no conditional jump in this SB.
    * A correction is needed if VEX inverted the last jump condition
    */
   if (cJumps>0) {
      UInt jmps_passed = cJumps;
      if (clgs.bb->cjmp_inverted) jmps_passed--;
      addConstMemStoreStmt(clgs.sbOut,
            (UWord) &CLG_(current_state).jmps_passed,
            jmps_passed, hWordTy);
   }
   CLG_ASSERT(clgs.bb->cjmp_count == cJumps);
   CLG_ASSERT(clgs.bb->instr_count == clgs.ii_index);

   /* Info for final exit from BB */
   {
      ClgJumpKind jk;

      if      (sbIn->jumpkind == Ijk_Call) jk = jk_Call;
      else if (sbIn->jumpkind == Ijk_Ret)  jk = jk_Return;
      else {
         jk = jk_Jump;
         if ((sbIn->next->tag == Iex_Const) &&
               (IRConst2Addr(sbIn->next->Iex.Const.con) ==
               origAddr + clgs.instr_offset)) {

            jk = jk_None;
         }
      }
      clgs.bb->jmp[cJumps].jmpkind = jk;
      /* Instruction index of the call/ret at BB end
       * (it is wrong for fall-through, but does not matter) */
      clgs.bb->jmp[cJumps].instr = clgs.ii_index-1;
   }

   /* swap information of last exit with final exit if inverted */
   if (clgs.bb->cjmp_inverted) {
      ClgJumpKind jk;
      UInt instr;

      jk = clgs.bb->jmp[cJumps].jmpkind;
      clgs.bb->jmp[cJumps].jmpkind = clgs.bb->jmp[cJumps-1].jmpkind;
      clgs.bb->jmp[cJumps-1].jmpkind = jk;
      instr = clgs.bb->jmp[cJumps].instr;
      clgs.bb->jmp[cJumps].instr = clgs.bb->jmp[cJumps-1].instr;
      clgs.bb->jmp[cJumps-1].instr = instr;
   }

   if (clgs.seen_before) {
      CLG_ASSERT(clgs.bb->instr_len == clgs.instr_offset);
   } else {
      clgs.bb->instr_len = clgs.instr_offset;
   }

   return clgs.sbOut;
}

/*--------------------------------------------------------------------*/
/*--- Discarding BB info                                           ---*/
/*--------------------------------------------------------------------*/

// Called when a translation is removed from the translation cache for
// any reason at all: to free up space, because the guest code was
// unmapped or modified, or for any arbitrary reason.
static
void clg_discard_superblock_info ( Addr orig_addr, VexGuestExtents vge )
{
   tl_assert(vge.n_used > 0);

   // Get BB info, remove from table, free BB info.  Simple!
   // When created, the BB is keyed by the first instruction address,
   // (not orig_addr, but eventually redirected address). Thus, we
   // use the first instruction address in vge.
   CLG_(delete_bb)(vge.base[0]);
}


/*------------------------------------------------------------*/
/*--- CLG_(fini)() and related function                     ---*/
/*------------------------------------------------------------*/

static
void unwind_thread(thread_info* t)
{
   /* unwind signal handlers */
   while (CLG_(current_state).sig != 0) {
      CLG_(post_signal)(CLG_(current_tid),CLG_(current_state).sig);
   }

   /* unwind regular call stack */
   while(CLG_(current_call_stack).sp>0) {
      CLG_(pop_call_stack)();
   }

   /* reset context and function stack for context generation */
   CLG_(init_exec_state)( &CLG_(current_state) );
   CLG_(current_fn_stack).top = CLG_(current_fn_stack).bottom;
}

static
void finish(void)
{
  /* pop all remaining items from CallStack for correct sum
   */
  CLG_(forall_threads)(unwind_thread);
}


void CLG_(fini)(Int exitcode)
{
   finish();
}


/*--------------------------------------------------------------------*/
/*--- Setup                                                        ---*/
/*--------------------------------------------------------------------*/

static void clg_start_client_code_callback ( ThreadId tid, ULong blocks_done )
{
   static ULong last_blocks_done = 0;

   /* throttle calls to CLG_(run_thread) by number of BBs executed */
   if (blocks_done - last_blocks_done < 5000) return;
   last_blocks_done = blocks_done;

   CLG_(run_thread)( tid );
}

void CLG_(post_clo_init)(void)
{
   VG_(needs_superblock_discards)(clg_discard_superblock_info);

   VG_(track_start_client_code)  ( & clg_start_client_code_callback );
   VG_(track_pre_deliver_signal) ( & CLG_(pre_signal) );
   VG_(track_post_deliver_signal)( & CLG_(post_signal) );

   VG_(clo_vex_control).iropt_unroll_thresh = 0;   // cannot be overridden.
   VG_(clo_vex_control).guest_chase = False; // cannot be overridden.

   CLG_(init_statistics)(& CLG_(stat));

   /* initialize hash tables */
   CLG_(init_obj_table)();
   CLG_(init_cxt_table)();
   CLG_(init_bb_hash)();

   CLG_(init_threads)();
   CLG_(run_thread)(1);
}

/*--------------------------------------------------------------------*/
/*--- end                                                   main.c ---*/
/*--------------------------------------------------------------------*/
