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

#pragma once

#include <drizzled/memory/root.h>
#include <drizzled/visibility.h>

namespace drizzled {
namespace memory {

void* sql_alloc(size_t);
void* sql_calloc(size_t);
char* sql_strdup(const char*);
char* sql_strmake(const char*, size_t);
void* sql_memdup(const void*, size_t);

class DRIZZLED_API SqlAlloc
{
public:
  static void* operator new(size_t size)
  {
    return memory::sql_alloc(size);
  }

  static void* operator new[](size_t size)
  {
    return memory::sql_alloc(size);
  }

  static void* operator new(size_t size, Root& root)
  {
    return root.alloc(size);
  }

  static void* operator new[](size_t size, Root& root)
  {
    return root.alloc(size);
  }

  static void* operator new(size_t size, Root* root)
  {
    return root->alloc(size);
  }

  static void* operator new[](size_t size, Root* root)
  {
    return root->alloc(size);
  }

  static void operator delete(void*)
  {  
  }

  static void operator delete[](void*)
  {  
  }

  static void operator delete(void*, Root&)
  {  
  }

  static void operator delete[](void*, Root&)
  {  
  }

  static void operator delete(void*, Root*)
  {  
  }

  static void operator delete[](void*, Root*)
  {  
  }
};

}
}
