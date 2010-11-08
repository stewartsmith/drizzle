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

#include "config.h"

#include <string.h>

#include "drizzled/errmsg_print.h"
#include "drizzled/memory/sql_alloc.h"
#include "drizzled/current_session.h"
#include "drizzled/error.h"
#include "drizzled/definitions.h"

#include "drizzled/internal/my_sys.h"

namespace drizzled
{

static void sql_alloc_error_handler(void)
{
  errmsg_printf(ERRMSG_LVL_ERROR, "%s",ER(ER_OUT_OF_RESOURCES));
}

void memory::init_sql_alloc(memory::Root *mem_root, size_t block_size, size_t)
{
  mem_root->init_alloc_root(block_size);
  mem_root->error_handler= sql_alloc_error_handler;
}


void *memory::sql_alloc(size_t Size)
{
  memory::Root *root= current_mem_root();
  return root->alloc_root(Size);
}


void *memory::sql_calloc(size_t size)
{
  void *ptr;

  if ((ptr=memory::sql_alloc(size)))
    memset(ptr, 0, size);

  return ptr;
}


char *memory::sql_strdup(const char *str)
{
  size_t len= strlen(str)+1;
  char *pos;
  if ((pos= (char*) memory::sql_alloc(len)))
    memcpy(pos,str,len);
  return pos;
}


char *memory::sql_strmake(const char *str, size_t len)
{
  char *pos;
  if ((pos= (char*) memory::sql_alloc(len+1)))
  {
    memcpy(pos,str,len);
    pos[len]=0;
  }
  return pos;
}


void* memory::sql_memdup(const void *ptr, size_t len)
{
  void *pos;
  if ((pos= memory::sql_alloc(len)))
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
  return mem_root->alloc_root(size);
}

void *memory::SqlAlloc::operator new(size_t size, memory::Root *mem_root)
{
  return mem_root->alloc_root(size);
}

} /* namespace drizzled */
