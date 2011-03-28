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
#include "my_static.h"
#include <drizzled/error.h>
#include <stdio.h>
#include <errno.h>
#include <string>
#ifdef HAVE_PATHS_H
#include <paths.h>
#endif

using namespace std;
namespace drizzled
{
namespace internal
{

/*
  @brief
  Create a temporary file with unique name in a given directory

  @details
  create_temp_file
    to             pointer to buffer where temporary filename will be stored
    dir            directory where to create the file
    prefix         prefix the filename with this
    MyFlags        Magic flags

  @return
    File descriptor of opened file if success
    -1 and sets errno if fails.

  @note
    The behaviour of this function differs a lot between
    implementation, it's main use is to generate a file with
    a name that does not already exist.

    The implementation using mkstemp should be considered the
    reference implementation when adding a new or modifying an
    existing one

*/

int create_temp_file(char *to, const char *dir, const char *prefix,
                     myf MyFlags)
{
  string prefix_str= prefix ? prefix : "tmp.";
  prefix_str.append("XXXXXX");

  if (!dir && ! (dir =getenv("TMPDIR")))
    dir= P_tmpdir;
  if (strlen(dir)+prefix_str.length() > FN_REFLEN-2)
  {
    errno= ENAMETOOLONG;
    return -1;
  }
  strcpy(convert_dirname(to,dir,NULL),prefix_str.c_str());
  int org_file= mkstemp(to);
  /* TODO: This was old behavior, but really don't we want to
   * unlink files immediately under all circumstances?
   * if (mode & O_TEMPORARY)
    (void) my_delete(to, MYF(MY_WME | ME_NOINPUT));
  */
  int file= my_register_filename(org_file, to, EE_CANTCREATEFILE, MyFlags);

  /* If we didn't manage to register the name, remove the temp file */
  if (org_file >= 0 && file < 0)
  {
    int tmp= errno;
    close(org_file);
    (void) my_delete(to, MYF(MY_WME | ME_NOINPUT));
    errno= tmp;
  }

  return file;
}

} /* namespace internal */
} /* namespace drizzled */
