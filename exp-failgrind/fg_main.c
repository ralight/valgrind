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
#include "clg_global.h"

#define FG_CALLSTACK_FILE "failgrind.callstacks"

//------------------------------------------------------------//
//--- Globals                                              ---//
//------------------------------------------------------------//

// Alloc
ULong FG_(alloc_success_count) = 0;
ULong FG_(alloc_fail_count) = 0;
ULong FG_(alloc_new_callstack_count) = 0;
Bool FG_(alloc_fail) = True;
XArray* FG_(alloc_toggle_funcs) = NULL;
XArray* FG_(alloc_allow_funcs) = NULL;

// Syscall
ULong FG_(syscall_success_count) = 0;
ULong FG_(syscall_fail_count) = 0;
ULong FG_(syscall_new_callstack_count) = 0;
Bool FG_(syscall_fail) = False;
XArray* FG_(syscall_toggle_funcs) = NULL;
XArray* FG_(syscall_allow_funcs) = NULL;


// Other
static UInt g_seed;
HChar *FG_(execomment) = NULL;
UInt FG_(execomment_len) = 0;

VgHashTable *FG_(callstack_ht) = NULL;

ULong FG_(callstacks_loaded) = 0;

Bool FG_(in_main) = False;

//------------------------------------------------------------//
//--- Command line options                                 ---//
//------------------------------------------------------------//

// Alloc
static Bool clo_alloc_fail_atstart = True;
UInt FG_(clo_alloc_fail_chance) = 100;
UInt FG_(clo_alloc_max_fails) = 0;
Long FG_(clo_alloc_thresh_high) = 0;
Long FG_(clo_alloc_thresh_low) = 0;
Bool FG_(clo_alloc_thresh_invert) = False;

// Syscall
static Bool clo_syscall_fail_atstart = False;
UInt FG_(clo_syscall_fail_chance) = 100;
UInt FG_(clo_syscall_max_fails) = 0;
Bool FG_(clo_syscall_specified_only) = False;

// Other
const HChar *FG_(clo_callstack_input) = FG_CALLSTACK_FILE;
const HChar *FG_(clo_callstack_output) = FG_CALLSTACK_FILE;
Bool FG_(clo_write_callstacks_at_end) = False;
Bool FG_(clo_print_failed_traces) = False;

static Bool clo_print_stats = True;
static UInt clo_seed = 0;


//------------------------------------------------------------//
//--- Random number 0-99                                  ---//
//------------------------------------------------------------//

UInt FG_(random_100)(void)
{
   UInt r;

   do {
      r = VG_(random)(&g_seed);
   } while (r > (4294967295U - (((4294967295U % 100) + 1) % 100)));

   return r % 100;

}


//------------------------------------------------------------//
//--- Client events                                        ---//
//------------------------------------------------------------//

static void print_monitor_help ( void )
{
   VG_(gdb_printf) ("\n");
   VG_(gdb_printf) ("failgrind monitor commands:\n");
   VG_(gdb_printf) ("  alloc_fail [on|off]\n");
   VG_(gdb_printf) ("        get/set (if on/off given) whether memory allocation failures are enabled.\n)");
   VG_(gdb_printf) ("  callstack_append <file>\n");
   VG_(gdb_printf) ("        appends the in-memory list of heap allocation callstacks to an existing file.\n");
   VG_(gdb_printf) ("  callstack_clear\n");
   VG_(gdb_printf) ("        clear the in-memory list of heap allocation callstacks.\n");
   VG_(gdb_printf) ("  callstack_read <file>\n");
   VG_(gdb_printf) ("        read a set of callstacks from a file into memory. Does not remove existing\n");
   VG_(gdb_printf) ("        callstacks in memory.\n");
   VG_(gdb_printf) ("  callstack_write <file>\n");
   VG_(gdb_printf) ("        write the in-memory list of callstacks to a new file.\n");
   VG_(gdb_printf) ("  print_stats\n");
   VG_(gdb_printf) ("        print the success/fail counts, then zeros them.\n");
   VG_(gdb_printf) ("  syscall_fail [on|off]\n");
   VG_(gdb_printf) ("        get/set (if on/off given) whether global syscall failures are enabled.\n)");
   VG_(gdb_printf) ("  zero_stats\n");
   VG_(gdb_printf) ("        zero the success/fail counts.\n");
   VG_(gdb_printf) ("\n");
}


