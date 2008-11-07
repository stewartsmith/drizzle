/* Copyright (C) 2000-2003 MySQL AB

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA */

#include "mysys_priv.h"
#include "my_static.h"
#include <mysys/mysys_err.h>
#include <mystrings/m_string.h>
#include <mystrings/m_ctype.h>
#include <signal.h>

bool my_init_done= 0;
uint	mysys_usage_id= 0;              /* Incremented for each my_init() */
uint32_t   my_thread_stack_size= 65536;

static uint32_t atoi_octal(const char *str)
{
  long int tmp;
  while (*str && my_isspace(&my_charset_utf8_general_ci, *str))
    str++;
  str2int(str,
	  (*str == '0' ? 8 : 10),       /* Octalt or decimalt */
	  0, INT_MAX, &tmp);
  return (uint32_t) tmp;
}


/*
  Init my_sys functions and my_sys variabels

  SYNOPSIS
    my_init()

  RETURN
    0  ok
    1  Couldn't initialize environment
*/

bool my_init(void)
{
  char * str;
  if (my_init_done)
    return 0;
  my_init_done=1;
  mysys_usage_id++;
  my_umask= 0660;                       /* Default umask for new files */
  my_umask_dir= 0700;                   /* Default umask for new directories */
  init_glob_errs();
#if defined(SAFE_MUTEX)
  safe_mutex_global_init();		/* Must be called early */
#endif
#if defined(MY_PTHREAD_FASTMUTEX) && !defined(SAFE_MUTEX)
  fastmutex_global_init();              /* Must be called early */
#endif
#if defined(HAVE_PTHREAD_INIT)
  pthread_init();
#endif
  if (my_thread_global_init())
    return 1;
  sigfillset(&my_signals);		/* signals blocked by mf_brkhant */
  {
    if (!home_dir)
    {					/* Don't initialize twice */
      if ((home_dir=getenv("HOME")) != 0)
	home_dir=intern_filename(home_dir_buff,home_dir);
      /* Default creation of new files */
      if ((str=getenv("UMASK")) != 0)
	my_umask=(int) (atoi_octal(str) | 0600);
	/* Default creation of new dir's */
      if ((str=getenv("UMASK_DIR")) != 0)
	my_umask_dir=(int) (atoi_octal(str) | 0700);
    }
    return(0);
  }
} /* my_init */


	/* End my_sys */

void my_end(int infoflag)
{
  /*
    this code is suboptimal to workaround a bug in
    Sun CC: Sun C++ 5.6 2004/06/02 for x86, and should not be
    optimized until this compiler is not in use anymore
  */
  FILE *info_file= stderr;
  bool print_info= 0;

  if ((infoflag & MY_CHECK_ERROR) || print_info)

  {					/* Test if some file is left open */
    if (my_file_opened | my_stream_opened)
    {
      sprintf(errbuff[0],EE(EE_OPEN_WARNING),my_file_opened,my_stream_opened);
      (void) my_message_no_curses(EE_OPEN_WARNING,errbuff[0],ME_BELL);
      my_print_open_files();
    }
  }
  free_charsets();
  my_error_unregister_all();
  my_once_free();

  if ((infoflag & MY_GIVE_INFO) || print_info)
  {
#ifdef HAVE_GETRUSAGE
    struct rusage rus;
#ifdef HAVE_purify
    /* Purify assumes that rus is uninitialized after getrusage call */
    memset(&rus, 0, sizeof(rus));
#endif
    if (!getrusage(RUSAGE_SELF, &rus))
      fprintf(info_file,"\n\
User time %.2f, System time %.2f\n\
Maximum resident set size %ld, Integral resident set size %ld\n\
Non-physical pagefaults %ld, Physical pagefaults %ld, Swaps %ld\n\
Blocks in %ld out %ld, Messages in %ld out %ld, Signals %ld\n\
Voluntary context switches %ld, Involuntary context switches %ld\n",
	      (rus.ru_utime.tv_sec * SCALE_SEC +
	       rus.ru_utime.tv_usec / SCALE_USEC) / 100.0,
	      (rus.ru_stime.tv_sec * SCALE_SEC +
	       rus.ru_stime.tv_usec / SCALE_USEC) / 100.0,
	      rus.ru_maxrss, rus.ru_idrss,
	      rus.ru_minflt, rus.ru_majflt,
	      rus.ru_nswap, rus.ru_inblock, rus.ru_oublock,
	      rus.ru_msgsnd, rus.ru_msgrcv, rus.ru_nsignals,
	      rus.ru_nvcsw, rus.ru_nivcsw);
#endif
  }
  else if (infoflag & MY_CHECK_ERROR)
  {
    TERMINATE(stderr, 0);		/* Print memory leaks on screen */
  }

  my_thread_end();
  my_thread_global_end();
#if defined(SAFE_MUTEX)
  /*
    Check on destroying of mutexes. A few may be left that will get cleaned
    up by C++ destructors
  */
  safe_mutex_end();

#endif /* defined(SAFE_MUTEX) */

  my_init_done=0;
} /* my_end */
