/* Copyright (C) 2000-2002 MySQL AB
   Copyright (C) 2008 eBay, Inc

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

/* Implements various base record-related functions, such as encode and decode into chunks */

#include "heap_priv.h"

#include <drizzled/common.h>

#include <string.h>
#include <algorithm>

using namespace std;
using namespace drizzled;

/**
  Calculate size of the record for the purpose of storing in chunks

  Walk through the fields of the record and calculates the exact space
  needed in chunks as well the the total chunk count

  @param       info         the hosting table
  @param       record       the record in standard unpacked format
  @param[out]  chunk_count  the number of chunks needed for this record

  @return The size of the required storage in bytes
*/

uint32_t hp_get_encoded_data_length(HP_SHARE *info, const unsigned char *, uint32_t *chunk_count)
{
  uint32_t dst_offset= info->fixed_data_length;

  /* Nothing more to copy */
  *chunk_count= 1;
  return dst_offset;
}

bool hp_compare_record_data_to_chunkset(HP_SHARE *info, const unsigned char *record, unsigned char *pos)
{
  unsigned char* curr_chunk= pos;

  if (memcmp(curr_chunk, record, (size_t) info->fixed_data_length))
  {
    return 1;
  }

  return 0;
}

/**
  Stores record in the heap table chunks

  Copies data from original unpacked record into the preallocated chunkset

  @param  info         the hosting table
  @param  record       the record in standard unpacked format
  @param  pos          the target chunkset
*/

void hp_copy_record_data_to_chunkset(HP_SHARE *info, const unsigned char *record, unsigned char *pos)
{
  unsigned char* curr_chunk= pos;

  memcpy(curr_chunk, record, (size_t) info->fixed_data_length);
}

/**
  Copies record data from storage to unpacked record format

  Copies data from chunkset into its original unpacked record

  @param       info         the hosting table
  @param[out]  record       the target record in standard unpacked format
  @param       pos          the source chunkset
*/

void hp_extract_record(HP_SHARE *info, unsigned char *record, const unsigned char *pos)
{
  const unsigned char* curr_chunk= pos;

  memcpy(record, curr_chunk, (size_t) info->fixed_data_length);
}
