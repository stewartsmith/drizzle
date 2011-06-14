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
 * Defines a simple structure for maintaining a write buffer.
 */

#pragma once

#include <stdint.h>
#include <vector>
#include <pthread.h>

class WriteBuffer
{
public:
  static const size_t DEFAULT_WRITE_BUFFER_SIZE= 1024; /* Many GPB messages are < 1 KB... */
  /**
   * Constructor.
   */
  WriteBuffer();
  ~WriteBuffer();
  /**
   * Locks the log write buffer
   */
  void lock()
  {
    pthread_mutex_lock(&latch);
  }
  /**
   * Unlocks the log's write buffer
   */
  void unlock()
  {
    pthread_mutex_unlock(&latch);
  }
  /**
   * Resizes the internal raw byte buffer
   *
   * @param[in] New size to allocate
   */
  void resize(size_t new_size);
  /**
   * Returns the pointer to the raw bytes.
   */
  uint8_t *getRawBytes()
  {
    return &buffer[0];
  }
  /**
   * Returns the size of the write buffer
   */
  size_t getCapacity()
  {
    return buffer.size();
  }
private:
  /* Prohibit these */
  WriteBuffer(const WriteBuffer&);
  WriteBuffer &operator=(const WriteBuffer&);

  std::vector<uint8_t> buffer; ///< Raw memory buffer managed by the log
  pthread_mutex_t latch; ///< Lock around the synchronized parts of the log (the write buffer)
};

