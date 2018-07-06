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
#include "pub_tool_hashtable.h"
#include "pub_tool_libcbase.h"
#include "pub_tool_libcprint.h"
#include "pub_tool_mallocfree.h"
#include "pub_tool_stacktrace.h"
#include "pub_tool_vki.h"

#include "fg_global.h"

typedef struct _VgHashNode_errno {
   struct _VgHashNode_errno *next;
   UWord key;
   HChar* fnname;
   Int errno;
} VgHashNode_errno;

//------------------------------------------------------------//
//--- Globals                                              ---//
//------------------------------------------------------------//

static VgHashTable *g_syscall_errno_values = NULL;
static Int g_syscall_errno = VKI_EINVAL;


//------------------------------------------------------------//
//--- Function declarations                                ---//
//------------------------------------------------------------//

Int fg_str2errno(const HChar* str);


//------------------------------------------------------------//
//--- Functions                                            ---//
//------------------------------------------------------------//

static Word fg_cmp_errno(const void* node1, const void* node2)
{
   const VgHashNode_errno *n1 = node1;
   const VgHashNode_errno *n2 = node2;

   return VG_(strcmp)(n1->fnname, n2->fnname);
}


static Int get_fn_errno(const HChar* fnname)
{
   if (g_syscall_errno_values != NULL) {
      VgHashNode_errno* found_node;
      VgHashNode_errno node;

      node.next = NULL;
      node.key = fnname[0];
      node.fnname = fnname;
      node.errno = VKI_EINVAL;
      found_node = VG_(HT_gen_lookup)(g_syscall_errno_values, &node, fg_cmp_errno);

      if (found_node) {
         return found_node->errno;
      } else {
         if (FG_(clo_syscall_specified_only) == True) {
            /* The user has specified errno for particular functions,
             * and has --syscall-specified-only=yes, so we should not
             * fail the syscall because this one wasn't specified.
             */
            return -1;
         } else {
            /* --syscall-specified-only=no, so use the global errno
             * return value. */
            return g_syscall_errno;
         }
      }
   } else {
      /* No specific syscalls configured, so use global errno return value. */
      return g_syscall_errno;
   }
}

static Bool should_syscall_fail(ThreadId tid, UWord *ips, UInt *n_ips, Int *force_errno)
{
   ExeContext* exe;
   UInt ecu;
   const HChar* fnname;

   /* Check for global disable */
   if (!FG_(syscall_fail)) {
      return False;
   }

   *force_errno = -1;

   exe = FG_(get_call_execontext)(tid, ips, n_ips);
   ecu = VG_(get_ECU_from_ExeContext)(exe);

   VG_(get_fnname)(VG_(current_DiEpoch)(), ips[0], &fnname);
   if (FG_(syscall_allow_funcs) != NULL
         && VG_(strIsMemberXA)(FG_(syscall_allow_funcs), fnname)) {

      /* White list */
      return False;
   }

   if (FG_(callstack_exists)(ecu)) {
      return False;
   } else {
      FG_(syscall_new_callstack_count)++;

      /* This test must come after the callstack_exists() check, otherwise
       * FG_(syscall_new_callstack_count) will be wrong. */
      if (FG_(clo_syscall_max_fails) > 0 && FG_(syscall_fail_count) >= FG_(clo_syscall_max_fails)) {
         return False;
      }

      if (FG_(random_100)() < FG_(clo_syscall_fail_chance)) {
         /* We are now going to fail the syscall - so find out what errno to use */
         *force_errno = get_fn_errno(fnname);
         if (*force_errno == -1) {
            /* --syscall-specified-only=yes and fn not specified, so don't fail */
            return False;
         }

         FG_(add_callstack_to_hashtable)(exe, ecu);
         FG_(write_callstack)(ips, *n_ips);
         return True;
      } else {
         return False;
      }
   }
}


//------------------------------------------------------------//
//--- Syscalls                                             ---//
//------------------------------------------------------------//

void FG_(pre_syscall)(ThreadId tid, SyscallStatus* status, UInt syscallno,
                           UWord* args, UInt nArgs)
{
   UWord ips[VG_(clo_backtrace_size)];
   UInt n_ips;
   Int force_errno;

   if (FG_(in_main)) {
      if (should_syscall_fail(tid, ips, &n_ips, &force_errno)) {
         if (FG_(clo_print_failed_traces)) {
            VG_(pp_StackTrace)(VG_(current_DiEpoch)(), ips, n_ips);
            VG_(umsg)("\n");
         }

         VG_(force_syscall_error)(status, force_errno);
         FG_(syscall_fail_count)++;
      } else {
         FG_(syscall_success_count)++;
      }
   }
}

