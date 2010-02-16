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

#ifndef DRIZZLED_STACKTRACE_H
#define DRIZZLED_STACKTRACE_H

#include <string.h>

#ifdef TARGET_OS_LINUX
#if defined(HAVE_STACKTRACE) || (defined (__x86_64__) || defined (__i386__) )
extern char* __bss_start;
#endif
#endif

namespace drizzled
{

#if defined(HAVE_BACKTRACE) && HAVE_BACKTRACE_SYMBOLS && HAVE_CXXABI_H && HAVE_ABI_CXA_DEMANGLE
#define BACKTRACE_DEMANGLE 1
#endif

#ifdef TARGET_OS_LINUX
#if defined(HAVE_STACKTRACE) || (defined (__x86_64__) || defined (__i386__) )
#undef HAVE_STACKTRACE
#define HAVE_STACKTRACE

extern char* heap_start;

#define init_stacktrace() do {                                 \
    heap_start = (char*) &__bss_start; \
  } while(0);
void check_thread_lib(void);
#endif /* defined (__i386__) */
#endif /* defined HAVE_OS_LINUX */

#ifdef HAVE_STACKTRACE
void print_stacktrace(unsigned char* stack_bottom, size_t thread_stack);
void safe_print_str(const char* name, const char* val, int max_len);
#else
/* Define empty prototypes for functions that are not implemented */
#define init_stacktrace() {}
#define print_stacktrace(A,B) {}
#define safe_print_str(A,B,C) {}
#endif /* HAVE_STACKTRACE */


void write_core(int sig);

} /* namespace drizzled */

#endif /* DRIZZLED_STACKTRACE_H */
