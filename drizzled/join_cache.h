/* -*- mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; -*-
 *  vim:expandtab:shiftwidth=2:tabstop=2:smarttab:
 *
 *  Copyright (C) 2008 Sun Microsystems
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; version 2 of the License.
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

#ifndef DRIZZLED_JOIN_CACHE_H
#define DRIZZLED_JOIN_CACHE_H

#include "drizzled/server_includes.h"

class Field_blob;
typedef struct st_join_table JOIN_TAB;

/**
  CACHE_FIELD and JOIN_CACHE is used on full join to cache records in outer
  table
*/
typedef struct st_cache_field {
  /*
    Where source data is located (i.e. this points to somewhere in
    tableX->record[0])
  */
  unsigned char *str;
  uint32_t length; /* Length of data at *str, in bytes */
  uint32_t blob_length; /* Valid IFF blob_field != 0 */
  Field_blob *blob_field;
  bool strip; /* true <=> Strip endspaces ?? */

  Table *get_rowid; /* _ != NULL <=> */
} CACHE_FIELD;

typedef struct st_join_cache
{
  unsigned char *buff;
  unsigned char *pos;    /* Start of free space in the buffer */
  unsigned char *end;
  uint32_t records;  /* # of row cominations currently stored in the cache */
  uint32_t record_nr;
  uint32_t ptr_record;
  /*
    Number of fields (i.e. cache_field objects). Those correspond to table
    columns, and there are also special fields for
     - table's column null bits
     - table's null-complementation byte
     - [new] table's rowid.
  */
  uint32_t fields;
  uint32_t length;
  uint32_t blobs;
  CACHE_FIELD *field;
  CACHE_FIELD **blob_ptr;
  SQL_SELECT *select;
} JOIN_CACHE;

int join_init_cache(Session *session, JOIN_TAB *tables, uint32_t table_count);
void reset_cache_read(JOIN_CACHE *cache);
void reset_cache_write(JOIN_CACHE *cache);
bool store_record_in_cache(JOIN_CACHE *cache);

#endif /* DRIZZLED_JOIN_CACHE_H */