void FG_(post_syscall)(ThreadId tid, UInt syscallno,
                            UWord* args, UInt nArgs, SysRes res)
{
}


//------------------------------------------------------------//
//--- CLO parsing                                          ---//
//------------------------------------------------------------//

void FG_(parse_syscall_errno)(const HChar* arg, const HChar* value)
{
   Int tmp_errno;

   tmp_errno = fg_str2errno(value);
   if (tmp_errno != -1) {
      /* String matches against just an error, so set the global return value */
      g_syscall_errno = tmp_errno;
   } else {
      /* Doesn't match a simple error, try to see if it matches <syscall>,<error> instead */
      if (VG_(strchr)(value, ',') == NULL) {
         VG_(fmsg_bad_option)(arg,
            "Invalid --syscall-errno argument. Use either \"<syscall>,<error>\" or \"<error>\"\n");
      }

      if (value[0] == ',') {
         VG_(fmsg_bad_option)(arg,
            "Invalid --syscall-errno argument. Empty function name.\n");
      }

      HChar* tmp_str = VG_(strdup)("fg.syscall-errno", value);
      HChar* func;
      HChar *err_str;

      func = VG_(strtok)(tmp_str, ",");
      err_str = VG_(strtok)(NULL, ",");

      if (err_str && VG_(strlen)(err_str)) {
         tmp_errno = fg_str2errno(err_str);
      }
      if (tmp_errno == -1) {
         VG_(free)(tmp_str);
         VG_(fmsg_bad_option)(arg,
            "Invalid --syscall-errno argument. Unrecognised error value.\n");

      } else {
         if (g_syscall_errno_values == NULL) {
            g_syscall_errno_values = VG_(HT_construct)("fg.syscall.errno.values.ht");
         }

         VgHashNode_errno *node = VG_(malloc)("fg.syscall-errno-values.node", sizeof(VgHashNode_errno));
         VgHashNode_errno *found_node;
         node->next = NULL;
         node->key = func[0];
         node->fnname = VG_(strdup)("fg.syscall-errno-values.func", func);
         node->errno = tmp_errno;

         found_node = VG_(HT_gen_lookup)(g_syscall_errno_values, node, fg_cmp_errno);
         if (found_node != NULL) {
            VG_(fmsg_bad_option)(arg, "Duplicate syscall specified in --syscall-errno\n");
         }

         VG_(HT_add_node)(g_syscall_errno_values, node);
      }
      VG_(free)(tmp_str);
   }
}


//------------------------------------------------------------//
//--- Platform specific string to errno conversion         ---//
//------------------------------------------------------------//