/* return True if request recognised, False otherwise */
static Bool handle_gdb_monitor_command (ThreadId tid, const HChar *req)
{
   HChar* wcmd;
   HChar s[VG_(strlen)(req) + 1]; /* copy for strtok_r */
   HChar *ssaveptr;

   VG_(strcpy) (s, req);

   wcmd = VG_(strtok_r) (s, " ", &ssaveptr);
   switch (VG_(keyword_id) ("help "
                            "alloc_fail "
                            "callstack_append callstack_clear callstack_read callstack_write "
                            "print_stats zero_stats "
                            "syscall_fail",
                            wcmd, kwd_report_duplicated_matches)) {

   case -2: /* multiple matches */
      return True;
   case -1: /* not found */
      return False;
   case  0: /* help */
      print_monitor_help();
      return True;
   case  1: { /* alloc_fail */
      HChar* arg = VG_(strtok_r) (0, " ", &ssaveptr);
      if (arg == NULL) {
         VG_(gdb_printf)("alloc_fail: %s\n",
         FG_(alloc_fail) ? "on":"off");
      } else {
         FG_(alloc_fail) = VG_(strcmp)(arg, "off")!=0;
      }
      return True;
   }
   case  2: { /* callstack_append */
      HChar* arg = VG_(strtok_r) (0, " ", &ssaveptr);
      if (arg == NULL) {
         return False;
      } else {
         return FG_(write_callstack_file)(arg, True);
      }
   }
   case  3: { /* callstack_clear */
      FG_(cleanup_callstacks)();
      FG_(callstack_ht) = VG_(HT_construct)("fg.handle_gdb_monitor_command");
      return True;
   }
   case  4: { /* callstack_read */
      HChar* arg = VG_(strtok_r) (0, " ", &ssaveptr);
      if (arg == NULL) {
         return False;
      } else {
         return FG_(read_callstack_file)(arg, True);
      }
   }
   case  5: { /* callstack_write */
      HChar* arg = VG_(strtok_r) (0, " ", &ssaveptr);
      if (arg == NULL) {
         return False;
      } else {
         return FG_(write_callstack_file)(arg, False);
      }
   }
   case  6: { /* print_stats */
      VG_(gdb_printf)("%llu allocations succeeded\n", FG_(alloc_success_count));
      VG_(gdb_printf)("%llu allocations failed\n", FG_(alloc_fail_count));
      VG_(gdb_printf)("%llu new allocation callstacks found\n", FG_(alloc_new_callstack_count));
      VG_(gdb_printf)("%llu syscalls succeeded\n", FG_(syscall_success_count));
      VG_(gdb_printf)("%llu syscalls failed\n", FG_(syscall_fail_count));
      VG_(gdb_printf)("%llu new syscall callstacks found\n", FG_(syscall_new_callstack_count));
      /* Fall through to zero_stats */
   }
   case  7: { /* zero_stats */
      FG_(alloc_success_count) = 0;
      FG_(alloc_fail_count) = 0;
      FG_(alloc_new_callstack_count) = 0;
      FG_(syscall_success_count) = 0;
      FG_(syscall_fail_count) = 0;
      FG_(syscall_new_callstack_count) = 0;
      return True;
   }
   case  8: { /* syscall_fail */
      HChar* arg = VG_(strtok_r) (0, " ", &ssaveptr);
      if (arg == NULL) {
         VG_(gdb_printf)("syscall_fail: %s\n",
         FG_(syscall_fail) ? "on":"off");
      } else {
         FG_(syscall_fail) = VG_(strcmp)(arg, "off")!=0;
      }
      return True;
   }

   default:
      tl_assert(0);
      return False;
   }
}

