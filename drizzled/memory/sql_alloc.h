/* -*- mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; -*-
 *  vim:expandtab:shiftwidth=2:tabstop=2:smarttab:
 *
 *  Copyright (C) 2008 Sun Microsystems, Inc.
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



#ifndef DRIZZLED_MEMORY_SQL_ALLOC_H
#define DRIZZLED_MEMORY_SQL_ALLOC_H

#include <unistd.h>
#include "drizzled/memory/root.h"

#include "drizzled/visibility.h"

namespace drizzled
{
class Session;

namespace memory
{

void init_sql_alloc(Root *root, size_t block_size, size_t pre_alloc_size);
void *sql_alloc(size_t);
void *sql_calloc(size_t);
char *sql_strdup(const char *str);
char *sql_strmake(const char *str, size_t len);
void *sql_memdup(const void * ptr, size_t size);

class DRIZZLED_API SqlAlloc
{
public:
  static void *operator new(size_t size);
  static void *operator new[](size_t size);
  static void *operator new[](size_t size, Root *mem_root);
  static void *operator new(size_t size, Root *mem_root);
  static void operator delete(void *, size_t)
  {  }
  static void operator delete(void *, Root *)
  {  }
  static void operator delete[](void *, Root *)
  {  }
  static void operator delete[](void *, size_t)
  {  }
  SqlAlloc() {}
  /**
   * @TODO: Make this virtual... but List<> must be fixed first
   */
  ~SqlAlloc() {}

};

}
}

#endif /* DRIZZLED_MEMORY_SQL_ALLOC_H */
