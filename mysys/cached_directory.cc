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

#include "cached_directory.h"

using namespace std;

namespace mysys
{

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
  size_t size;
  DIR *dirp;

  if ((dirp= opendir(in_path.c_str())) == NULL)
  {
    error= errno;
    return false;
  }

  /*
   * The readdir_r() call on Solaris operates a bit differently from other
   * systems in that the dirent structure must be allocated along with enough
   * space to contain the filename (see man page for readdir_r on Solaris).
   */

#ifdef SOLARIS
  size= sizeof(dirent) + pathconf(in_path.c_str(), _PC_NAME_MAX);
#else
  size= sizeof(dirent);
#endif

  dirent *entry= (dirent *) malloc(size);

  if (entry == NULL)
  {
    error= errno;
    closedir(dirp);
    return false;
  }

  int retcode;
  dirent *result;

  retcode= readdir_r(dirp, entry, &result);
  while ((retcode == 0) && (result != NULL))
  {
    entries.push_back(new Entry(entry->d_name));
    retcode= readdir_r(dirp, entry, &result);
  }
    
  closedir(dirp);
  free(entry);

  error= retcode;

  return error == 0;
}

} /* end namespace mysys */
