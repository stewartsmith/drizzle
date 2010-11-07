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

/* Implements various base dataspace-related functions - allocate, free, clear */

#include "heap_priv.h"

#include <cassert>


/*
  MySQL Heap tables keep data in arrays of fixed-size chunks.
  These chunks are organized into two groups of HP_BLOCK structures:
    - group1 contains indexes, with one HP_BLOCK per key
      (part of HP_KEYDEF)
    - group2 contains record data, with single HP_BLOCK
      for all records, referenced by HP_SHARE.recordspace.block

  While columns used in index are usually small, other columns
  in the table may need to accomodate larger data. Typically,
  larger data is placed into VARCHAR or BLOB columns. With actual
  sizes varying, Heap Engine has to support variable-sized records
  in memory. Heap Engine implements the concept of dataspace
  (HP_DATASPACE), which incorporates HP_BLOCK for the record data,
  and adds more information for managing variable-sized records.

  Variable-size records are stored in multiple "chunks",
  which means that a single record of data (database "row") can
  consist of multiple chunks organized into one "set". HP_BLOCK
  contains chunks. In variable-size format, one record
  is represented as one or many chunks, depending on the actual
  data, while in fixed-size mode, one record is always represented
  as one chunk. The index structures would always point to the first
  chunk in the chunkset.

  At the time of table creation, Heap Engine attempts to find out
  if variable-size records are desired. A user can request
  variable-size records by providing either row_type=dynamic or
  block_size=NNN table create option. Heap Engine will check
  whether block_size provides enough space in the first chunk
  to keep all null bits and columns that are used in indexes.
  If block_size is too small, table creation will be aborted
  with an error. Heap Engine will revert to fixed-size allocation
  mode if block_size provides no memory benefits (no VARCHAR
  fields extending past first chunk).

  In order to improve index search performance, Heap Engine needs
  to keep all null flags and all columns used as keys inside
  the first chunk of a chunkset. In particular, this means that
  all columns used as keys should be defined first in the table
  creation SQL. The length of data used by null bits and key columns
  is stored as fixed_data_length inside HP_SHARE. fixed_data_length
  will extend past last key column if more fixed-length fields can
  fit into the first chunk.

  Variable-size records are necessary only in the presence
  of variable-size columns. Heap Engine will be looking for VARCHAR
  columns, which declare length of 32 or more. If no such columns
  are found, table will be switched to fixed-size format. You should
  always try to put such columns at the end of the table definition.

  Whenever data is being inserted or updated in the table
  Heap Engine will calculate how many chunks are necessary.
  For insert operations, Heap Engine allocates new chunkset in
  the recordspace. For update operations it will modify length of
  the existing chunkset, unlinking unnecessary chunks at the end,
  or allocating and adding more if larger length is necessary.

  When writing data to chunks or copying data back to record,
  Heap Engine will first copy fixed_data_length of data using single
  memcpy call. The rest of the columns are processed one-by-one.
  Non-VARCHAR columns are copied in their full format. VARCHAR's
  are copied based on their actual length. Any NULL values after
  fixed_data_length are skipped.

  The allocation and contents of the actual chunks varies between
  fixed and variable-size modes. Total chunk length is always
  aligned to the next sizeof(unsigned char*). Here is the format of
  fixed-size chunk:
      unsigned char[] - sizeof=chunk_dataspace_length, but at least
               sizeof(unsigned char*) bytes. Keeps actual data or pointer
               to the next deleted chunk.
               chunk_dataspace_length equals to full record length
      unsigned char   - status field (1 means "in use", 0 means "deleted")
  Variable-size uses different format:
      unsigned char[] - sizeof=chunk_dataspace_length, but at least
               sizeof(unsigned char*) bytes. Keeps actual data or pointer
               to the next deleted chunk.
               chunk_dataspace_length is set according to table
               setup (block_size)
      unsigned char*  - pointer to the next chunk in this chunkset,
               or NULL for the last chunk
      unsigned char  -  status field (1 means "first", 0 means "deleted",
               2 means "linked")

  When allocating a new chunkset of N chunks, Heap Engine will try
  to allocate chunks one-by-one, linking them as they become
  allocated. Allocation of a single chunk will attempt to reuse
  a deleted (freed) chunk. If no free chunks are available,
  it will attempt to allocate a new area inside HP_BLOCK.
  Freeing chunks will place them at the front of free list
  referenced by del_link in HP_DATASPACE. The newly freed chunk
  will contain reference to the previously freed chunk in its first
  sizeof(unsigned char*) of the payload space.

  Here is open issues:
    1. It is not very nice to require people to keep key columns
       at the beginning of the table creation SQL. There are three
       proposed resolutions:
       a. Leave it as is. It's a reasonable limitation
       b. Add new HA_KEEP_KEY_COLUMNS_TO_FRONT flag to handler.h and
          make table.cpp align columns when it creates the table
       c. Make HeapEngine reorder columns in the chunk data, so that
          key columns go first. Add parallel HA_KEYSEG structures
          to distinguish positions in record vs. positions in
          the first chunk. Copy all data field-by-field rather than
          using single memcpy unless DBA kept key columns to
          the beginning.
    2. heap_check_heap needs verify linked chunks, looking for
       issues such as orphans, cycles, and bad links. However,
       Heap Engine today does not do similar things even for
       free list.
    3. With new HP_DATASPACE allocation mechaism, BLOB will become
       increasingly simple to implement, but I may not have time
       for that. In one approach, BLOB data can be placed at
       the end of the same record. In another approach (which I
       prefer) BLOB data would have its own HP_DATASPACE with
       variable-size entries.
    4. In a more sophisticated implementation, some space can
       be saved even with all fixed-size columns if many of them
       have NULL value, as long as these columns are not used
       in indexes
    5. In variable-size format status should be moved to lower
       bits of the "next" pointer. Pointer is always aligned
       to sizeof(unsigned char*), which is at least 4, leaving 2 lower
       bits free. This will save 8 bytes per chunk
       on 64-bit platform.
    6. As we do not want to modify FRM format, BLOCK_SIZE option
       of "CREATE TABLE" is saved as "RAID_CHUNKSIZE" for
       Heap Engine tables.
*/

