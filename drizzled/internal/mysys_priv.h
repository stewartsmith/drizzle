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

#ifndef DRIZZLED_INTERNAL_MYSYS_PRIV_H
#define DRIZZLED_INTERNAL_MYSYS_PRIV_H

#include "config.h"
#include "drizzled/internal/my_sys.h"

#include <sys/resource.h>

#include "drizzled/internal/my_pthread.h"
extern pthread_mutex_t THR_LOCK_malloc, THR_LOCK_keycache;
extern pthread_mutex_t THR_LOCK_net;

/*
  EDQUOT is used only in 3 C files only in mysys/. If it does not exist on
  system, we set it to some value which can never happen.
*/
#ifndef EDQUOT
#define EDQUOT (-1)
#endif

#endif /* DRIZZLED_INTERNAL_MYSYS_PRIV_H */
