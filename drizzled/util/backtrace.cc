/* - mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; -*-
 *  vim:expandtab:shiftwidth=2:tabstop=2:smarttab:
 *
 *  Copyright (C) 2010 Brian Aker
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
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

#include <config.h>
#include <drizzled/util/backtrace.h>

#include <cstring>
#include <cstdlib>
#include <iostream>
#include <cstdio>

#ifdef __GNUC__
# ifdef HAVE_BACKTRACE
#   include <execinfo.h>
#   include <cxxabi.h>
# endif // HAVE_BACKTRACE
#endif // __GNUC__

namespace drizzled
{
namespace util
{

void custom_backtrace(const char *file, int line, const char *func, size_t depth)
{
  (void)file; (void)line; (void)func; (void)depth;
#ifdef HAVE_BACKTRACE
  void *array[50];

  size_t size= backtrace(array, 50);
  char **strings= backtrace_symbols(array, size);

  if (strings == NULL)
  {
    return;
  }

  std::cerr << std::endl << "call_backtrace(" << size << ") began at " << file << ":" << line << " for " << func << "()" << std::endl;

  if (depth == 0)
  {
    depth= size;
  }
  else
  {
    depth= std::min(depth, size);
  }

  char *named_function= (char *)::realloc(NULL, 1024);
  
  if (named_function == NULL)
  {
    ::free(strings);
    return;
  }

  for (size_t x= 1; x < depth; x++) 
  {
    if (true) // DEMANGLE
    {
      size_t sz= 200;
      char *named_function_ptr= (char *)::realloc(named_function, sz);
      if (named_function_ptr == NULL)
      {
        continue;
      }
      named_function= named_function_ptr;

      char *begin_name= 0;
      char *begin_offset= 0;
      char *end_offset= 0;

      for (char *j= strings[x]; *j; ++j)
      {
        if (*j == '(')
        {
          begin_name= j;
        }
        else if (*j == '+')
        {
          begin_offset= j;
        }
        else if (*j == ')' and begin_offset) 
        {
          end_offset= j;
          break;
        }
      }

      if (begin_name and begin_offset and end_offset and begin_name < begin_offset)
      {
        *begin_name++= '\0';
        *begin_offset++= '\0';
        *end_offset= '\0';

        int status;
        char *ret= abi::__cxa_demangle(begin_name, named_function, &sz, &status);
        if (ret) // realloc()'ed string
        {
          named_function= ret;
          std::cerr << " " << strings[x] << " : " << begin_name << "() + " << begin_offset << std::endl;
        }
        else
        {
          std::cerr << " " << strings[x] << " : " << begin_name << "() + " << begin_offset << std::endl;
        }
      }
      else
      {
        std::cerr << " " << strings[x] << std::endl;
      }
    }
    else
    {
      std::cerr << " unmangled:" << strings[x] << std::endl;
    }
  }

  ::free(named_function);
  ::free(strings);
#endif // HAVE_BACKTRACE
}

} /* namespace util */
} /* namespace drizzled */
