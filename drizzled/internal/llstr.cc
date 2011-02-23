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

/*
  Defines: llstr();

  llstr(value, buff);

  This function saves a int64_t value in a buffer and returns the pointer to
  the buffer.  This is useful when trying to portable print int64_t
  variables with printf() as there is no usable printf() standard one can use.
*/

#include <config.h>
#include <drizzled/internal/m_string.h>

namespace drizzled
{
namespace internal
{

char *llstr(int64_t value,char *buff)
{
  int64_t10_to_str(value,buff,-10);
  return buff;
}

char *ullstr(int64_t value,char *buff)
{
  int64_t10_to_str(value,buff,10);
  return buff;
}

} /* namespace internal */
} /* namespace drizzled */
