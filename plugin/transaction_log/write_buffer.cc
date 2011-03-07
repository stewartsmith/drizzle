/* - mode: c; c-basic-offset: 2; indent-tabs-mode: nil; -*-
 *  vim:expandtab:shiftwidth=2:tabstop=2:smarttab:
 *
 *  Copyright (C) 2010 Jay Pipes <jaypipes@gmail.com>
 *
 *  Authors:
 *
 *  Jay Pipes <jaypipes@gmail.com>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
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
 *
 * Defines the implementation of a simple locked write buffer
 *
 * @details
 *
 * The write buffer keeps a block of allocated raw bytes available for
 * callers.  
 */

#include <config.h>
#include "write_buffer.h"

#include <drizzled/errmsg_print.h>
#include <drizzled/gettext.h>

#include <vector>

using namespace std;
using namespace drizzled;

WriteBuffer::WriteBuffer() :
  buffer()
{
  buffer.reserve(DEFAULT_WRITE_BUFFER_SIZE);
  pthread_mutex_init(&latch, NULL);
}

WriteBuffer::~WriteBuffer()
{
  buffer.clear();
  pthread_mutex_destroy(&latch);
}

void WriteBuffer::resize(size_t new_size)
{
  /* 
   * Attempt allocation of raw memory buffer for the 
   * requested size.  Does nothing if already allocated size
   * if greater...
   */
  if (buffer.capacity() >= new_size)
    return;

  buffer.reserve(new_size);
}
