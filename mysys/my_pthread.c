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

/* Functions to get threads more portable */

#define DONT_REMAP_PTHREAD_FUNCTIONS

#include "mysys_priv.h"
#ifdef THREAD
#include <signal.h>
#include <m_string.h>
#include <thr_alarm.h>

uint thd_lib_detected= 0;

int my_pthread_create_detached=1;

/****************************************************************************
 The following functions fixes that all pthread functions should work
 according to latest posix standard
****************************************************************************/

/* Undefined wrappers set my_pthread.h so that we call os functions */
#undef pthread_mutex_init
#undef pthread_mutex_lock
#undef pthread_mutex_unlock
#undef pthread_mutex_destroy
#undef pthread_mutex_wait
#undef pthread_mutex_timedwait
#undef pthread_mutex_trylock
#undef pthread_mutex_t
#undef pthread_cond_init
#undef pthread_cond_wait
#undef pthread_cond_timedwait
#undef pthread_cond_t
#undef pthread_attr_getstacksize
#endif /* THREAD */
