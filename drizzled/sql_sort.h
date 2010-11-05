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

#ifndef DRIZZLED_SQL_SORT_H
#define DRIZZLED_SQL_SORT_H

#include <unistd.h>
#include "drizzled/base.h"
#include "drizzled/qsort_cmp.h"

namespace drizzled
{

namespace internal
{
typedef struct st_io_cache IO_CACHE;
}

class Field;
class Table;
class SortField;


/* Defines used by filesort and uniques */

#define MERGEBUFF		7
#define MERGEBUFF2		15

/*
   The structure sort_addon_field describes a fixed layout
   for field values appended to sorted values in records to be sorted
   in the sort buffer.
   Only fixed layout is supported now.
   Null bit maps for the appended values is placed before the values
   themselves. Offsets are from the last sorted field, that is from the
   record referefence, which is still last component of sorted records.
   It is preserved for backward compatiblility.
   The structure is used tp store values of the additional fields
   in the sort buffer. It is used also when these values are read
   from a temporary file/buffer. As the reading procedures are beyond the
   scope of the 'filesort' code the values have to be retrieved via
   the callback function 'unpack_addon_fields'.
*/

class sort_addon_field {  /* Sort addon packed field */
public:
  Field *field;          /* Original field */
  uint32_t   offset;         /* Offset from the last sorted field */
  uint32_t   null_offset;    /* Offset to to null bit from the last sorted field */
  uint32_t   length;         /* Length in the sort buffer */
  uint8_t  null_bit;       /* Null bit mask for the field */

  sort_addon_field() :
    field(NULL),
    offset(0),
    null_offset(0),
    length(0),
    null_bit(0)
  { }

};

class buffpek {		/* Struktur om sorteringsbuffrarna */
public:
  off_t file_pos;			/* Where we are in the sort file */
  unsigned char *base;			/* key pointers */
  unsigned char *key;			/* key pointers */
  ha_rows count;			/* Number of rows in table */
  size_t mem_count;			/* numbers of keys in memory */
  size_t max_keys;			/* Max keys in buffert */

  buffpek() :
    file_pos(0),
    base(0),
    key(0),
    count(0),
    mem_count(0),
    max_keys(0)
  { }

};

class BUFFPEK_COMPARE_CONTEXT
{
public:
  qsort_cmp2 key_compare;
  void *key_compare_arg;
};

class sort_param {
public:
  uint32_t rec_length;          /* Length of sorted records */
  uint32_t sort_length;			/* Length of sorted columns */
  uint32_t ref_length;			/* Length of record ref. */
  uint32_t addon_length;        /* Length of added packed fields */
  uint32_t res_length;          /* Length of records in final sorted file/buffer */
  uint32_t keys;				/* Max keys / buffer */
  ha_rows max_rows,examined_rows;
  Table *sort_form;			/* For quicker make_sortkey */
  SortField *local_sortorder;
  SortField *end;
  sort_addon_field *addon_field; /* Descriptors for companion fields */
  unsigned char *unique_buff;
  bool not_killable;
  char* tmp_buffer;
  /* The fields below are used only by Unique class */
  qsort2_cmp compare;
  BUFFPEK_COMPARE_CONTEXT cmp_context;
};

typedef class sort_param SORTPARAM;

} /* namespace drizzled */

#endif /* DRIZZLED_SQL_SORT_H */
