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

typedef struct _VgHashNode_callstack {
   struct _VgHashNode_callstack *next;
   UInt ecu;
   ExeContext *exe;
} VgHashNode_callstack;

//------------------------------------------------------------//
//--- Function declarations                                ---//
//------------------------------------------------------------//
static void add_callstack_from_file(Addr *ips, UInt n_ips);


/* Creates a callstack and ECU for the current context */
ExeContext *FG_(get_call_execontext)(ThreadId tid, UWord *ips, UInt *n_ips)
{
   Int i;

   for (i = 0; i < VG_(clo_backtrace_size); i++) {
      ips[i] = 0;
   }

   *n_ips = VG_(get_StackTrace)( tid, ips, VG_(clo_backtrace_size),
         NULL, NULL, 0);

   /* From coregrind/m_stacktrace.c */
   if ( ! VG_(clo_show_below_main) ) {
      DiEpoch ep = VG_(current_DiEpoch());
      // Search (from the outer frame onwards) the appearance of "main"
      // or the last appearance of a below main function.
      // Then decrease n_ips so as not to consider frames below main
      for (i = *n_ips - 1; i >= 0; i--) {
         Vg_FnNameKind kind = VG_(get_fnname_kind_from_IP)(ep, ips[i]);
         if (Vg_FnNameMain == kind || Vg_FnNameBelowMain == kind)
            *n_ips = i + 1;
         if (Vg_FnNameMain == kind)
            break;
      }
   }

   return VG_(make_ExeContext_from_StackTrace)(ips, *n_ips);
}


//------------------------------------------------------------//
//--- Hash table add/search/cleanup                        ---//
//------------------------------------------------------------//

static void add_callstack_from_file(Addr *ips, UInt n_ips)
{
   ExeContext *exe;
   UInt ecu;

   FG_(callstacks_loaded)++;
   exe = VG_(make_ExeContext_from_StackTrace)(ips, n_ips);
   ecu = VG_(get_ECU_from_ExeContext)(exe);

   if (!FG_(callstack_exists)(ecu)) {
      FG_(add_callstack_to_hashtable)(exe, ecu);
   }
}


/* Add the callstack ECU to the in memory hash table. */
void FG_(add_callstack_to_hashtable)(ExeContext *exe, UInt ecu)
{
   VgHashNode_callstack *node = VG_(calloc)("fg.add_callstack_to_hashtable", 1, sizeof(VgHashNode_callstack));
   node->ecu = (UWord)ecu;
   node->exe = exe;
   VG_(HT_add_node)(FG_(callstack_ht), node);
}


/* Check whether a ecu exists in the hash table. */
Bool FG_(callstack_exists)(UInt ecu)
{
   if (VG_(HT_lookup)(FG_(callstack_ht), (UWord)ecu)) {
      return True;
   } else {
      return False;
   }
}


/* Free all memory from hash table. */
void FG_(cleanup_callstacks)(void)
{
   VG_(HT_destruct)(FG_(callstack_ht), VG_(free));
   FG_(callstack_ht) = NULL;
}


//------------------------------------------------------------//
//--- Callstack file reading/writing                       ---//
//------------------------------------------------------------//

Bool FG_(read_callstack_file)(const HChar *filename, Bool no_open_is_error)
{
   UWord ips[VG_(clo_backtrace_size)];
   UInt n_ips = 0;
   Int fd;
   Int lineno;
   SizeT nBuf;
   const HChar *addr_str;
   const HChar *err_str;
   HChar *bufp;

   /* Starting point for size of bufp. This will be increased by VG_(get_line)
    * if required. */
   nBuf = 100;
   bufp = VG_(malloc)("fg.read_callstacks_file", nBuf);

   fd = VG_(fd_open)(filename, VKI_O_RDONLY, 0);
   if (fd < 0) {
      VG_(free)(bufp);

      return !no_open_is_error;
   }

   while (!VG_(get_line)(fd, &bufp, &nBuf, &lineno)) {
      if (n_ips == VG_(clo_backtrace_size)) {
         err_str = "   too many callers in stack trace, try increasing --num-callers";
         goto error;
      }

      if (!VG_(strncmp)(bufp, "at 0x", 5)) {
         /* Start of a new stack trace. If we're already part way through
          * reading a stack trace then this indicates the end of that trace
          * and the start of another. */
         if (n_ips > 0) {
            add_callstack_from_file(ips, n_ips);
            n_ips = 0;
         }
         addr_str = &bufp[3];
         VG_(parse_Addr)(&addr_str, &ips[n_ips]);
         n_ips++;
      } else if (!VG_(strncmp)(bufp, "by 0x", 5)) {
         if (n_ips == 0) {
            /* We haven't seen an "at 0x" yet, so this is a corrupt trace. */
            err_str = "   incomplete stack trace";
            goto error;
         }
         addr_str = &bufp[3];
         VG_(parse_Addr)(&addr_str, &ips[n_ips]);
         n_ips++;
      } else {
         err_str = "   corrupt line";
         goto error;
      }
   }
   /* Catch the final stack trace */
   if (n_ips > 0) {
      add_callstack_from_file(ips, n_ips);
   }

   VG_(close)(fd);
   VG_(free)(bufp);
   return True;

error:
   VG_(free)(bufp);
   VG_(close)(fd);

   VG_(umsg)("ERROR: in callstack file \"%s\" near line %d:\n",
         filename, lineno);
   VG_(umsg)("   %s\n", err_str);
   return False;
}