static Bool handle_client_request(ThreadId tid, UWord *args, UWord *ret)
{
   if (!VG_IS_TOOL_USERREQ('F', 'G', args[0])
       && VG_USERREQ__GDB_MONITOR_COMMAND != args[0])
      return False;

   switch(args[0]) {
   case VG_USERREQ__FG_ALLOC_FAIL_ON:
      FG_(alloc_fail) = True;
      return True;
   case VG_USERREQ__FG_ALLOC_FAIL_OFF:
      FG_(alloc_fail) = False;
      return True;
   case VG_USERREQ__FG_ALLOC_FAIL_TOGGLE:
      FG_(alloc_fail) = !FG_(alloc_fail);
      return True;
   case VG_USERREQ__FG_ALLOC_GET_FAIL_COUNT: {
      *ret = FG_(alloc_fail_count);
      return True;
   }
   case VG_USERREQ__FG_ALLOC_GET_NEW_CALLSTACK_COUNT: {
      *ret = FG_(alloc_new_callstack_count);
      return True;
   }
   case VG_USERREQ__FG_ALLOC_GET_SUCCESS_COUNT: {
      *ret = FG_(alloc_success_count);
      return True;
   }
   case VG_USERREQ__FG_CALLSTACK_CLEAR: {
      FG_(cleanup_callstacks)();
      FG_(callstack_ht) = VG_(HT_construct)("fg.handle_client_request");
      return True;
   }
   case VG_USERREQ__FG_CALLSTACK_READ: {
      return FG_(read_callstack_file)((HChar *)args[1], True);
   }
   case VG_USERREQ__FG_CALLSTACK_WRITE: {
      return FG_(write_callstack_file)((HChar *)args[1], False);
   }
   case VG_USERREQ__FG_CALLSTACK_APPEND: {
      return FG_(write_callstack_file)((HChar *)args[1], True);
   }
   case VG_USERREQ__FG_SYSCALL_FAIL_ON:
      FG_(syscall_fail) = True;
      return True;
   case VG_USERREQ__FG_SYSCALL_FAIL_OFF:
      FG_(syscall_fail) = False;
      return True;
   case VG_USERREQ__FG_SYSCALL_FAIL_TOGGLE:
      FG_(syscall_fail) = !FG_(syscall_fail);
      return True;
   case VG_USERREQ__FG_SYSCALL_GET_FAIL_COUNT: {
      *ret = FG_(syscall_fail_count);
      return True;
   }
   case VG_USERREQ__FG_SYSCALL_GET_NEW_CALLSTACK_COUNT: {
      *ret = FG_(syscall_new_callstack_count);
      return True;
   }
   case VG_USERREQ__FG_SYSCALL_GET_SUCCESS_COUNT: {
      *ret = FG_(syscall_success_count);
      return True;
   }
   case VG_USERREQ__FG_ZERO_COUNTS: {
      FG_(alloc_success_count) = 0;
      FG_(alloc_fail_count) = 0;
      FG_(alloc_new_callstack_count) = 0;
      FG_(syscall_success_count) = 0;
      FG_(syscall_fail_count) = 0;
      FG_(syscall_new_callstack_count) = 0;
      return True;
   }

   case VG_USERREQ__GDB_MONITOR_COMMAND: {
      Bool handled = handle_gdb_monitor_command (tid, (HChar*)args[1]);
      if (handled)
         *ret = 1;
      else
         *ret = 0;
      return handled;
   }
   default:
      return False;
   }

   return True;
}


//------------------------------------------------------------//
//--- Command line args                                    ---//
//------------------------------------------------------------//

