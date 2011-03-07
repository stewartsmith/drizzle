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

#include <config.h>

#include <drizzled/internal/my_sys.h>
#include <drizzled/error.h>
#include <drizzled/internal/m_string.h>
#include <errno.h>

namespace drizzled
{
namespace internal
{

bool test_if_hard_path(const char *dir_name)
{
  if (dir_name[0] == FN_HOMELIB && dir_name[1] == FN_LIBCHAR)
    return (home_dir != NULL && test_if_hard_path(home_dir));
  if (dir_name[0] == FN_LIBCHAR)
    return (true);
  return false;
} /* test_if_hard_path */

} /* namespace internal */
} /* namespace drizzled */
