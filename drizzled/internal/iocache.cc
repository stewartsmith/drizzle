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

/* Open a temporary file and cache it with io_cache. Delete it on close */

#include "config.h"

#include "drizzled/internal/my_sys.h"
#include "drizzled/internal/m_string.h"
#include "drizzled/internal/my_static.h"
#include "drizzled/internal/iocache.h"
#include "drizzled/error.h"

namespace drizzled
{
namespace internal
{

/*
** Open tempfile cached by st_io_cache
** Should be used when no seeks are done (only reinit_io_buff)
** Return false if cache is inited ok
** The actual file is created when the st_io_cache buffer gets filled
** If dir is not given, use TMPDIR.
*/

bool open_cached_file(st_io_cache *cache, const char *dir_arg, const char *prefix_arg,
		      size_t cache_size_arg, myf cache_myflags)
{
  cache->dir=	 dir_arg ? strdup(dir_arg) : (char*) 0;
  cache->prefix= (prefix_arg ? strdup(prefix_arg) : (char*) 0);

  if ((cache->dir == NULL) || (cache->prefix == NULL))
    return true;

  cache->file_name= 0;
  cache->buffer= 0;				/* Mark that not open */
  if (not cache->init_io_cache(-1, cache_size_arg,WRITE_CACHE,0L,0, MYF(cache_myflags | MY_NABP)))
  {
    return false;
  }
  free(cache->dir);
  free(cache->prefix);

  return true;
}

/* Create the temporary file */

bool st_io_cache::real_open_cached_file()
{
  char name_buff[FN_REFLEN];

  if ((file= create_temp_file(name_buff, dir, prefix, MYF(MY_WME))) >= 0)
  {
    my_delete(name_buff,MYF(MY_WME | ME_NOINPUT));
    return false;
  }

  return true;
}


void st_io_cache::close_cached_file()
{
  if (my_b_inited(this))
  {
    int _file= file;
    file= -1;				/* Don't flush data */
    (void) end_io_cache();
    if (_file >= 0)
    {
      (void) my_close(_file, MYF(0));
#ifdef CANT_DELETE_OPEN_FILES
      if (file_name)
      {
	(void) my_delete(file_name, MYF(MY_WME | ME_NOINPUT));
	free(file_name);
      }
#endif
    }
    free(dir);
    free(prefix);
  }
}

} /* namespace internal */
} /* namespace drizzled */
