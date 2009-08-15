/* -*- mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; -*-
 *  vim:expandtab:shiftwidth=2:tabstop=2:smarttab:
 *
 *  Copyright (C) 2008 Sun Microsystems
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

/**
 * @file
 *   cached_directory.cc
 *
 * @brief
 *   Implementation of CachedDirectory class.
 */

#include "drizzled/global.h"
#include "cached_directory.h"

using namespace std;


CachedDirectory::CachedDirectory(const string &in_path)
  : error(0)
{
  (void) open(in_path);
}

CachedDirectory::~CachedDirectory()
{
  Entries::iterator p= entries.begin();
  while (p != entries.end())
  {
    if (*p)
      delete *p;
    ++p;
  }
}

bool CachedDirectory::open(const string &in_path)
{
  DIR *dirp= opendir(in_path.c_str());

  if (dirp == NULL)
  {
    error= errno;
    return false;
  }

  union {
    dirent entry;
#ifdef __sun
    /*
     * The readdir_r() call on Solaris operates a bit differently from other
     * systems in that the dirent structure must be allocated along with enough
     * space to contain the filename (see man page for readdir_r on Solaris).
     * Instead of dynamically try to allocate this buffer, just set the max
     * name for a path instead.
     */
    char space[sizeof(dirent) + PATH_MAX + 1];
#endif
  } buffer;

  int retcode;
  dirent *result;

  while ((retcode= readdir_r(dirp, &buffer.entry, &result)) == 0 &&
         result != NULL)
    entries.push_back(new Entry(result->d_name));
    
  closedir(dirp);
  error= retcode;

  return error == 0;
}
