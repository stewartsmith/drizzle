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

#include "mysys/mysys_priv.h"
#include "my_static.h"
#include <mysys/mysys_err.h>
#include <mystrings/m_string.h>
#include <mystrings/m_ctype.h>
#include <stdio.h>
#include <stdlib.h>

bool my_init_done= 0;
uint	mysys_usage_id= 0;              /* Incremented for each my_init() */

static uint32_t atoi_octal(const char *str)
{
  long int tmp;
  while (*str && my_isspace(&my_charset_utf8_general_ci, *str))
    str++;
  tmp= strtol(str, NULL, (*str == '0' ? 8 : 10));
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

void my_end(int)
{
  free_charsets();
  my_error_unregister_all();

  my_thread_end();
  my_thread_global_end();

  my_init_done=0;
} /* my_end */
