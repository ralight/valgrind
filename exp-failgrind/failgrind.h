/*
   ----------------------------------------------------------------

   Notice that the following BSD-style license applies to this one
   file (failgrind.h) only.  The rest of Valgrind is licensed under the
   terms of the GNU General Public License, version 2, unless
   otherwise indicated.  See the COPYING file in the source
   distribution for details.

   ----------------------------------------------------------------

   This file is part of Failgrind, a Valgrind tool for testing program
   behaviour when heap allocations or syscalls fail.

   Copyright (C) 2018-2021 Roger Light.  All rights reserved.

   Redistribution and use in source and binary forms, with or without
   modification, are permitted provided that the following conditions
   are met:

   1. Redistributions of source code must retain the above copyright
      notice, this list of conditions and the following disclaimer.

   2. The origin of this software must not be misrepresented; you must
      not claim that you wrote the original software.  If you use this
      software in a product, an acknowledgment in the product
      documentation would be appreciated but is not required.

   3. Altered source versions must be plainly marked as such, and must
      not be misrepresented as being the original software.

   4. The name of the author may not be used to endorse or promote
      products derived from this software without specific prior written
      permission.

   THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS
   OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
   WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
   ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
   DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
   DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE
   GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
   INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
   WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
   NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
   SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

   ----------------------------------------------------------------

   Notice that the above BSD-style license applies to this one file
   (failgrind.h) only.  The entire rest of Valgrind is licensed under
   the terms of the GNU General Public License, version 2.  See the
   COPYING file in the source distribution for details.

   ----------------------------------------------------------------
*/

#ifndef __FAILGRIND_H
#define __FAILGRIND_H

#include "pub_tool_clreq.h"

/* !! ABIWARNING !! ABIWARNING !! ABIWARNING !! ABIWARNING !!
   This enum comprises an ABI exported by Valgrind to programs
   which use client requests.  DO NOT CHANGE THE ORDER OF THESE
   ENTRIES, NOR DELETE ANY -- add new ones at the end.
 */

typedef
   enum {
      VG_USERREQ__FG_ALLOC_FAIL_ON = VG_USERREQ_TOOL_BASE('F','G'),
      VG_USERREQ__FG_ALLOC_FAIL_OFF,
      VG_USERREQ__FG_ALLOC_FAIL_TOGGLE,
      VG_USERREQ__FG_ALLOC_GET_FAIL_COUNT,
      VG_USERREQ__FG_ALLOC_GET_SUCCESS_COUNT,
      VG_USERREQ__FG_ALLOC_GET_NEW_CALLSTACK_COUNT,
      VG_USERREQ__FG_CALLSTACK_APPEND,
      VG_USERREQ__FG_CALLSTACK_CLEAR,
      VG_USERREQ__FG_CALLSTACK_READ,
      VG_USERREQ__FG_CALLSTACK_WRITE,
      VG_USERREQ__FG_SYSCALL_FAIL_ON,
      VG_USERREQ__FG_SYSCALL_FAIL_OFF,
      VG_USERREQ__FG_SYSCALL_FAIL_TOGGLE,
      VG_USERREQ__FG_SYSCALL_GET_FAIL_COUNT,
      VG_USERREQ__FG_SYSCALL_GET_SUCCESS_COUNT,
      VG_USERREQ__FG_SYSCALL_GET_NEW_CALLSTACK_COUNT,
      VG_USERREQ__FG_ZERO_COUNTS,
   } Vg_FailgrindClientRequest;

/* Enable heap allocation failures. */
#define FAILGRIND_ALLOC_FAIL_ON                                 \
  VALGRIND_DO_CLIENT_REQUEST_STMT(VG_USERREQ__FG_ALLOC_FAIL_ON, \
                                  0, 0, 0, 0, 0)

/* Disable heap allocation failures. */
#define FAILGRIND_ALLOC_FAIL_OFF                                \
  VALGRIND_DO_CLIENT_REQUEST_STMT(VG_USERREQ__FG_ALLOC_FAIL_OFF,\
                                  0, 0, 0, 0, 0)

/* Toggle heap allocation failures enable/disable state. */
#define FAILGRIND_ALLOC_FAIL_TOGGLE                             \
  VALGRIND_DO_CLIENT_REQUEST_STMT(VG_USERREQ__FG_ALLOC_FAIL_TOGGLE,\
                                  0, 0, 0, 0, 0)

/* Return the count of heap memory allocations that have been failed
 * through Failgrind. */
#define FAILGRIND_ALLOC_GET_FAIL_COUNT                          \
  (unsigned)VALGRIND_DO_CLIENT_REQUEST_EXPR(0,                  \
          VG_USERREQ__FG_ALLOC_GET_FAIL_COUNT, 0, 0, 0, 0, 0)

