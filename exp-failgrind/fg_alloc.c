//--------------------------------------------------------------------*/
//--- Failgrind: a memory allocation failure testing tool          ---*/
//--------------------------------------------------------------------*/

/*
   This file is part of Failgrind, a Valgrind tool for testing program
   behaviour when heap allocations or syscalls fail.

   Copyright (C) 2018-2021 Roger Light.

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

/* Contributed by Roger Light <roger@atchoo.org> */


#include "pub_tool_basics.h"
#include "pub_tool_clientstate.h"
#include "pub_tool_gdbserver.h"
#include "pub_tool_hashtable.h"
#include "pub_tool_libcbase.h"
#include "pub_tool_libcassert.h"
#include "pub_tool_libcfile.h"
#include "pub_tool_libcprint.h"
#include "pub_tool_libcproc.h"
#include "pub_tool_machine.h"
#include "pub_tool_mallocfree.h"
#include "pub_tool_options.h"
#include "pub_tool_poolalloc.h"
#include "pub_tool_replacemalloc.h"
#include "pub_tool_stacktrace.h"
#include "pub_tool_threadstate.h"
#include "pub_tool_tooliface.h"
#include "pub_tool_vki.h"

#include "pub_tool_clreq.h"
#include "failgrind.h"
#include "fg_global.h"

static Bool should_alloc_fail(ThreadId tid, UWord *ips, UInt *n_ips, SizeT size);

/* Creates a callstack and ECU for the current context, then checks whether it
 * already exists in our hash table. If it exists, the allocation should not
 * fail. */
static Bool should_alloc_fail(ThreadId tid, UWord *ips, UInt *n_ips, SizeT size)
{
   ExeContext *exe;
   UInt ecu;
   const HChar* fnname;

   /* Check for global disable */
   if (!FG_(alloc_fail)) {
      return False;
   }

   /* Check for size outside of thresholds */
   if (FG_(clo_alloc_thresh_invert)) {
      if (FG_(clo_alloc_thresh_high) != 0 && FG_(clo_alloc_thresh_low) != 0) {
         if (size > FG_(clo_alloc_thresh_low) && size < FG_(clo_alloc_thresh_high)) {
            return False;
         }
      } else {
         if ((FG_(clo_alloc_thresh_high) != 0 && size < FG_(clo_alloc_thresh_high))
               || (FG_(clo_alloc_thresh_low) != 0 && size > FG_(clo_alloc_thresh_low))) {

            return False;
         }
      }
   } else {
      if ((FG_(clo_alloc_thresh_high) != 0 && size > FG_(clo_alloc_thresh_high))
            || (FG_(clo_alloc_thresh_low) != 0 && size < FG_(clo_alloc_thresh_low))) {

         return False;
      }
   }

   exe = FG_(get_call_execontext)(tid, ips, n_ips);
   ecu = VG_(get_ECU_from_ExeContext)(exe);

   VG_(get_fnname)(VG_(current_DiEpoch)(), ips[0], &fnname);
   if (FG_(alloc_allow_funcs) != NULL
         && VG_(strIsMemberXA)(FG_(alloc_allow_funcs), fnname)) {

      /* White list */
      return False;
   }

   if (FG_(callstack_exists)(ecu)) {
      return False;
   } else {
      FG_(alloc_new_callstack_count)++;

      /* This test must come after the callstack_exists() check, otherwise
       * FG_(alloc_new_callstack_count) will be wrong. */
      if (FG_(clo_alloc_max_fails) > 0 && FG_(alloc_fail_count) >= FG_(clo_alloc_max_fails)) {
         return False;
      }

      if (FG_(random_100)() < FG_(clo_alloc_fail_chance)) {
         FG_(add_callstack_to_hashtable)(exe, ecu);
         FG_(write_callstack)(ips, *n_ips);
         return True;
      } else {
         return False;
      }
   }
}


//------------------------------------------------------------//
//--- Actual memory allocation routine                     ---//
//------------------------------------------------------------//

