/* - mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; -*-
 *  vim:expandtab:shiftwidth=2:tabstop=2:smarttab:
 *
 *  Copyright (C) 2008 Sun Microsystems, Inc.
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

#include "config.h"
#include "drizzled/stacktrace.h"
#include <cstddef>

#include <signal.h>
#include "drizzled/internal/my_pthread.h"
#include "drizzled/internal/m_string.h"
#ifdef HAVE_STACKTRACE
#include <unistd.h>
#include <strings.h>

#if HAVE_EXECINFO_H
#include <execinfo.h>
#endif

#include <cstring>
#include <cstdio>
#include <algorithm>

#if defined(BACKTRACE_DEMANGLE)
# include <cxxabi.h>
#endif

#include "drizzled/definitions.h"

using namespace std;

namespace drizzled
{

#define PTR_SANE(p) ((p) && (char*)(p) >= heap_start && (char*)(p) <= heap_end)

char *heap_start;

void safe_print_str(const char* name, const char* val, int max_len)
{
  char *heap_end= (char*) sbrk(0);
  fprintf(stderr, "%s at %p ", name, val);

  if (!PTR_SANE(val))
  {
    fprintf(stderr, " is invalid pointer\n");
    return;
  }

  fprintf(stderr, "= ");
  for (; max_len && PTR_SANE(val) && *val; --max_len)
    fputc(*val++, stderr);
  fputc('\n', stderr);
}

#ifdef TARGET_OS_LINUX

#ifdef __i386__
#define SIGRETURN_FRAME_OFFSET 17
#endif

#ifdef __x86_64__
#define SIGRETURN_FRAME_OFFSET 23
#endif

#if defined(BACKTRACE_DEMANGLE)

static inline char *my_demangle(const char *mangled_name, int *status)
{
  return abi::__cxa_demangle(mangled_name, NULL, NULL, status);
}

static void my_demangle_symbols(char **addrs, int n)
{
  int status, i;
  char *begin, *end, *demangled;

  for (i= 0; i < n; i++)
  {
    demangled= NULL;
    begin= strchr(addrs[i], '(');
    end= begin ? strchr(begin, '+') : NULL;

    if (begin && end)
    {
      *begin++= *end++= '\0';
      demangled= my_demangle(begin, &status);
      if (!demangled || status)
      {
        demangled= NULL;
        begin[-1]= '(';
        end[-1]= '+';
      }
    }

    if (demangled)
      fprintf(stderr, "%s(%s+%s\n", addrs[i], demangled, end);
    else
      fprintf(stderr, "%s\n", addrs[i]);
  }
}
#endif


#if HAVE_BACKTRACE
static void backtrace_current_thread(void)
{
  void *addrs[128];
  char **strings= NULL;
  int n = backtrace(addrs, array_elements(addrs));
#if BACKTRACE_DEMANGLE
  if ((strings= backtrace_symbols(addrs, n)))
  {
    my_demangle_symbols(strings, n);
    free(strings);
  }
#endif
#if HAVE_BACKTRACE_SYMBOLS_FD
  if (!strings)
  {
    backtrace_symbols_fd(addrs, n, fileno(stderr));
  }
#endif
}
#endif


void  print_stacktrace(unsigned char* stack_bottom, size_t thread_stack)
{
#if HAVE_BACKTRACE
  backtrace_current_thread();
  return;
#endif
  unsigned char** fp;
  uint32_t frame_count = 0, sigreturn_frame_count;

#ifdef __i386__
  __asm __volatile__ ("movl %%ebp,%0"
                      :"=r"(fp)
                      :"r"(fp));
#endif
#ifdef __x86_64__
  __asm __volatile__ ("movq %%rbp,%0"
                      :"=r"(fp)
                      :"r"(fp));
#endif
  if (!fp)
  {
    fprintf(stderr, "frame pointer is NULL, did you compile with\n\
-fomit-frame-pointer? Aborting backtrace!\n");
    return;
  }

  if (!stack_bottom || (unsigned char*) stack_bottom > (unsigned char*) &fp)
  {
    ulong tmp= min((size_t)0x10000,thread_stack);
    /* Assume that the stack starts at the previous even 65K */
    stack_bottom= (unsigned char*) (((ulong) &fp + tmp) &
			  ~(ulong) 0xFFFF);
    fprintf(stderr, "Cannot determine thread, fp=%p, backtrace may not be correct.\n", (void *)fp);
  }
  if (fp > (unsigned char**) stack_bottom ||
      fp < (unsigned char**) stack_bottom - thread_stack)
  {
    fprintf(stderr, "Bogus stack limit or frame pointer,\
 fp=%p, stack_bottom=%p, thread_stack=%"PRIu64", aborting backtrace.\n",
	    (void *)fp, (void *)stack_bottom, (uint64_t)thread_stack);
    return;
  }

  fprintf(stderr, "Stack range sanity check OK, backtrace follows:\n");

  /* We are 1 frame above signal frame with NPTL and 2 frames above with LT */
  sigreturn_frame_count = internal::thd_lib_detected == THD_LIB_LT ? 2 : 1;

  while (fp < (unsigned char**) stack_bottom)
  {
#if defined(__i386__) || defined(__x86_64__)
    unsigned char** new_fp = (unsigned char**)*fp;
    fprintf(stderr, "%p\n", frame_count == sigreturn_frame_count ?
            *(fp + SIGRETURN_FRAME_OFFSET) : *(fp + 1));
#endif /* defined(__386__)  || defined(__x86_64__) */

    if (new_fp <= fp )
    {
      fprintf(stderr, "New value of fp=%p failed sanity check,\
 terminating stack trace!\n", (void *)new_fp);
      goto end;
    }
    fp = new_fp;
    ++frame_count;
  }

  fprintf(stderr, "Stack trace seems successful - bottom reached\n");

end:
  fprintf(stderr,
          "Please read http://dev.mysql.com/doc/refman/5.1/en/resolve-stack-dump.html\n"
          "and follow instructions on how to resolve the stack trace.\n"
          "Resolved stack trace is much more helpful in diagnosing the\n"
          "problem, so please do resolve it\n");
}
#endif /* TARGET_OS_LINUX */

} /* namespace drizzled */

#endif /* HAVE_STACKTRACE */

/* Produce a core for the thread */

namespace drizzled
{

void write_core(int sig)
{
  signal(sig, SIG_DFL);
#ifdef HAVE_gcov
  /*
    For GCOV build, crashing will prevent the writing of code coverage
    information from this process, causing gcov output to be incomplete.
    So we force the writing of coverage information here before terminating.
  */
  extern void __gcov_flush(void);
  __gcov_flush();
#endif
  pthread_kill(pthread_self(), sig);
#if defined(P_MYID) && !defined(SCO)
  /* On Solaris, the above kill is not enough */
  sigsend(P_PID,P_MYID,sig);
#endif
}

} /* namespace drizzled */
