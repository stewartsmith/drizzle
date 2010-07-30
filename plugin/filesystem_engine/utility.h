/*
  Copyright (C) 2010 Zimin

  This program is free software; you can redistribute it and/or
  modify it under the terms of the GNU General Public License
  as published by the Free Software Foundation; either version 2
  of the License, or (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
*/

#ifndef PLUGIN_FILESYSTEM_ENGINE_UTILITY_H
#define PLUGIN_FILESYSTEM_ENGINE_UTILITY_H

static inline ssize_t write_in_all(int fd, const void *buf, size_t count)
{
  const char *p= (const char*)buf;
  ssize_t total= 0;
  while (count > 0)
  {
    ssize_t len;
    while ((len= ::write(fd, p, count)) < 0 &&
           (errno == EINTR || errno == EAGAIN))
      ;
    if (len < 0)
      return -1;
    if (len == 0)
    {
      errno= ENOSPC;
      return -1;
    }
    p+= len;
    total+= len;
    count-= len;
  }
  return total;
}

#endif /* PLUGIN_FILESYSTEM_ENGINE_UTILITY_H */
