/* Copyright (C) 2000-2001, 2003-2004 MySQL AB

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


/* Mallocs for used in threads */

#include <config.h>

#include <string.h>

#include <drizzled/errmsg_print.h>
#include <drizzled/memory/sql_alloc.h>
#include <drizzled/current_session.h>
#include <drizzled/error.h>
#include <drizzled/definitions.h>

#include <drizzled/internal/my_sys.h>

namespace drizzled {

void *memory::sql_alloc(size_t Size)
{
  return current_mem_root()->alloc(Size);
}

void *memory::sql_calloc(size_t size)
{
  void *ptr= memory::sql_alloc(size);
  memset(ptr, 0, size);
  return ptr;
}

char *memory::sql_strdup(const char *str)
{
  size_t len= strlen(str)+1;
  char *pos= (char*) memory::sql_alloc(len);
  memcpy(pos,str,len);
  return pos;
}

char *memory::sql_strmake(const char *str, size_t len)
{
  char *pos= (char*) memory::sql_alloc(len+1);
  memcpy(pos,str,len);
  pos[len]=0;
  return pos;
}

void* memory::sql_memdup(const void *ptr, size_t len)
{
  void *pos= memory::sql_alloc(len);
  memcpy(pos,ptr,len);
  return pos;
}

void *memory::SqlAlloc::operator new(size_t size)
{
  return memory::sql_alloc(size);
}

void *memory::SqlAlloc::operator new[](size_t size)
{
  return memory::sql_alloc(size);
}

void *memory::SqlAlloc::operator new[](size_t size, memory::Root *mem_root)
{
  return mem_root->alloc(size);
}

void *memory::SqlAlloc::operator new(size_t size, memory::Root *mem_root)
{
  return mem_root->alloc(size);
}

} /* namespace drizzled */