/* Initialise callstacks and attempt to load all callstacks from the callstack
 * file defined on the command line. */
void FG_(load_callstacks)(void)
{
   /* Must construct this even if we aren't loading the callstacks. */
   FG_(callstack_ht) = VG_(HT_construct)("fg.load_callstacks.1");

   if (FG_(clo_callstack_input) == NULL) {
      return;
   }

   if (!FG_(read_callstack_file)(FG_(clo_callstack_input), False)) {
      VG_(umsg)("exiting now.\n");
      VG_(exit)(1);
   }
}


static void write_callstack_line(UInt n, DiEpoch ep, Addr ip, void* uu_opaque)
{
   Int *fd;
   UInt flen = 0;
   HChar *fbuf = NULL;
   InlIPCursor *iipc = VG_(new_IIPC)(ep, ip);

   fd = (Int *)uu_opaque;

   do {
      const HChar *buf = VG_(describe_IP)(ep, ip, iipc);
      UInt len = VG_(strlen)(buf) + VG_(strlen)("at \n");

      if (len > flen) {
         fbuf = VG_(realloc)("fg.write_callstack_line", fbuf, len+1);
         flen = len;
      }

      VG_(snprintf)(fbuf, flen+1, "%s %s\n",
                   ( n == 0 ? "at" : "by" ), buf);

      VG_(write)(*fd, fbuf, flen);
      n++;
      // Increase n to show "at" for only one level.
   } while (VG_(next_IIPC)(iipc));
   VG_(free)(fbuf);
   VG_(delete_IIPC)(iipc);
}


Bool FG_(write_callstack_file)(const HChar *filename, Bool append)
{
   VgHashNode_callstack *node;
   Int fd;
   Int flags = VKI_O_CREAT | VKI_O_WRONLY;

   if (filename == NULL) return False;

   if (append) {
      flags |= VKI_O_APPEND;
   }

   fd = VG_(fd_open)(filename, VKI_O_CREAT | VKI_O_WRONLY, 0660);
   if (fd < 0) {
      VG_(gdb_printf)("Unable to open %s\n", filename);
      return False;
   }

   VG_(HT_ResetIter)(FG_(callstack_ht));
   while ((node = VG_(HT_Next)(FG_(callstack_ht)))) {
      VG_(write)(fd, FG_(execomment), FG_(execomment_len));
      VG_(apply_ExeContext)(write_callstack_line, &fd, node->exe);
      VG_(write)(fd, "\n", 1);
   }

   VG_(close)(fd);

   return True;
}


/* Write a single callstack to the callstack file, creating if necessary. */
void FG_(write_callstack)(UWord *ips, UInt n_ips)
{
   Int fd;

   if (FG_(clo_callstack_output) == NULL || FG_(clo_write_callstacks_at_end)) {
      return;
   }

   /* It is worth thinking about whether it is better to keep the callstack
    * file open for writing for the duration of the program, or to open and
    * close it every time a new callstack is written. The justification I am
    * using for opening and closing it every time is that from what I have seen
    * there are only typically a few failures (and corresponding file writes)
    * required before a program will exit, and that I like the simplicity of
    * having the entire writing code in one function.
    *
    * It would be trivial to change to opening the file in fg_post_clo_init()
    * and closing it in fg_fini().
    */
   fd = VG_(fd_open)(FG_(clo_callstack_output), VKI_O_CREAT | VKI_O_APPEND | VKI_O_WRONLY, 0660);
   if (fd < 0) {
      VG_(umsg)("Unable to open %s\n", FG_(clo_callstack_output));
      return;
   }

   VG_(write)(fd, FG_(execomment), FG_(execomment_len));
   VG_(apply_StackTrace)(write_callstack_line, &fd, VG_(current_DiEpoch)(), ips, n_ips);
   VG_(write)(fd, "\n", 1);

   VG_(close)(fd);
}



//--------------------------------------------------------------------//
//--- end                                           fg_callstack.c ---//
//--------------------------------------------------------------------//