#if defined(VGO_linux)
Int fg_str2errno(const HChar* str)
{
   if(!VG_(strcasecmp)(str, "EPERM"))            return VKI_EPERM;
   else if(!VG_(strcasecmp)(str, "ENOENT"))      return VKI_ENOENT;
   else if(!VG_(strcasecmp)(str, "ESRCH"))       return VKI_ESRCH;
   else if(!VG_(strcasecmp)(str, "EINTR"))       return VKI_EINTR;
   else if(!VG_(strcasecmp)(str, "EIO"))         return VKI_EIO;
   else if(!VG_(strcasecmp)(str, "ENXIO"))       return VKI_ENXIO;
   else if(!VG_(strcasecmp)(str, "E2BIG"))       return VKI_E2BIG;
   else if(!VG_(strcasecmp)(str, "ENOEXEC"))     return VKI_ENOEXEC;
   else if(!VG_(strcasecmp)(str, "EBADF"))       return VKI_EBADF;
   else if(!VG_(strcasecmp)(str, "ECHILD"))      return VKI_ECHILD;
   else if(!VG_(strcasecmp)(str, "EAGAIN"))      return VKI_EAGAIN;
   else if(!VG_(strcasecmp)(str, "ENOMEM"))      return VKI_ENOMEM;
   else if(!VG_(strcasecmp)(str, "EACCES"))      return VKI_EACCES;
   else if(!VG_(strcasecmp)(str, "EFAULT"))      return VKI_EFAULT;
   else if(!VG_(strcasecmp)(str, "ENOTBLK"))     return VKI_ENOTBLK;
   else if(!VG_(strcasecmp)(str, "EBUSY"))       return VKI_EBUSY;
   else if(!VG_(strcasecmp)(str, "EEXIST"))      return VKI_EEXIST;
   else if(!VG_(strcasecmp)(str, "EXDEV"))       return VKI_EXDEV;
   else if(!VG_(strcasecmp)(str, "ENODEV"))      return VKI_ENODEV;
   else if(!VG_(strcasecmp)(str, "ENOTDIR"))     return VKI_ENOTDIR;
   else if(!VG_(strcasecmp)(str, "EISDIR"))      return VKI_EISDIR;
   else if(!VG_(strcasecmp)(str, "EINVAL"))      return VKI_EINVAL;
   else if(!VG_(strcasecmp)(str, "ENFILE"))      return VKI_ENFILE;
   else if(!VG_(strcasecmp)(str, "EMFILE"))      return VKI_EMFILE;
   else if(!VG_(strcasecmp)(str, "ENOTTY"))      return VKI_ENOTTY;
   else if(!VG_(strcasecmp)(str, "ETXTBSY"))     return VKI_ETXTBSY;
   else if(!VG_(strcasecmp)(str, "EFBIG"))       return VKI_EFBIG;
   else if(!VG_(strcasecmp)(str, "ENOSPC"))      return VKI_ENOSPC;
   else if(!VG_(strcasecmp)(str, "ESPIPE"))      return VKI_ESPIPE;
   else if(!VG_(strcasecmp)(str, "EROFS"))       return VKI_EROFS;
   else if(!VG_(strcasecmp)(str, "EMLINK"))      return VKI_EMLINK;
   else if(!VG_(strcasecmp)(str, "EPIPE"))       return VKI_EPIPE;
   else if(!VG_(strcasecmp)(str, "EDOM"))        return VKI_EDOM;
   else if(!VG_(strcasecmp)(str, "ERANGE"))      return VKI_ERANGE;
   else if(!VG_(strcasecmp)(str, "EWOULDBLOCK")) return VKI_EWOULDBLOCK;
   else if(!VG_(strcasecmp)(str, "ENOSYS"))      return VKI_ENOSYS;
   else if(!VG_(strcasecmp)(str, "EOVERFLOW"))   return VKI_EOVERFLOW;
   else                                          return -1;
}