static Bool fg_process_cmd_line_option(const HChar* arg)
{
   const HChar* optval;

   if VG_STR_CLO(arg, "--alloc-allow", optval) {
      if (!FG_(alloc_allow_funcs)) {
         FG_(alloc_allow_funcs) = VG_(newXA)(VG_(malloc), "fg.process_cmd_line_option.5",
                                             VG_(free), sizeof(HChar*));
      }
      VG_(addToXA)(FG_(alloc_allow_funcs), &optval);
   }
   else if VG_BOOL_CLO(arg, "--alloc-fail-atstart", clo_alloc_fail_atstart) {}
   else if VG_BINT_CLO(arg, "--alloc-fail-chance", FG_(clo_alloc_fail_chance), 1, 100) {}
   else if VG_BINT_CLO(arg, "--alloc-max-fails", FG_(clo_alloc_max_fails), 0, 1000000) {}
   else if VG_INT_CLO(arg, "--alloc-threshold-high", FG_(clo_alloc_thresh_high)) {
      if (FG_(clo_alloc_thresh_high) < 0) {
         VG_(fmsg_bad_option)(arg,
            "--alloc-threshold-high must be greater than 0\n");
      }
   }
   else if VG_INT_CLO(arg, "--alloc-threshold-low", FG_(clo_alloc_thresh_low)) {
      if (FG_(clo_alloc_thresh_low) < 0) {
         VG_(fmsg_bad_option)(arg,
            "--alloc-threshold-low must be greater than 0\n");
      }
   }
   else if VG_BOOL_CLO(arg, "--alloc-threshold-invert", FG_(clo_alloc_thresh_invert)) {}
   else if VG_STR_CLO(arg, "--alloc-toggle", optval) {
      if (!FG_(alloc_toggle_funcs)) {
         FG_(alloc_toggle_funcs) = VG_(newXA)(VG_(malloc), "fg.process_cmd_line_option.1",
                                           VG_(free), sizeof(HChar*));
      }
      VG_(addToXA)(FG_(alloc_toggle_funcs), &optval);
      /* defaults to failing disabled initially */
      clo_alloc_fail_atstart = False;
   }
   else if VG_BOOL_CLO(arg, "--failgrind-stats", clo_print_stats) {}
   else if VG_STR_CLO(arg, "--callstack-input", FG_(clo_callstack_input)) {}
   else if VG_STR_CLO(arg, "--callstack-output", FG_(clo_callstack_output)) {}
   else if VG_INT_CLO(arg, "--seed", clo_seed) {}
   else if VG_BOOL_CLO(arg, "--show-failed", FG_(clo_print_failed_traces)) {}
   else if VG_STR_CLO(arg, "--syscall-allow", optval) {
      if (!FG_(syscall_allow_funcs)) {
         FG_(syscall_allow_funcs) = VG_(newXA)(VG_(malloc), "fg.process_cmd_line_option.4",
                                             VG_(free), sizeof(HChar*));
      }
      VG_(addToXA)(FG_(syscall_allow_funcs), &optval);
   }
   else if VG_STR_CLO(arg, "--syscall-errno", optval){
      FG_(parse_syscall_errno)(arg, optval);
   }
   else if VG_BOOL_CLO(arg, "--syscall-fail-atstart", clo_syscall_fail_atstart) {}
   else if VG_BINT_CLO(arg, "--syscall-fail-chance", FG_(clo_syscall_fail_chance), 1, 100) {}
   else if VG_BINT_CLO(arg, "--syscall-max-fails", FG_(clo_syscall_max_fails), 0, 1000000) {}
   else if VG_BOOL_CLO(arg, "--syscall-specified-only", FG_(clo_syscall_specified_only)) {}
   else if VG_STR_CLO(arg, "--syscall-toggle", optval) {
      if (!FG_(syscall_toggle_funcs)) {
         FG_(syscall_toggle_funcs) = VG_(newXA)(VG_(malloc), "fg.process_cmd_line_option.3",
                                             VG_(free), sizeof(HChar*));
      }
      VG_(addToXA)(FG_(syscall_toggle_funcs), &optval);
      /* defaults to failing disabled initially */
      clo_syscall_fail_atstart = False;
   }
   else if VG_BOOL_CLO(arg, "--write-callstacks-at-end", FG_(clo_write_callstacks_at_end)) {}
   else
      return VG_(replacement_malloc_process_cmd_line_option)(arg);

   return True;
}


