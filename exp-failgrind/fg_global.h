/*--------------------------------------------------------------------*/
/*--- Failgrind data structures, functions.            fg_global.h ---*/
/*--------------------------------------------------------------------*/

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

#ifndef FG_GLOBAL
#define FG_GLOBAL

#include "pub_tool_basics.h"
#include "pub_tool_execontext.h"
#include "pub_tool_options.h"
#include "pub_tool_hashtable.h"
#include "pub_tool_tooliface.h"

#define FG_(str) VGAPPEND(vgFailgrind_,str)

//------------------------------------------------------------//
//--- Functions                                            ---//
//------------------------------------------------------------//

/* from fg_alloc.c */
void* FG_(malloc)(ThreadId tid, SizeT szB);
void* FG_(__builtin_new)(ThreadId tid, SizeT szB);
void* FG_(__builtin_new_aligned)(ThreadId tid, SizeT szB, SizeT alignB);
void* FG_(__builtin_vec_new)(ThreadId tid, SizeT szB);
void* FG_(__builtin_vec_new_aligned)(ThreadId tid, SizeT szB, SizeT alignB);
void* FG_(calloc)(ThreadId tid, SizeT m, SizeT szB);
void *FG_(memalign)(ThreadId tid, SizeT alignB, SizeT szB);
void FG_(free)(ThreadId tid __attribute__((unused)), void* p);
void FG_(__builtin_delete)(ThreadId tid, void* p);
void FG_(__builtin_delete_aligned)(ThreadId tid, void* p, SizeT align);
void FG_(__builtin_vec_delete)(ThreadId tid, void* p);
void FG_(__builtin_vec_delete_aligned)(ThreadId tid, void* p, SizeT align);
void* FG_(realloc)(ThreadId tid, void* p_old, SizeT new_szB);
SizeT FG_(malloc_usable_size)(ThreadId tid, void* p);

/* from fg_callstack.c */
void FG_(add_callstack_to_hashtable)(ExeContext *exe, UInt ecu);
Bool FG_(callstack_exists)(UInt ecu);
void FG_(cleanup_callstacks)(void);
ExeContext *FG_(get_call_execontext)(ThreadId tid, UWord *ips, UInt *n_ips);
void FG_(load_callstacks)(void);
Bool FG_(read_callstack_file)(const HChar *filename, Bool no_read_is_error);
void FG_(write_callstack)(UWord *ips, UInt n_ips);
Bool FG_(write_callstack_file)(const HChar *filename, Bool append);

/* from fg_main.c */
UInt FG_(random_100)(void);

/* from fg_syscall.c */
void FG_(parse_syscall_errno)(const HChar* arg, const HChar* value);
void FG_(pre_syscall)(ThreadId tid, SyscallStatus* status, UInt syscallno,
                           UWord* args, UInt nArgs);

void FG_(post_syscall)(ThreadId tid, UInt syscallno,
                            UWord* args, UInt nArgs, SysRes res);

//------------------------------------------------------------//
//--- Exported global variables                            ---//
//------------------------------------------------------------//

extern XArray* FG_(alloc_allow_funcs);
extern Bool FG_(alloc_fail);
extern ULong FG_(alloc_fail_count);
extern ULong FG_(alloc_new_callstack_count);
extern ULong FG_(alloc_success_count);
extern XArray* FG_(alloc_toggle_funcs);

extern VgHashTable *FG_(callstack_ht);
extern ULong FG_(callstacks_loaded);

extern HChar *FG_(execomment);
extern UInt FG_(execomment_len);

extern Bool FG_(in_main);

extern XArray* FG_(syscall_allow_funcs);
extern Bool FG_(syscall_fail);
extern ULong FG_(syscall_fail_count);
extern ULong FG_(syscall_new_callstack_count);
extern ULong FG_(syscall_success_count);
extern XArray* FG_(syscall_toggle_funcs);

//------------------------------------------------------------//
//--- Exported CLO variables                               ---//
//------------------------------------------------------------//
extern Bool FG_(clo_alloc_fail_atstart);
extern UInt FG_(clo_alloc_fail_chance);
extern UInt FG_(clo_alloc_max_fails);
extern Long FG_(clo_alloc_thresh_high);
extern Long FG_(clo_alloc_thresh_low);
extern Bool FG_(clo_alloc_thresh_invert);

extern const HChar *FG_(clo_callstack_input);
extern const HChar *FG_(clo_callstack_output);

extern Bool FG_(clo_print_failed_traces);

extern Bool FG_(clo_syscall_fail_atstart);
extern UInt FG_(clo_syscall_fail_chance);
extern UInt FG_(clo_syscall_max_fails);
extern Bool FG_(clo_syscall_specified_only);

extern Bool FG_(clo_write_callstacks_at_end);

#endif /* FG_GLOBAL */