static unsigned char *hp_allocate_one_chunk(HP_DATASPACE *info);


/**
  Clear a dataspace

  Frees memory and zeros-out any relevant counters in the dataspace

  @param  info  the dataspace to clear
*/

void hp_clear_dataspace(HP_DATASPACE *info)
{
  if (info->block.levels)
  {
    hp_free_level(&info->block,info->block.levels,info->block.root,
                  (unsigned char*) 0);
  }
  info->block.levels=0;
  info->del_chunk_count= info->chunk_count= 0;
  info->del_link=0;
  info->total_data_length= 0;
}


/**
  Allocate a new chunkset in the dataspace

  Attempts to allocate a new chunkset

  @param  info            the hosting dataspace
  @param  chunk_count     the number of chunks that we expect as the result

  @return  Pointer to the first chunk in the new or updated chunkset, or NULL if unsuccessful
*/

unsigned char *hp_allocate_chunkset(HP_DATASPACE *info, uint32_t )
{
  unsigned char* result;

  result= hp_allocate_one_chunk(info);
  if (result)
  {
    result[info->offset_status]= CHUNK_STATUS_ACTIVE;
  }

  return(result);
}


/**
  Allocate a single chunk in the dataspace

  Attempts to allocate a new chunk or reuse one from deleted list

  @param  info            the hosting dataspace

  @return  Pointer to the chunk, or NULL if unsuccessful
*/

static unsigned char *hp_allocate_one_chunk(HP_DATASPACE *info)
{
  unsigned char* curr_chunk;
  size_t length, block_pos;

  if (info->del_link)
  {
    curr_chunk=info->del_link;
    info->del_link= *((unsigned char**) curr_chunk);
    info->del_chunk_count--;

    return curr_chunk;
  }

  block_pos= (info->chunk_count % info->block.records_in_block);
  if (!block_pos)
  {
    if (hp_get_new_block(&info->block,&length))
    {
      /* no space in the current block */
      return NULL;
    }

    info->total_data_length+= length;
  }

  info->chunk_count++;
  curr_chunk= ((unsigned char*) info->block.level_info[0].last_blocks +
               block_pos * info->block.recbuffer);


  return curr_chunk;
}


/**
  Free a list of chunks

  Reclaims all chunks linked by the pointer,
  which could be the whole chunkset or a part of an existing chunkset

  @param  info            the hosting dataspace
  @param  pos             pointer to the head of the chunkset
*/

void hp_free_chunks(HP_DATASPACE *info, unsigned char *pos)
{
  unsigned char* curr_chunk= pos;

  if (curr_chunk) 
  {
    info->del_chunk_count++;
    *((unsigned char**) curr_chunk)= info->del_link;
    info->del_link= curr_chunk;

    curr_chunk[info->offset_status]= CHUNK_STATUS_DELETED;
  }
}