static void fg_print_usage(void)
{
   VG_(printf)(
"    --alloc-fail-atstart=no|yes  Begin memory allocation failures at failgrind\n"
"                              start [yes]\n"
"    --alloc-fail-chance=<number>  The default operation of failgrind is to reject\n"
"                              all memory allocations when a call stack is seen\n"
"                              for the first time. Set this option to 1-100 to\n"
"                              act as the percentage chance that an allocation\n"
"                              that is due to be rejected will actually be\n"
"                              rejected. Using this option makes your testing\n"
"                              non-deterministic [100]\n"
"    --alloc-max-fails=<number>  Maximum number of allocation failures allowed\n"
"                              per run [0: unlimited]\n"
"    --alloc-threshold-high=<bytes>  If set, allocations larger than bytes will\n"
"                              never be failed.\n"
"    --alloc-threshold-low=<bytes>   If set, allocations smaller than bytes will\n"
"                              never be failed.\n"
"    --alloc-threshold-invert=yes|no  Invert the sense of --alloc-threshold-high\n"
"                              and --alloc-threshold-low, i.e. only reject\n"
"                              allocations smaller than \"high\" and larger than\n"
"                              \"low\" [no].\n"
"    --alloc-toggle=<function> Toggle the enable/disable state of heap memory\n"
"                              allocation failures when this function is entered\n"
"                              and returned from. When this option is in use the\n"
"                              --alloc-fail-atstart option defaults to no,\n"
"                              meaning that allocation failures will be enabled\n"
"                              when <function> is entered.\n"
"    --callstack-input=no|<filename>  Load callstacks that should always succeed\n"
"                              from this file. Defaults to %s.\n"
"                              Set to 'no' to disable the loading of callstacks.\n"
"    --callstack-output=no|<filename>  Relevant call stacks that failgrind fails\n"
"                              for the first time will be written to this file.\n"
"                              Defaults to %s.\n"
"                              Set to 'no' to disable the writing of callstacks.\n"
"    --failgrind-stats=yes|no  Show simple allocation success/failure counts\n"
"                              after the program ends [yes]\n"
"    --seed=<number>           Random seed to use when --alloc-fail-chance or\n"
"                              --syscall-fail-chance is in use\n"
"    --show-failed=yes|no      Print a stack trace whenever an allocation or syscall\n"
"                              is made to fail [no]\n"
"    --syscall-allow=<function>   Always allow the named syscall to succeed,\n"
"                              regardless of other settings.\n"
"    --syscall-errno=<error>   Set syscall failures to produce <error> instead of \n"
"                              EINVAL. See the user manual for a list of supported\n"
"                              errors.\n"
"    --syscall-errno=<function>,<error>  Set the error to use for a specific syscall\n"
"                              only, e.g. \"open,EPERM\". Can be used multiple times,\n"
"                              but only once per syscall.\n"
"    --syscall-fail-atstart=no|yes  Begin global syscall failures at failgrind\n"
"                              start [no]\n"
"    --syscall-fail-chance=<number>  The default operation of failgrind is to\n"
"                              reject all syscalls when a call stack is seen\n"
"                              for the first time. Set this option to 1-100 to\n"
"                              act as the percentage chance that a syscall\n"
"                              that is due to be rejected will actually be\n"
"                              rejected. Using this option makes your testing\n"
"                              non-deterministic [100]\n"
"    --syscall-max-fails=<number>  Maximum number of syscall failures allowed\n"
"                              per run [0: unlimited]\n"
"    --syscall-specified-only=yes|no  When specific functions have been defined with\n"
"                              --syscall-errno, set this option to yes to restrict\n"
"                              failures to the specified syscalls only. Default: no,\n"
"    --syscall-toggle=<function> Toggle the enable/disable state of syscall\n"
"                              failures when this function is entered and\n"
"                              returned from. When this option is in use the\n"
"                              --syscall-fail-atstart option defaults to no,\n"
"                              meaning that syscall failures will be enabled\n"
"                              when <function> is entered.\n"
"    --write-callstacks-at-end=yes|no  Set to yes to write the callstack output\n"
"                              file when Failgrind exits. This creates a new file.\n"
"                              Set to no to have each new callstack appended to\n"
"                              the output file immediately [no].\n"
   , FG_CALLSTACK_FILE, FG_CALLSTACK_FILE);
}