static void* new_block(ThreadId tid, SizeT size, SizeT align, Bool is_zeroed)
{
   UWord ips[VG_(clo_backtrace_size)];
   UInt n_ips;
   void* p;

   if (should_alloc_fail(tid, ips, &n_ips, size)) {
      if (FG_(clo_print_failed_traces)) {
         VG_(umsg)("Heap memory failure\n");
         VG_(pp_StackTrace)(VG_(current_DiEpoch)(), ips, n_ips);
         VG_(umsg)("\n");
      }
      FG_(alloc_fail_count)++;
      return NULL;
   }

   p = VG_(cli_malloc)(align, size);
   if (p == NULL) {
      return NULL;
   }

   if (is_zeroed) {
      VG_(memset)(p, 0, size);
   }

   if (FG_(alloc_fail)) {
      FG_(alloc_success_count)++;
   }
   return p;
}


//------------------------------------------------------------//
//--- malloc() et al replacement wrappers                  ---//
//------------------------------------------------------------//

void* FG_(malloc)(ThreadId tid, SizeT szB)
{
   return new_block( tid, szB, VG_(clo_alignment), /*is_zeroed*/False);
}

void* FG_(__builtin_new)(ThreadId tid, SizeT szB)
{
   return new_block( tid, szB, VG_(clo_alignment), /*is_zeroed*/False);
}

void* FG_(__builtin_new_aligned)(ThreadId tid, SizeT szB, SizeT alignB)
{
   return new_block( tid, szB, alignB, /*is_zeroed*/False);
}

void* FG_(__builtin_vec_new)(ThreadId tid, SizeT szB)
{
   return new_block( tid, szB, VG_(clo_alignment), /*is_zeroed*/False);
}

void* FG_(__builtin_vec_new_aligned)(ThreadId tid, SizeT szB, SizeT alignB)
{
   return new_block( tid, szB, alignB, /*is_zeroed*/False);
}

void* FG_(calloc)(ThreadId tid, SizeT m, SizeT szB)
{
   return new_block( tid, m*szB, VG_(clo_alignment), /*is_zeroed*/True);
}

void *FG_(memalign)(ThreadId tid, SizeT alignB, SizeT szB)
{
   return new_block( tid, szB, alignB, False);
}

void FG_(free)(ThreadId tid __attribute__((unused)), void* p)
{
   VG_(cli_free)(p);
}

void FG_(__builtin_delete)(ThreadId tid, void* p)
{
   VG_(cli_free)(p);
}

void FG_(__builtin_delete_aligned)(ThreadId tid, void* p, SizeT align)
{
   VG_(cli_free)(p);
}

void FG_(__builtin_vec_delete)(ThreadId tid, void* p)
{
   VG_(cli_free)(p);
}

void FG_(__builtin_vec_delete_aligned)(ThreadId tid, void* p, SizeT align)
{
   VG_(cli_free)(p);
}

void* FG_(realloc)(ThreadId tid, void* p_old, SizeT new_szB)
{
   SizeT actual_szB;
   void* p_new;

   if (p_old == NULL) {
      return FG_(malloc)(tid, new_szB);
   }
   if (new_szB == 0) {
      FG_(free)(tid, p_old);
      return NULL;
   }

   actual_szB = VG_(cli_malloc_usable_size)(p_old);

   if (actual_szB >= new_szB) {
      return p_old;
   } else {
      p_new = new_block(tid, new_szB, VG_(clo_alignment), /*is_zeroed*/False);
      if (p_new != NULL) {
           VG_(memcpy)(p_new, p_old, actual_szB);
      }
      return p_new;
   }
}

SizeT FG_(malloc_usable_size)(ThreadId tid, void* p)
{
   return VG_(cli_malloc_usable_size)(p);
}


//--------------------------------------------------------------------//
//--- end                                               fg_alloc.c ---//
//--------------------------------------------------------------------//
