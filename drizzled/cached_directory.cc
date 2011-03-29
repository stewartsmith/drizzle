/* -*- mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; -*-
 *  vim:expandtab:shiftwidth=2:tabstop=2:smarttab:
 *
 *  Copyright (C) 2008 Sun Microsystems, Inc.
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

#include <config.h>
#include <dirent.h>
#include <drizzled/definitions.h>

#include <boost/foreach.hpp>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#include <strings.h>
#include <limits.h>

#include <drizzled/cached_directory.h>
#include <drizzled/util/find_ptr.h>
#include <drizzled/error_t.h>
#include <drizzled/error.h>
#include <drizzled/errmsg_print.h>

using namespace std;
using namespace drizzled;

namespace drizzled {

CachedDirectory::CachedDirectory() : 
  error(0)
{
}


CachedDirectory::CachedDirectory(const string &in_path) :
  error(0),
  use_full_path(false)
{
  // TODO: Toss future exception
  (void) open(in_path);
}


CachedDirectory::CachedDirectory(const string& in_path, set<string>& allowed_exts) :
  error(0),
  use_full_path(false)
{
  // TODO: Toss future exception
  (void) open(in_path, allowed_exts);
}

CachedDirectory::CachedDirectory(const string& in_path, enum CachedDirectory::FILTER filter, bool use_full_path_arg) :
  error(0),
  use_full_path(use_full_path_arg)
{
  set<string> empty;
  // TODO: Toss future exception
  (void) open(in_path, empty, filter);
}


CachedDirectory::~CachedDirectory()
{
	BOOST_FOREACH(Entries::reference iter, entries)
    delete iter;
}

bool CachedDirectory::open(const string &in_path)
{
  set<string> empty;
  return open(in_path, empty);
}

bool CachedDirectory::open(const string &in_path, set<string> &allowed_exts)
{
  return open(in_path, allowed_exts, CachedDirectory::NONE);
}

bool CachedDirectory::open(const string &in_path, set<string> &allowed_exts, enum CachedDirectory::FILTER filter)
{
  DIR *dirp= opendir(in_path.c_str());

  if (dirp == NULL)
  {
    error= errno;
    return false;
  }

  path= in_path;

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
  {
    std::string buffered_fullpath;
    if (not allowed_exts.empty())
    {
      char *ptr= rindex(result->d_name, '.');
      if (ptr && allowed_exts.count(ptr))
      {
        entries.push_back(new Entry(result->d_name));
      }
    }
    else
    {
      switch (filter)
      {
      case DIRECTORY:
        {
          struct stat entrystat;

          if (result->d_name[0] == '.') // We don't pass back anything hidden at the moment.
            continue;

          if (use_full_path)
          {
            buffered_fullpath.append(in_path);
            if (buffered_fullpath[buffered_fullpath.length()] != '/')
              buffered_fullpath.append(1, FN_LIBCHAR);
          }

          buffered_fullpath.append(result->d_name);

          int err= stat(buffered_fullpath.c_str(), &entrystat);

          if (err != 0)
            errmsg_printf(error::WARN, ER(ER_CANT_GET_STAT),
                          buffered_fullpath.c_str(),
                          errno);

          if (err == 0 && S_ISDIR(entrystat.st_mode))
          {
            entries.push_back(new Entry(result->d_name));
          }
        }
        break;
      case FILE:
        {
          struct stat entrystat;

          buffered_fullpath.append(in_path);
          if (buffered_fullpath[buffered_fullpath.length()] != '/')
            buffered_fullpath.append(1, FN_LIBCHAR);

          buffered_fullpath.assign(result->d_name);

          stat(buffered_fullpath.c_str(), &entrystat);

          if (S_ISREG(entrystat.st_mode))
          {
            entries.push_back(new Entry(result->d_name));
          }
        }
        break;
      case NONE:
      case MAX:
        entries.push_back(new Entry(result->d_name));
        break;
      }
    }
  }
    
  closedir(dirp);
  error= retcode;

  return error == 0;
}

std::ostream& operator<<(std::ostream& output, const CachedDirectory &directory)
{
  output << "CachedDirectory:(Path: " << directory.getPath() << ")\n";
  BOOST_FOREACH(const CachedDirectory::Entry* iter, directory.getEntries())
    output << "\t(" << iter->filename << ")\n";
  return output;
}

} /* namespace drizzled */