static void fg_print_debug_usage(void)
{
   VG_(printf)(
"    (none)\n"
   );
}


static void fg_fini(Int exit_status)
{
   if (FG_(clo_write_callstacks_at_end)) {
      FG_(write_callstack_file)(FG_(clo_callstack_output), False);
   }

   FG_(cleanup_callstacks)();
   VG_(free)(FG_(execomment));

   if (clo_print_stats && VG_(clo_verbosity) > 0) {
      VG_(umsg)(" Failgrind: %llu call stacks loaded from file\n", FG_(callstacks_loaded));
      VG_(umsg)("            %llu allocations succeeded\n", FG_(alloc_success_count));
      VG_(umsg)("            %llu allocations failed\n", FG_(alloc_fail_count));
      VG_(umsg)("            %llu new allocation callstacks found\n", FG_(alloc_new_callstack_count));
      VG_(umsg)("            %llu syscalls succeeded\n", FG_(syscall_success_count));
      VG_(umsg)("            %llu syscalls failed\n", FG_(syscall_fail_count));
      VG_(umsg)("            %llu new syscall callstacks found\n", FG_(syscall_new_callstack_count));
      VG_(umsg)("\n");
   }
}


//------------------------------------------------------------//
//--- Initialisation                                       ---//
//------------------------------------------------------------//

static void fg_post_clo_init(void)
{
   const HChar *exename;

   /* Input/output files */
   if (FG_(clo_callstack_input) != NULL) {
      if (!VG_(strcmp)(FG_(clo_callstack_input), "no")) {
         FG_(clo_callstack_input) = NULL;
      /*
      } else {
         FG_(clo_callstack_input) is custom file.
      */
      }
   } else {
      FG_(clo_callstack_input) = FG_CALLSTACK_FILE;
   }

   if (FG_(clo_callstack_output) != NULL) {
      if (!VG_(strcmp)(FG_(clo_callstack_output), "no")) {
         FG_(clo_callstack_output) = NULL;
      /*
      } else {
         FG_(clo_callstack_output) is custom file.
      */
      }
   } else {
      FG_(clo_callstack_output) = FG_CALLSTACK_FILE;
   }

   FG_(load_callstacks)();

   /* Prepare exename comment line for writing. '# ./test\n' */
   exename = VG_(args_the_exename);
   FG_(execomment_len) = VG_(strlen)(exename) + 3;
   FG_(execomment) = VG_(malloc)("fg.post_clo_init.1", FG_(execomment_len) + 1);
   VG_(sprintf)(FG_(execomment), "# %s\n", exename);

   /* Random seed generation and reporting */
   if (FG_(clo_alloc_fail_chance) < 100 || FG_(clo_syscall_fail_chance) < 100) {
      if (clo_seed == 0) {
         g_seed = (VG_(getpid)() << 9) ^ VG_(getppid)();
      } else {
         g_seed = clo_seed;
      }

      if (VG_(clo_verbosity) > 0) {
         VG_(umsg)("Using random seed: %u\n", g_seed);
      }
   }

   FG_(alloc_fail) = clo_alloc_fail_atstart;
   FG_(syscall_fail) = clo_syscall_fail_atstart;

   CLG_(post_clo_init)();
}

