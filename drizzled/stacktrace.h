/* -*- mode: c; c-basic-offset: 2; indent-tabs-mode: nil; -*-
 *  vim:expandtab:shiftwidth=2:tabstop=2:smarttab:
 *
 *  Copyright (C) 2008 Sun Microsystems
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; version 2 of the License.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 */

#include "global.h"

#ifdef __cplusplus
extern "C" {
#endif

#if HAVE_BACKTRACE && HAVE_BACKTRACE_SYMBOLS && HAVE_CXXABI_H && HAVE_ABI_CXA_DEMANGLE
#define BACKTRACE_DEMANGLE 1
#endif

#if BACKTRACE_DEMANGLE
  char *my_demangle(const char *mangled_name, int *status);
#endif

#if defined(HAVE_STACKTRACE) || (defined (__x86_64__) || defined (__i386__) )
#undef HAVE_STACKTRACE
#define HAVE_STACKTRACE

  extern char* __bss_start;
  extern char* heap_start;

#define init_stacktrace() do {                                 \
    heap_start = (char*) &__bss_start; \
  } while(0);
  void check_thread_lib(void);
#endif /* defined (__i386__) */

#ifdef HAVE_STACKTRACE
  void print_stacktrace(uchar* stack_bottom, ulong thread_stack);
  void safe_print_str(const char* name, const char* val, int max_len);
#else
/* Define empty prototypes for functions that are not implemented */
#define init_stacktrace() {}
#define print_stacktrace(A,B) {}
#define safe_print_str(A,B,C) {}
#endif /* HAVE_STACKTRACE */


  void write_core(int sig);

#ifdef  __cplusplus
}
#endif
