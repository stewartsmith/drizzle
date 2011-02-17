/* Copyright (C) 2003 MySQL AB

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
#include <cstdlib>
#include <drizzled/internal/my_sys.h>
#include "transparent_file.h"
#include "utility.h"

using namespace drizzled;

TransparentFile::TransparentFile() : lower_bound(0), buff_size(IO_SIZE)
{
  buff= static_cast<unsigned char *>(malloc(buff_size*sizeof(unsigned char)));
}

TransparentFile::~TransparentFile()
{
  free(buff);
}

void TransparentFile::init_buff(int filedes_arg)
{
  filedes= filedes_arg;
  /* read the beginning of the file */
  lower_bound= 0;
  lseek(filedes, 0, SEEK_SET);
  if (filedes && buff)
    upper_bound= ::read(filedes, buff, buff_size);
}

unsigned char *TransparentFile::ptr()
{
  return buff;
}

off_t TransparentFile::start()
{
  return lower_bound;
}

off_t TransparentFile::end()
{
  return upper_bound;
}

off_t TransparentFile::read_next()
{
  ssize_t bytes_read;

  /*
     No need to seek here, as the file managed by TransparentFile class
     always points to upper_bound byte
  */
  if ((bytes_read= ::read(filedes, buff, buff_size)) < 0)
    return (off_t) -1;

  /* end of file */
  if (!bytes_read)
    return (off_t) -1;

  lower_bound= upper_bound;
  upper_bound+= bytes_read;

  return lower_bound;
}


char TransparentFile::get_value(off_t offset)
{
  ssize_t bytes_read;

  /* check boundaries */
  if ((lower_bound <= offset) && (offset < upper_bound))
    return buff[offset - lower_bound];

  lseek(filedes, offset, SEEK_SET);
  /* read appropriate portion of the file */
  if ((bytes_read= ::read(filedes, buff, buff_size)) < 0)
    return 0;

  lower_bound= offset;
  upper_bound= lower_bound + bytes_read;

  /* end of file */
  if (upper_bound == offset)
    return 0;

  return buff[0];
}
