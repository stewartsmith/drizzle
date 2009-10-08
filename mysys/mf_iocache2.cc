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
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA */

/*
  More functions to be used with IO_CACHE files
*/

#include "mysys/mysys_priv.h"
#include <mystrings/m_string.h>
#include <stdarg.h>
#include <mystrings/m_ctype.h>
#include <mysys/iocache.h>
#include <string>

using namespace std;

static my_off_t my_b_append_tell(IO_CACHE* info)
{
  /*
    Prevent optimizer from putting res in a register when debugging
    we need this to be able to see the value of res when the assert fails
  */
  volatile my_off_t res;

  /*
    We need to lock the append buffer mutex to keep flush_io_cache()
    from messing with the variables that we need in order to provide the
    answer to the question.
  */
  pthread_mutex_lock(&info->append_buffer_lock);
  res = info->end_of_file + (info->write_pos-info->append_read_pos);
  pthread_mutex_unlock(&info->append_buffer_lock);
  return res;
}