#elif defined(VGO_darwin)
Int fg_str2errno(const HChar* str)
{
   if(!VG_(strcasecmp)(str, "EPERM"))               return VKI_EPERM;
   else if(!VG_(strcasecmp)(str, "ENOENT"))         return VKI_ENOENT;
   else if(!VG_(strcasecmp)(str, "ESRCH"))          return VKI_ESRCH;
   else if(!VG_(strcasecmp)(str, "EINTR"))          return VKI_EINTR;
   else if(!VG_(strcasecmp)(str, "EIO"))            return VKI_EIO;
   else if(!VG_(strcasecmp)(str, "ENXIO"))          return VKI_ENXIO;
   else if(!VG_(strcasecmp)(str, "E2BIG"))          return VKI_E2BIG;
   else if(!VG_(strcasecmp)(str, "ENOEXEC"))        return VKI_ENOEXEC;
   else if(!VG_(strcasecmp)(str, "EBADF"))          return VKI_EBADF;
   else if(!VG_(strcasecmp)(str, "ECHILD"))         return VKI_ECHILD;
   else if(!VG_(strcasecmp)(str, "EDEADLK"))        return VKI_EDEADLK;
   else if(!VG_(strcasecmp)(str, "ENOMEM"))         return VKI_ENOMEM;
   else if(!VG_(strcasecmp)(str, "EACCES"))         return VKI_EACCES;
   else if(!VG_(strcasecmp)(str, "EFAULT"))         return VKI_EFAULT;
   else if(!VG_(strcasecmp)(str, "ENOTBLK"))        return VKI_ENOTBLK;
   else if(!VG_(strcasecmp)(str, "EBUSY"))          return VKI_EBUSY;
   else if(!VG_(strcasecmp)(str, "EEXIST"))         return VKI_EEXIST;
   else if(!VG_(strcasecmp)(str, "EXDEV"))          return VKI_EXDEV;
   else if(!VG_(strcasecmp)(str, "ENODEV"))         return VKI_ENODEV;
   else if(!VG_(strcasecmp)(str, "ENOTDIR"))        return VKI_ENOTDIR;
   else if(!VG_(strcasecmp)(str, "EISDIR"))         return VKI_EISDIR;
   else if(!VG_(strcasecmp)(str, "EINVAL"))         return VKI_EINVAL;
   else if(!VG_(strcasecmp)(str, "ENFILE"))         return VKI_ENFILE;
   else if(!VG_(strcasecmp)(str, "EMFILE"))         return VKI_EMFILE;
   else if(!VG_(strcasecmp)(str, "ENOTTY"))         return VKI_ENOTTY;
   else if(!VG_(strcasecmp)(str, "ETXTBSY"))        return VKI_ETXTBSY;
   else if(!VG_(strcasecmp)(str, "EFBIG"))          return VKI_EFBIG;
   else if(!VG_(strcasecmp)(str, "ENOSPC"))         return VKI_ENOSPC;
   else if(!VG_(strcasecmp)(str, "ESPIPE"))         return VKI_ESPIPE;
   else if(!VG_(strcasecmp)(str, "EROFS"))          return VKI_EROFS;
   else if(!VG_(strcasecmp)(str, "EMLINK"))         return VKI_EMLINK;
   else if(!VG_(strcasecmp)(str, "EPIPE"))          return VKI_EPIPE;
   else if(!VG_(strcasecmp)(str, "EDOM"))           return VKI_EDOM;
   else if(!VG_(strcasecmp)(str, "ERANGE"))         return VKI_ERANGE;
   else if(!VG_(strcasecmp)(str, "EAGAIN"))         return VKI_EAGAIN;
   else if(!VG_(strcasecmp)(str, "EINPROGRESS"))    return VKI_EINPROGRESS;
   else if(!VG_(strcasecmp)(str, "EALREADY"))       return VKI_EALREADY;
   else if(!VG_(strcasecmp)(str, "ENOTSOCK"))       return VKI_ENOTSOCK;
   else if(!VG_(strcasecmp)(str, "EDESTADDRREQ"))   return VKI_EDESTADDRREQ;
   else if(!VG_(strcasecmp)(str, "EMSGSIZE"))       return VKI_EMSGSIZE;
   else if(!VG_(strcasecmp)(str, "EPROTOTYPE"))     return VKI_EPROTOTYPE;
   else if(!VG_(strcasecmp)(str, "ENOPROTOOPT"))    return VKI_ENOPROTOOPT;
   else if(!VG_(strcasecmp)(str, "EPROTONOSUPPORT"))return VKI_EPROTONOSUPPORT;
   else if(!VG_(strcasecmp)(str, "ESOCKTNOSUPPORT"))return VKI_ESOCKTNOSUPPORT;
   else if(!VG_(strcasecmp)(str, "ENOTSUP"))        return VKI_ENOTSUP;
   else if(!VG_(strcasecmp)(str, "EPFNOSUPPORT"))   return VKI_EPFNOSUPPORT;
   else if(!VG_(strcasecmp)(str, "EAFNOSUPPORT"))   return VKI_EAFNOSUPPORT;
   else if(!VG_(strcasecmp)(str, "EADDRINUSE"))     return VKI_EADDRINUSE;
   else if(!VG_(strcasecmp)(str, "EADDRNOTAVAIL"))  return VKI_EADDRNOTAVAIL;
   else if(!VG_(strcasecmp)(str, "ENETDOWN"))       return VKI_ENETDOWN;
   else if(!VG_(strcasecmp)(str, "ENETUNREACH"))    return VKI_ENETUNREACH;
   else if(!VG_(strcasecmp)(str, "ENETRESET"))      return VKI_ENETRESET;
   else if(!VG_(strcasecmp)(str, "ECONNABORTED"))   return VKI_ECONNABORTED;
   else if(!VG_(strcasecmp)(str, "ECONNRESET"))     return VKI_ECONNRESET;
   else if(!VG_(strcasecmp)(str, "ENOBUFS"))        return VKI_ENOBUFS;
   else if(!VG_(strcasecmp)(str, "EISCONN"))        return VKI_EISCONN;
   else if(!VG_(strcasecmp)(str, "ENOTCONN"))       return VKI_ENOTCONN;
   else if(!VG_(strcasecmp)(str, "ESHUTDOWN"))      return VKI_ESHUTDOWN;
   else if(!VG_(strcasecmp)(str, "ETOOMANYREFS"))   return VKI_ETOOMANYREFS;
   else if(!VG_(strcasecmp)(str, "ETIMEDOUT"))      return VKI_ETIMEDOUT;
   else if(!VG_(strcasecmp)(str, "ECONNREFUSED"))   return VKI_ECONNREFUSED;
   else if(!VG_(strcasecmp)(str, "ELOOP"))          return VKI_ELOOP;
   else if(!VG_(strcasecmp)(str, "ENAMETOOLONG"))   return VKI_ENAMETOOLONG;
   else if(!VG_(strcasecmp)(str, "EHOSTDOWN"))      return VKI_EHOSTDOWN;
   else if(!VG_(strcasecmp)(str, "EHOSTUNREACH"))   return VKI_EHOSTUNREACH;
   else if(!VG_(strcasecmp)(str, "ENOTEMPTY"))      return VKI_ENOTEMPTY;
   else if(!VG_(strcasecmp)(str, "EPROCLIM"))       return VKI_EPROCLIM;
   else if(!VG_(strcasecmp)(str, "EUSERS"))         return VKI_EUSERS;
   else if(!VG_(strcasecmp)(str, "EDQUOT"))         return VKI_EDQUOT;
   else if(!VG_(strcasecmp)(str, "ESTALE"))         return VKI_ESTALE;
   else if(!VG_(strcasecmp)(str, "EREMOTE"))        return VKI_EREMOTE;
   else if(!VG_(strcasecmp)(str, "EBADRPC"))        return VKI_EBADRPC;
   else if(!VG_(strcasecmp)(str, "ERPCMISMATCH"))   return VKI_ERPCMISMATCH;
   else if(!VG_(strcasecmp)(str, "EPROGUNAVAIL"))   return VKI_EPROGUNAVAIL;
   else if(!VG_(strcasecmp)(str, "EPROGMISMATCH"))  return VKI_EPROGMISMATCH;
   else if(!VG_(strcasecmp)(str, "EPROCUNAVAIL"))   return VKI_EPROCUNAVAIL;
   else if(!VG_(strcasecmp)(str, "ENOLCK"))         return VKI_ENOLCK;
   else if(!VG_(strcasecmp)(str, "ENOSYS"))         return VKI_ENOSYS;
   else if(!VG_(strcasecmp)(str, "EFTYPE"))         return VKI_EFTYPE;
   else if(!VG_(strcasecmp)(str, "EAUTH"))          return VKI_EAUTH;
   else if(!VG_(strcasecmp)(str, "ENEEDAUTH"))      return VKI_ENEEDAUTH;
   else if(!VG_(strcasecmp)(str, "EPWROFF"))        return VKI_EPWROFF;
   else if(!VG_(strcasecmp)(str, "EDEVERR"))        return VKI_EDEVERR;
   else if(!VG_(strcasecmp)(str, "EOVERFLOW"))      return VKI_EOVERFLOW;
   else if(!VG_(strcasecmp)(str, "EBADEXEC"))       return VKI_EBADEXEC;
   else if(!VG_(strcasecmp)(str, "EBADARCH"))       return VKI_EBADARCH;
   else if(!VG_(strcasecmp)(str, "ESHLIBVERS"))     return VKI_ESHLIBVERS;
   else if(!VG_(strcasecmp)(str, "EBADMACHO"))      return VKI_EBADMACHO;
   else if(!VG_(strcasecmp)(str, "ECANCELED"))      return VKI_ECANCELED;
   else if(!VG_(strcasecmp)(str, "EIDRM"))          return VKI_EIDRM;
   else if(!VG_(strcasecmp)(str, "ENOMSG"))         return VKI_ENOMSG;
   else if(!VG_(strcasecmp)(str, "EILSEQ"))         return VKI_EILSEQ;
   else if(!VG_(strcasecmp)(str, "ENOATTR"))        return VKI_ENOATTR;
   else if(!VG_(strcasecmp)(str, "EBADMSG"))        return VKI_EBADMSG;
   else if(!VG_(strcasecmp)(str, "EMULTIHOP"))      return VKI_EMULTIHOP;
   else if(!VG_(strcasecmp)(str, "ENODATA"))        return VKI_ENODATA;
   else if(!VG_(strcasecmp)(str, "ENOLINK"))        return VKI_ENOLINK;
   else if(!VG_(strcasecmp)(str, "ENOSR"))          return VKI_ENOSR;
   else if(!VG_(strcasecmp)(str, "ENOSTR"))         return VKI_ENOSTR;
   else if(!VG_(strcasecmp)(str, "EPROTO"))         return VKI_EPROTO;
   else if(!VG_(strcasecmp)(str, "ETIME"))          return VKI_ETIME;
   else if(!VG_(strcasecmp)(str, "EOPNOTSUPP"))     return VKI_EOPNOTSUPP;
   else if(!VG_(strcasecmp)(str, "ELAST"))          return VKI_ELAST;
   else                                             return -1;
}

