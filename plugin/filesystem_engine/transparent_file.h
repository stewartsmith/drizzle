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

#ifndef PLUGIN_FILESYSTEM_ENGINE_TRANSPARENT_FILE_H
#define PLUGIN_FILESYSTEM_ENGINE_TRANSPARENT_FILE_H

#include <sys/types.h>
#include <sys/stat.h>

class TransparentFile
{
  int filedes;
  unsigned char *buff;  /* in-memory window to the file or mmaped area */
  /* current window sizes */
  off_t lower_bound;
  off_t upper_bound;
  uint32_t buff_size;

public:

  TransparentFile();
  ~TransparentFile();

  void init_buff(int filedes_arg);
  unsigned char *ptr();
  off_t start();
  off_t end();
  char get_value (off_t offset);
  off_t read_next();
};

#endif /* PLUGIN_FILESYSTEM_ENGINE_TRANSPARENT_FILE_H */
