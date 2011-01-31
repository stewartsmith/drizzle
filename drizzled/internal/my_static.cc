/* Copyright (C) 2000 MySQL AB

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA */

/*
  Static variables for mysys library. All definied here for easy making of
  a shared library
*/

#include "config.h"

#include "drizzled/internal/my_sys.h"
#include "drizzled/error.h"
#include "my_static.h"
#include <stdlib.h>

#include "drizzled/visibility.h"

namespace drizzled
{
namespace internal
{

bool timed_mutexes= 0;

	/* from my_init */
char *	home_dir=0;
const char      *my_progname=0;
char curr_dir[FN_REFLEN]= {0},
     home_dir_buff[FN_REFLEN]= {0};
DRIZZLED_API int my_umask=0664;
int my_umask_dir=0777;
uint32_t   my_file_limit= MY_NFILE;

	/* From mf_brkhant */
int my_dont_interrupt=0;
volatile int		_my_signals=0;
sigset_t my_signals;			/* signals blocked by mf_brkhant */

	/* from mf_reccache.c */
uint32_t my_default_record_cache_size=RECORD_CACHE_SIZE;

	/* from soundex.c */
				/* ABCDEFGHIJKLMNOPQRSTUVWXYZ */
				/* :::::::::::::::::::::::::: */
const char *soundex_map=	  "01230120022455012623010202";

	/* from safe_malloc */
uint32_t sf_malloc_prehunc=0,		/* If you have problem with core- */
     sf_malloc_endhunc=0,		/* dump when malloc-message.... */
					/* set theese to 64 or 128  */
     sf_malloc_quick=0;			/* set if no calls to sanity */
uint32_t sf_malloc_cur_memory= 0L;		/* Current memory usage */
uint32_t sf_malloc_max_memory= 0L;		/* Maximum memory usage */
uint32_t  sf_malloc_count= 0;		/* Number of times NEW() was called */
unsigned char *sf_min_adress= (unsigned char*) ~(unsigned long) 0L,
     *sf_max_adress= (unsigned char*) 0L;
/* Root of the linked list of struct st_irem */
irem *sf_malloc_root = NULL;

	/* from my_alarm */
int volatile my_have_got_alarm=0;	/* declare variable to reset */
uint32_t my_time_to_wait_for_lock=2;	/* In seconds */

	/* How to disable options */
bool my_disable_async_io= true;
bool my_disable_flush_key_blocks=0;
bool my_disable_symlinks=0;
bool mysys_uses_curses=0;

} /* namespace internal */
} /* namespace drizzled */