#elif defined(VGO_solaris)
Int fg_str2errno(const HChar* str)
{
   if(!VG_(strcasecmp)(str, "EPERM"))           return VKI_EPERM;
   else if(!VG_(strcasecmp)(str, "ENOENT"))     return VKI_ENOENT;
   else if(!VG_(strcasecmp)(str, "ESRCH"))      return VKI_ESRCH;
   else if(!VG_(strcasecmp)(str, "EINTR"))      return VKI_EINTR;
   else if(!VG_(strcasecmp)(str, "EIO"))        return VKI_EIO;
   else if(!VG_(strcasecmp)(str, "ENXIO"))      return VKI_ENXIO;
   else if(!VG_(strcasecmp)(str, "E2BIG"))      return VKI_E2BIG;
   else if(!VG_(strcasecmp)(str, "EBADF"))      return VKI_EBADF;
   else if(!VG_(strcasecmp)(str, "ECHILD"))     return VKI_ECHILD;
   else if(!VG_(strcasecmp)(str, "ENOEXEC"))    return VKI_ENOEXEC;
   else if(!VG_(strcasecmp)(str, "EAGAIN"))     return VKI_EAGAIN;
   else if(!VG_(strcasecmp)(str, "ENOMEM"))     return VKI_ENOMEM;
   else if(!VG_(strcasecmp)(str, "EACCES"))     return VKI_EACCES;
   else if(!VG_(strcasecmp)(str, "EFAULT"))     return VKI_EFAULT;
   else if(!VG_(strcasecmp)(str, "ENOTBLK"))    return VKI_ENOTBLK;
   else if(!VG_(strcasecmp)(str, "EBUSY"))      return VKI_EBUSY;
   else if(!VG_(strcasecmp)(str, "EEXIST"))     return VKI_EEXIST;
   else if(!VG_(strcasecmp)(str, "EXDEV"))      return VKI_EXDEV;
   else if(!VG_(strcasecmp)(str, "ENODEV"))     return VKI_ENODEV;
   else if(!VG_(strcasecmp)(str, "ENOTDIR"))    return VKI_ENOTDIR;
   else if(!VG_(strcasecmp)(str, "EISDIR"))     return VKI_EISDIR;
   else if(!VG_(strcasecmp)(str, "EINVAL"))     return VKI_EINVAL;
   else if(!VG_(strcasecmp)(str, "ENFILE"))     return VKI_ENFILE;
   else if(!VG_(strcasecmp)(str, "EMFILE"))     return VKI_EMFILE;
   else if(!VG_(strcasecmp)(str, "ENOTTY"))     return VKI_ENOTTY;
   else if(!VG_(strcasecmp)(str, "ETXTBSY"))    return VKI_ETXTBSY;
   else if(!VG_(strcasecmp)(str, "EFBIG"))      return VKI_EFBIG;
   else if(!VG_(strcasecmp)(str, "ENOSPC"))     return VKI_ENOSPC;
   else if(!VG_(strcasecmp)(str, "ESPIPE"))     return VKI_ESPIPE;
   else if(!VG_(strcasecmp)(str, "EROFS"))      return VKI_EROFS;
   else if(!VG_(strcasecmp)(str, "EMLINK"))     return VKI_EMLINK;
   else if(!VG_(strcasecmp)(str, "EPIPE"))      return VKI_EPIPE;
   else if(!VG_(strcasecmp)(str, "EDOM"))       return VKI_EDOM;
   else if(!VG_(strcasecmp)(str, "ERANGE"))     return VKI_ERANGE;
   else if(!VG_(strcasecmp)(str, "ENOTSUP"))    return VKI_ENOTSUP;
   else if(!VG_(strcasecmp)(str, "ENODATA"))    return VKI_ENODATA;
   else if(!VG_(strcasecmp)(str, "EOVERFLOW"))  return VKI_EOVERFLOW;
   else if(!VG_(strcasecmp)(str, "ENOSYS"))     return VKI_ENOSYS;
   else if(!VG_(strcasecmp)(str, "ERESTART"))   return VKI_ERESTART;
   else if(!VG_(strcasecmp)(str, "EADDRINUSE")) return VKI_EADDRINUSE;
   else                                         return -1;
}

#else
#  error Unknown Plat/OS
#endif
