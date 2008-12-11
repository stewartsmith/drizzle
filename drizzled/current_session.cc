/* -*- mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; -*-
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

#include <drizzled/server_includes.h>
#include <drizzled/current_session.h>

#include <pthread.h>

extern pthread_key_t THR_Session;
extern pthread_key_t THR_Mem_root;

Session *_current_session(void)
{
  return static_cast<Session *>(pthread_getspecific(THR_Session));
}


MEM_ROOT *current_mem_root(void)
{
  return *(static_cast<MEM_ROOT **>(pthread_getspecific(THR_Mem_root)));
}


MEM_ROOT **current_mem_root_ptr(void)
{
  return static_cast<MEM_ROOT **>(pthread_getspecific(THR_Mem_root));
}

