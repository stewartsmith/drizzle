/* Copyright (C) 2003 MySQL AB

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

/* Structures and functions private to the vio package */

#define DONT_MAP_VIO
#include <my_global.h>
#include <drizzle_com.h>
#include <mysys/my_sys.h>
#include <string.h>
#include <violite.h>

#include <sys/socket.h>
#include <netdb.h>

void	vio_ignore_timeout(Vio *vio, uint32_t which, uint32_t timeout);
void	vio_timeout(Vio *vio,uint32_t which, uint32_t timeout);