/* Return the count of new heap memory allocation callstacks that have been
 * found on this run. This count will be the same as
 * FAILGRIND_ALLOC_GET_FAIL_COUNT in normal operation, but gives a better count
 * for the situation where --alloc-max-fails or --alloc-fail-chance are being used. */
#define FAILGRIND_ALLOC_GET_NEW_CALLSTACK_COUNT            \
  (unsigned)VALGRIND_DO_CLIENT_REQUEST_EXPR(0,                  \
          VG_USERREQ__FG_ALLOC_GET_NEW_CALLSTACK_COUNT, 0, 0, 0, 0, 0)

/* Return the count of heap memory allocations that have succeeded
 * through Failgrind. */
#define FAILGRIND_ALLOC_GET_SUCCESS_COUNT                       \
  (unsigned)VALGRIND_DO_CLIENT_REQUEST_EXPR(0,                  \
          VG_USERREQ__FG_ALLOC_GET_SUCCESS_COUNT, 0, 0, 0, 0, 0)

/* Clear the in-memory list of stored call stacks (i.e. after calling
 * this, if a call stack that had been seen before is seen again, it
 * will fail again) */
#define FAILGRIND_CALLSTACK_CLEAR                                   \
  VALGRIND_DO_CLIENT_REQUEST_STMT(VG_USERREQ__FG_CALLSTACK_CLEAR,   \
                                  0, 0, 0, 0, 0)

/* Load a file of stored call stacks into memory. */
#define FAILGRIND_CALLSTACK_READ(fname)                             \
  VALGRIND_DO_CLIENT_REQUEST_STMT(VG_USERREQ__FG_CALLSTACK_READ,\
                                  fname, 0, 0, 0, 0)

/* Writes the in-memory list of stored call stacks to a new file. */
#define FAILGRIND_CALLSTACK_WRITE(fname)                            \
  VALGRIND_DO_CLIENT_REQUEST_STMT(VG_USERREQ__FG_CALLSTACK_WRITE,\
                                  fname, 0, 0, 0, 0)

/* Appends the in-memory list of stored call stacks to an existing file. */
#define FAILGRIND_CALLSTACK_APPEND(fname)                                \
  VALGRIND_DO_CLIENT_REQUEST_STMT(VG_USERREQ__FG_CALLSTACK_APPEND,      \
                                  fname, 0, 0, 0, 0)

/* Enable syscall failures. */
#define FAILGRIND_SYSCALL_FAIL_ON                                 \
  VALGRIND_DO_CLIENT_REQUEST_STMT(VG_USERREQ__FG_SYSCALL_FAIL_ON, \
                                  0, 0, 0, 0, 0)

/* Disable syscall failures. */
#define FAILGRIND_SYSCALL_FAIL_OFF                                \
  VALGRIND_DO_CLIENT_REQUEST_STMT(VG_USERREQ__FG_SYSCALL_FAIL_OFF,\
                                  0, 0, 0, 0, 0)

/* Toggle syscall failures enable/disable state. */
#define FAILGRIND_SYSCALL_FAIL_TOGGLE                             \
  VALGRIND_DO_CLIENT_REQUEST_STMT(VG_USERREQ__FG_SYSCALL_FAIL_TOGGLE,\
                                  0, 0, 0, 0, 0)

/* Return the count of syscalls that have been failed through Failgrind. */
#define FAILGRIND_SYSCALL_GET_FAIL_COUNT                          \
  (unsigned)VALGRIND_DO_CLIENT_REQUEST_EXPR(0,                  \
          VG_USERREQ__FG_SYSCALL_GET_FAIL_COUNT, 0, 0, 0, 0, 0)

/* Return the count of new syscall callstacks that have been found on this run.
 * This count will be the same as FAILGRIND_SYSCALL_GET_FAIL_COUNT in normal
 * operation, but gives a better count for the situation where
 * --syscall-max-fails or --syscall-fail-chance are being used. */
#define FAILGRIND_SYSCALL_GET_NEW_CALLSTACK_COUNT            \
  (unsigned)VALGRIND_DO_CLIENT_REQUEST_EXPR(0,                  \
          VG_USERREQ__FG_SYSCALL_GET_NEW_CALLSTACK_COUNT, 0, 0, 0, 0, 0)

/* Return the count of syscall that have succeeded through Failgrind. */
#define FAILGRIND_SYSCALL_GET_SUCCESS_COUNT                       \
  (unsigned)VALGRIND_DO_CLIENT_REQUEST_EXPR(0,                  \
          VG_USERREQ__FG_SYSCALL_GET_SUCCESS_COUNT, 0, 0, 0, 0, 0)

/* Zero the fail/success heap memory allocation and syscall counts. */
#define FAILGRIND_ZERO_COUNTS                             \
  VALGRIND_DO_CLIENT_REQUEST_STMT(                              \
         VG_USERREQ__FG_ZERO_COUNTS, 0, 0, 0, 0, 0)

#endif /* __FAILGRIND_H */
