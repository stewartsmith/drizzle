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

#include <string.h>
#include <stdlib.h>
#include <iostream>

#ifdef __GNUC__
#ifdef HAVE_BACKTRACE
#include <execinfo.h>
#include <cxxabi.h>
#endif // HAVE_BACKTRACE
#endif // __GNUC__


namespace drizzled
{
namespace util
{

void custom_backtrace(void)
{
#ifdef __GNUC__
#ifdef HAVE_BACKTRACE
  void *array[50];
  size_t size;
  char **strings;

  size= backtrace(array, 50);
  strings= backtrace_symbols(array, size);

  std::cerr << "Number of stack frames obtained: " << size <<  std::endl;

  for (size_t x= 1; x < size; x++) 
  {
    size_t sz= 200;
    char *function= (char *)malloc(sz);
    char *begin= 0;
    char *end= 0;

    for (char *j = strings[x]; *j; ++j)
    {
      if (*j == '(') {
        begin = j;
      }
      else if (*j == '+') {
        end = j;
      }
    }
    if (begin && end)
    {
      begin++;
      *end= '\0';

      int status;
      char *ret = abi::__cxa_demangle(begin, function, &sz, &status);
      if (ret) 
      {
        function= ret;
      }
      else
      {
        strncpy(function, begin, sz);
        strncat(function, "()", sz);
        function[sz-1] = '\0';
      }
      std::cerr << function << std::endl;
    }
    else
    {
      std::cerr << strings[x] << std::endl;
    }
    free(function);
  }


  free (strings);
#endif // HAVE_BACKTRACE
#endif // __GNUC__
}

} /* namespace util */
} /* namespace drizzled */
