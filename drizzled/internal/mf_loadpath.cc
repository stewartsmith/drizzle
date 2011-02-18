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
#include <drizzled/internal/m_string.h>

namespace drizzled
{
namespace internal
{

	/* Returns full load-path for a file. to may be = path */
	/* if path is a hard-path return path */
	/* if path starts with home-dir return path */
	/* if path starts with current dir or parent-dir unpack path */
	/* if there is no path, prepend with own_path_prefix if given */
	/* else unpack path according to current dir */

char * my_load_path(char * to, const char *path,
		       const char *own_path_prefix)
{
  char buff[FN_REFLEN];
  int is_cur;

  if ((path[0] == FN_HOMELIB && path[1] == FN_LIBCHAR) ||
      test_if_hard_path(path))
    strcpy(buff,path);
  else if ((is_cur=(path[0] == FN_CURLIB && path[1] == FN_LIBCHAR)) ||
	   (strncmp(path,FN_PARENTDIR, strlen(FN_PARENTDIR)) == 0) ||
	   ! own_path_prefix)
  {
    if (is_cur)
      is_cur=2;					/* Remove current dir */
    if (! getcwd(buff,(uint32_t) (FN_REFLEN-strlen(path)+is_cur)))
      strcat(buff,path+is_cur);
    else
      strcpy(buff,path);			/* Return org file name */
  }
  else
    snprintf(buff, sizeof(buff), "%s%s",own_path_prefix,path);
  strcpy(to,buff);
  return(to);
} /* my_load_path */

} /* namespace internal */
} /* namespace drizzled */
