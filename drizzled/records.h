/* -*- mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; -*-
 *  vim:expandtab:shiftwidth=2:tabstop=2:smarttab:
 *
 *  Copyright (C) 2009 Sun Microsystems, Inc.
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

#pragma once

#include <drizzled/common_fwd.h>

namespace drizzled {

 
struct ReadRecord {			/* Parameter to read_record */
  Table *table;			/* Head-form */
  Cursor *cursor;
  Table **forms;			/* head and ref forms */
  int (*read_record)(ReadRecord *);
  Session *session;
  optimizer::SqlSelect *select;
  uint32_t cache_records;
  uint32_t ref_length;
  uint32_t struct_length;
  uint32_t reclength;
  uint32_t rec_cache_size;
  uint32_t error_offset;
  uint32_t index;
  unsigned char *ref_pos;				/* pointer to form->refpos */
  unsigned char *record;
  unsigned char *rec_buf;                /* to read field values  after filesort */
private:
  unsigned char	*cache;
public:
  unsigned char *getCache()
  {
    return cache;
  }
  unsigned char *cache_pos;
  unsigned char *cache_end;
  unsigned char *read_positions;
  internal::io_cache_st *io_cache;
  bool print_error;
  bool ignore_not_found_rows;
  JoinTable *do_insideout_scan;
  ReadRecord() :
    table(NULL),
    cursor(NULL),
    forms(0),
    read_record(0),
    session(0),
    select(0),
    cache_records(0),
    ref_length(0),
    struct_length(0),
    reclength(0),
    rec_cache_size(0),
    error_offset(0),
    index(0),
    ref_pos(0),
    record(0),
    rec_buf(0),
    cache(0),
    cache_pos(0),
    cache_end(0),
    read_positions(0),
    io_cache(0),
    print_error(0),
    ignore_not_found_rows(0),
    do_insideout_scan(0)
  {
  }

  void init()
  {
    table= NULL;
    cursor= NULL;
    forms= 0;
    read_record= 0;
    session= 0;
    select= 0;
    cache_records= 0;
    ref_length= 0;
    struct_length= 0;
    reclength= 0;
    rec_cache_size= 0;
    error_offset= 0;
    index= 0;
    ref_pos= 0;
    record= 0;
    rec_buf= 0;
    cache= 0;
    cache_pos= 0;
    cache_end= 0;
    read_positions= 0;
    io_cache= 0;
    print_error= 0;
    ignore_not_found_rows= 0;
    do_insideout_scan= 0;
  }

  virtual ~ReadRecord()
  { }

/*
  init_read_record is used to scan by using a number of different methods.
  Which method to use is set-up in this call so that later calls to
  the info->read_record will call the appropriate method using a function
  pointer.

  There are five methods that relate completely to the sort function
  filesort. The result of a filesort is retrieved using read_record
  calls. The other two methods are used for normal table access.

  The filesort will produce references to the records sorted, these
  references can be stored in memory or in a temporary cursor.

  The temporary cursor is normally used when the references doesn't fit into
  a properly sized memory buffer. For most small queries the references
  are stored in the memory buffer.

  The temporary cursor is also used when performing an update where a key is
  modified.

  Methods used when ref's are in memory (using rr_from_pointers):
    rr_unpack_from_buffer:
    ----------------------
      This method is used when table->sort.addon_field is allocated.
      This is allocated for most SELECT queries not involving any BLOB's.
      In this case the records are fetched from a memory buffer.
    rr_from_pointers:
    -----------------
      Used when the above is not true, UPDATE, DELETE and so forth and
      SELECT's involving BLOB's. It is also used when the addon_field
      buffer is not allocated due to that its size was bigger than the
      session variable max_length_for_sort_data.
      In this case the record data is fetched from the handler using the
      saved reference using the rnd_pos handler call.

  Methods used when ref's are in a temporary cursor (using rr_from_tempfile)
    rr_unpack_from_tempfile:
    ------------------------
      Same as rr_unpack_from_buffer except that references are fetched from
      temporary cursor. Should obviously not really happen other than in
      strange configurations.

    rr_from_tempfile:
    -----------------
      Same as rr_from_pointers except that references are fetched from
      temporary cursor instead of from
    rr_from_cache:
    --------------
      This is a special variant of rr_from_tempfile that can be used for
      handlers that is not using the HA_FAST_KEY_READ table flag. Instead
      of reading the references one by one from the temporary cursor it reads
      a set of them, sorts them and reads all of them into a buffer which
      is then used for a number of subsequent calls to rr_from_cache.
      It is only used for SELECT queries and a number of other conditions
      on table size.

  All other accesses use either index access methods (rr_quick) or a full
  table scan (rr_sequential).
  rr_quick:
  ---------
    rr_quick uses one of the QUICK_SELECT classes in optimizer/range.cc to
    perform an index scan. There are loads of functionality hidden
    in these quick classes. It handles all index scans of various kinds.
  rr_sequential:
  --------------
    This is the most basic access method of a table using rnd_init,
    rnd_next and rnd_end. No indexes are used.
*/
  int init_read_record(Session *session,
                       Table *reg_form,
                       optimizer::SqlSelect *select,
                       int use_record_cache,
                       bool print_errors) __attribute__ ((warn_unused_result));

  void end_read_record();


/**
  Initialize ReadRecord structure to perform full index scan (in forward
  direction) using read_record.read_record() interface.

    This function has been added at late stage and is used only by
    UPDATE/DELETE. Other statements perform index scans using
    join_read_first/next functions.

  @param info         ReadRecord structure to initialize.
  @param session          Thread handle
  @param table        Table to be accessed
  @param print_error  If true, call table->print_error() if an error
                      occurs (except for end-of-records error)
  @param idx          index to scan
                    */
  int init_read_record_idx(Session *session,
                           Table *table,
                           bool print_error,
                           uint32_t idx) __attribute__ ((warn_unused_result));

  void init_reard_record_sequential();

  bool init_rr_cache();
};

} /* namespace drizzled */

