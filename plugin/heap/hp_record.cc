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
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA */

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

uint32_t hp_get_encoded_data_length(HP_SHARE *info, const unsigned char *record, uint32_t *chunk_count)
{
  uint32_t dst_offset= info->fixed_data_length;

  if (!info->recordspace.is_variable_size)
  {
    /* Nothing more to copy */
    *chunk_count= 1;
    return dst_offset;
  }

  for (uint32_t i= info->fixed_column_count; i < info->column_count; i++)
  {
    uint32_t src_offset, length;

    HP_COLUMNDEF* column= info->column_defs + i;

    if (column->null_bit)
    {
      if (record[column->null_pos] & column->null_bit)
      {
        /* Skip all NULL values */
        continue;
      }
    }

    src_offset= column->offset;
    if (column->type == DRIZZLE_TYPE_VARCHAR)
    {
      uint32_t pack_length;

      /* >= 5.0.3 true VARCHAR */

      pack_length= column->length_bytes;
      length= pack_length + (pack_length == 1 ?
        (uint) *(unsigned char*) (record + src_offset) : uint2korr(record + src_offset));
    }
    else
    {
      length= column->length;
    }

    dst_offset+= length;
  }

  *chunk_count= get_chunk_count(&info->recordspace, dst_offset);

  return dst_offset;
}


/**
  Encodes or compares record

  Copies data from original unpacked record into the preallocated chunkset,
  or performs data comparison

  @param  info         the hosting table
  @param  record       the record in standard unpacked format
  @param  pos          the target chunkset
  @param  is_compare   flag indicating whether we should compare data or store it

  @return  Status of comparison
    @retval  non-zero  if comparison fond data differences
    @retval  zero      otherwise
*/

uint32_t hp_process_record_data_to_chunkset(HP_SHARE *info, const unsigned char *record,
                                      unsigned char *pos, uint32_t is_compare)
{
  uint32_t dst_offset;
  unsigned char* curr_chunk= pos;

  if (is_compare)
  {
    if (memcmp(curr_chunk, record, (size_t) info->fixed_data_length))
    {
      return 1;
    }
  }
  else
  {
    memcpy(curr_chunk, record, (size_t) info->fixed_data_length);
  }

  if (!info->recordspace.is_variable_size)
  {
    /* Nothing more to copy */
    return 0;
  }

  dst_offset= info->fixed_data_length;

  for (uint32_t i= info->fixed_column_count; i < info->column_count; i++)
  {
    uint32_t src_offset, length;

    HP_COLUMNDEF* column= info->column_defs + i;

    if (column->null_bit)
    {
      if (record[column->null_pos] & column->null_bit)
      {
        /* Skip all NULL values */
        continue;
      }
    }

    src_offset= column->offset;
    if (column->type == DRIZZLE_TYPE_VARCHAR)
    {
      uint32_t pack_length;

      /* >= 5.0.3 true VARCHAR */

      /* Make sure to copy length indicator and actuals string bytes */
      pack_length= column->length_bytes;
      length= pack_length + (pack_length == 1 ?
        (uint) *(unsigned char*) (record + src_offset) : uint2korr(record + src_offset));
    }
    else
    {
      length= column->length;
    }

    while (length > 0)
    {
      uint32_t to_copy;

      to_copy= info->recordspace.chunk_dataspace_length - dst_offset;
      if (to_copy == 0)
      {
        /* Jump to the next chunk */
        /*dump_chunk(info, curr_chunk);*/
        curr_chunk= *((unsigned char**) (curr_chunk + info->recordspace.offset_link));
        dst_offset= 0;
        continue;
      }

      to_copy= min(length, to_copy);

      if (is_compare)
      {
        if (memcmp(curr_chunk + dst_offset, record + src_offset, (size_t) to_copy))
        {
          return 1;
        }
      }
      else
      {
        memcpy(curr_chunk + dst_offset, record + src_offset, (size_t) to_copy);
      }

      src_offset+= to_copy;
      dst_offset+= to_copy;
      length-= to_copy;
    }
  }

  /*dump_chunk(info, curr_chunk);*/
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

  hp_process_record_data_to_chunkset(info, record, pos, 0);

  return;
}


/*
  Macro to switch curr_chunk to the next chunk in the chunkset and reset src_offset
*/
#define SWITCH_TO_NEXT_CHUNK_FOR_READ(info, curr_chunk, src_offset) \
      { \
        curr_chunk= *((unsigned char**) (curr_chunk + info->recordspace.offset_link)); \
        src_offset= 0; \
        /*dump_chunk(info, curr_chunk);*/ \
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
  uint32_t src_offset;
  const unsigned char* curr_chunk= pos;


  /*if (info->is_variable_size)
  {
    dump_chunk(info, curr_chunk);
  }*/

  memcpy(record, curr_chunk, (size_t) info->fixed_data_length);

  if (not info->recordspace.is_variable_size)
  {
    /* Nothing more to copy */
    return;
  }

  src_offset= info->fixed_data_length;

  for (uint32_t i= info->fixed_column_count; i < info->column_count; i++)
  {
    uint32_t dst_offset, length, is_null = 0;

    HP_COLUMNDEF* column= info->column_defs + i;

    if (column->null_bit)
    {
      if (record[column->null_pos] & column->null_bit)
      {
        is_null = 1;
      }
    }

    dst_offset= column->offset;
    if (column->type == DRIZZLE_TYPE_VARCHAR)
    {
      uint32_t pack_length, byte1, byte2;

      /* >= 5.0.3 true VARCHAR */

      if (is_null)
      {
        /* TODO: is memset really needed? */
        memset(record + column->offset, 0, column->length);
        continue;
      }

      pack_length= column->length_bytes;

      if (src_offset == info->recordspace.chunk_dataspace_length)
      {
        SWITCH_TO_NEXT_CHUNK_FOR_READ(info, curr_chunk, src_offset);
      }
      byte1= *(unsigned char*) (curr_chunk + src_offset++);
      *(record + dst_offset++)= byte1;

      if (pack_length == 1)
      {
        length= byte1;
      }
      else
      {
        if (src_offset == info->recordspace.chunk_dataspace_length)
        {
          SWITCH_TO_NEXT_CHUNK_FOR_READ(info, curr_chunk, src_offset);
        }
        byte2= *(unsigned char*) (curr_chunk + src_offset++);
        *(record + dst_offset++)= byte2;

        /* We copy byte-by-byte and then use uint2korr to combine bytes in the right order */
        length= uint2korr(record + dst_offset - 2);
      }
    }
    else
    {
      if (is_null)
      {
        /* TODO: is memset really needed? */
        memset(record + column->offset, 0, column->length);
        continue;
      }

      length= column->length;
    }

    while (length > 0)
    {
      uint32_t to_copy;

      to_copy= info->recordspace.chunk_dataspace_length - src_offset;
      if (to_copy == 0)
      {
        SWITCH_TO_NEXT_CHUNK_FOR_READ(info, curr_chunk, src_offset);
        to_copy= info->recordspace.chunk_dataspace_length;
      }

      to_copy= min(length, to_copy);

      memcpy(record + dst_offset, curr_chunk + src_offset, (size_t) to_copy);
      src_offset+= to_copy;
      dst_offset+= to_copy;
      length-= to_copy;
    }
  }
}