static void fg_pre_clo_init(void)
{
   VG_(details_name)            ("Failgrind");
   VG_(details_version)         (NULL);
   VG_(details_description)     ("a memory allocation and syscall failure testing tool");
   VG_(details_copyright_author)(
      "Copyright (C) 2018-2021, and GNU GPL'd, by Roger Light.");
   VG_(details_bug_reports_to)  (VG_BUGS_TO);

   // Basic functions.
   VG_(basic_tool_funcs)          (fg_post_clo_init,
                                   CLG_(instrument),
                                   fg_fini);
   // Needs.
   VG_(needs_libc_freeres)();
   VG_(needs_cxx_freeres)();
   VG_(needs_command_line_options)(fg_process_cmd_line_option,
                                   fg_print_usage,
                                   fg_print_debug_usage);
   VG_(needs_malloc_replacement)  (FG_(malloc),
                                   FG_(__builtin_new),
                                   FG_(__builtin_new_aligned),
                                   FG_(__builtin_vec_new),
                                   FG_(__builtin_vec_new_aligned),
                                   FG_(memalign),
                                   FG_(calloc),
                                   FG_(free),
                                   FG_(__builtin_delete),
                                   FG_(__builtin_delete_aligned),
                                   FG_(__builtin_vec_delete),
                                   FG_(__builtin_vec_delete_aligned),
                                   FG_(realloc),
                                   FG_(malloc_usable_size),
                                   0 );

   VG_(needs_client_requests)(handle_client_request);

   VG_(needs_syscall_wrapper)(FG_(pre_syscall), FG_(post_syscall));

   /* Arbitrary increase of --num-callers from default of 12. */
   VG_(clo_backtrace_size) = 30;

   FG_(syscall_allow_funcs) = VG_(newXA)(VG_(malloc), "fg.pre_clo_init.1",
                                         VG_(free), sizeof(HChar*));

   /* These syscalls should always succeed. */
   HChar* s;
   s = VG_(strdup)("fg.pre_clo_init.addToXA", "alarm");
   VG_(addToXA)(FG_(syscall_allow_funcs), &s);
   s = VG_(strdup)("fg.pre_clo_init.addToXA", "exit");
   VG_(addToXA)(FG_(syscall_allow_funcs), &s);
   s = VG_(strdup)("fg.pre_clo_init.addToXA", "exit_group");
   VG_(addToXA)(FG_(syscall_allow_funcs), &s);
   s = VG_(strdup)("fg.pre_clo_init.addToXA", "getegid");
   VG_(addToXA)(FG_(syscall_allow_funcs), &s);
   s = VG_(strdup)("fg.pre_clo_init.addToXA", "geteuid");
   VG_(addToXA)(FG_(syscall_allow_funcs), &s);
   s = VG_(strdup)("fg.pre_clo_init.addToXA", "getgid");
   VG_(addToXA)(FG_(syscall_allow_funcs), &s);
   s = VG_(strdup)("fg.pre_clo_init.addToXA", "getpgrp");
   VG_(addToXA)(FG_(syscall_allow_funcs), &s);
   s = VG_(strdup)("fg.pre_clo_init.addToXA", "getpid");
   VG_(addToXA)(FG_(syscall_allow_funcs), &s);
   s = VG_(strdup)("fg.pre_clo_init.addToXA", "getppid");
   VG_(addToXA)(FG_(syscall_allow_funcs), &s);
   s = VG_(strdup)("fg.pre_clo_init.addToXA", "gettid");
   VG_(addToXA)(FG_(syscall_allow_funcs), &s);
   s = VG_(strdup)("fg.pre_clo_init.addToXA", "getuid");
   VG_(addToXA)(FG_(syscall_allow_funcs), &s);
   s = VG_(strdup)("fg.pre_clo_init.addToXA", "sched_yield");
   VG_(addToXA)(FG_(syscall_allow_funcs), &s);
   s = VG_(strdup)("fg.pre_clo_init.addToXA", "set_tid_address");
   VG_(addToXA)(FG_(syscall_allow_funcs), &s);
   s = VG_(strdup)("fg.pre_clo_init.addToXA", "sync");
   VG_(addToXA)(FG_(syscall_allow_funcs), &s);
   s = VG_(strdup)("fg.pre_clo_init.addToXA", "umask");
   VG_(addToXA)(FG_(syscall_allow_funcs), &s);
}

VG_DETERMINE_INTERFACE_VERSION(fg_pre_clo_init)

//--------------------------------------------------------------------//
//--- end                                                fg_main.c ---//
//--------------------------------------------------------------------//
