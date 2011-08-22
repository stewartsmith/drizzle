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
#include <drizzled/error.h>
#include <drizzled/internal/my_sys.h>
#include "keycache.h"
#include <drizzled/internal/m_string.h>
#include <drizzled/internal/my_bit.h>
#include <errno.h>
#include <stdarg.h>

using namespace drizzled;

int init_key_cache(KEY_CACHE *keycache, uint32_t, size_t, uint32_t, uint32_t)
{
  memset(keycache, 0, sizeof(KEY_CACHE));
  return 0;
}

unsigned char *key_cache_read(KEY_CACHE*, int file, internal::my_off_t filepos, int, unsigned char *buff, uint32_t length, uint32_t, int)
{
  return pread(file, buff, length, filepos) ? buff : NULL;
}

int key_cache_insert(KEY_CACHE*, int, internal::my_off_t, int, unsigned char*, uint32_t)
{
  return 0;
}

int key_cache_write(KEY_CACHE*, int file, internal::my_off_t filepos, int, unsigned char *buff, uint32_t length, uint32_t, int)
{
  return pwrite(file, buff, length, filepos) == 0;
}

int flush_key_blocks(KEY_CACHE*, int, flush_type)
{
  return 0;
}
