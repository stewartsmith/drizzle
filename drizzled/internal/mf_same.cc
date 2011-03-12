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

/* Kopierar biblioteksstrukturen och extensionen fr}n ett filnamn */

#include <config.h>

#include <drizzled/internal/my_sys.h>
#include <drizzled/internal/m_string.h>

namespace drizzled
{
namespace internal
{

        /*
	  Copy directory and/or extension between filenames.
	  (For the meaning of 'flag', check mf_format.c)
	  'to' may be equal to 'name'.
	  Returns 'to'.
	*/

char * fn_same(char *to, const char *name, int flag)
{
  char dev[FN_REFLEN];
  const char *ext;
  size_t dev_length;

  if ((ext=strrchr(name+dirname_part(dev, name, &dev_length),FN_EXTCHAR)) == 0)
    ext="";

  return(fn_format(to,to,dev,ext,flag));
} /* fn_same */

} /* namespace internal */
} /* namespace drizzled */
