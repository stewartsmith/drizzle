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
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA */

/* Funktions for comparing with wild-cards */

#include <config.h>

#include <drizzled/internal/my_sys.h>

namespace drizzled
{
namespace internal
{

	/* Test if a string is "comparable" to a wild-card string */
	/* returns 0 if the strings are "comparable" */

char wild_many='%';
char wild_one='_';
char wild_prefix= '\\';

int wild_compare(const char *str, const char *wildstr, bool str_is_pattern)
{
  char cmp;

  while (*wildstr)
  {
    while (*wildstr && *wildstr != wild_many && *wildstr != wild_one)
    {
      if (*wildstr == wild_prefix && wildstr[1])
      {
	wildstr++;
        if (str_is_pattern && *str++ != wild_prefix)
          return 1;
      }
      if (*wildstr++ != *str++)
        return 1;
    }
    if (! *wildstr )
      return(*str != 0);
    if (*wildstr++ == wild_one)
    {
      if (! *str || (str_is_pattern && *str == wild_many))
        return 1;                     /* One char; skip */
      if (*str++ == wild_prefix && str_is_pattern && *str)
        str++;
    }
    else
    {						/* Found '*' */
      while (str_is_pattern && *str == wild_many)
        str++;
      for (; *wildstr ==  wild_many || *wildstr == wild_one; wildstr++)
        if (*wildstr == wild_many)
        {
          while (str_is_pattern && *str == wild_many)
            str++;
        }
        else
        {
          if (str_is_pattern && *str == wild_prefix && str[1])
            str+=2;
          else if (! *str++)
            return (1);
        }
      if (!*wildstr)
        return 0;		/* '*' as last char: OK */
      if ((cmp= *wildstr) == wild_prefix && wildstr[1] && !str_is_pattern)
        cmp=wildstr[1];
      for (;;str++)
      {
        while (*str && *str != cmp)
          str++;
        if (!*str)
          return (1);
	if (wild_compare(str,wildstr,str_is_pattern) == 0)
          return (0);
      }
      /* We will never come here */
    }
  }
  return (*str != 0);
} /* wild_compare */

} /* namespace internal */
} /* namespace drizzled */
