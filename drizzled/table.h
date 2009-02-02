/* -*- mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; -*-
 *  vim:expandtab:shiftwidth=2:tabstop=2:smarttab:
 *
 *  Copyright (C) 2008 Sun Microsystems
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

/* Structs that defines the Table */

#ifndef DRIZZLED_TABLE_H
#define DRIZZLED_TABLE_H

#include <storage/myisam/myisam.h>
#include <drizzled/order.h>
#include <drizzled/filesort_info.h>
#include <drizzled/natural_join_column.h>
#include <drizzled/field_iterator.h>
#include <mysys/hash.h>
#include <drizzled/handler.h>
#include <drizzled/lex_string.h>
#include <drizzled/table_list.h>
#include <drizzled/table_share.h>

class Item;
class Item_subselect;
class st_select_lex_unit;
class st_select_lex;
class COND_EQUAL;
class Security_context;
class TableList;

/*************************************************************************/


class Field_timestamp;
class Field_blob;

typedef enum enum_table_category TABLE_CATEGORY;

TABLE_CATEGORY get_table_category(const LEX_STRING *db,
                                  const LEX_STRING *name);


extern uint32_t refresh_version;

typedef struct st_table_field_w_type
{
  LEX_STRING name;
  LEX_STRING type;
  LEX_STRING cset;
} TABLE_FIELD_W_TYPE;

bool create_myisam_from_heap(Session *session, Table *table,
                             MI_COLUMNDEF *start_recinfo,
                             MI_COLUMNDEF **recinfo,
                             int error, bool ignore_last_dupp_key_error);

class Table {

public:
  TABLE_SHARE	*s;
  Table() {}                               /* Remove gcc warning */

  /* SHARE methods */
  inline TABLE_SHARE *getShare() { return s; } /* Get rid of this long term */
  inline void setShare(TABLE_SHARE *new_share) { s= new_share; } /* Get rid of this long term */
  inline uint32_t sizeKeys() { return s->keys; }
  inline uint32_t sizeFields() { return s->fields; }
  inline uint32_t getRecordLength() { return s->reclength; }
  inline uint32_t sizeBlobFields() { return s->blob_fields; }
  inline uint32_t *getBlobField() { return s->blob_field; }
  inline uint32_t getNullBytes() { return s->null_bytes; }
  inline uint32_t getNullFields() { return s->null_fields; }
  inline unsigned char *getDefaultValues() { return s->default_values; }

  inline bool isNullFieldFirst() { return s->null_field_first; }
  inline bool isDatabaseLowByteFirst() { return s->db_low_byte_first; }		/* Portable row format */
  inline bool isCrashed() { return s->crashed; }
  inline bool isNameLock() { return s->name_lock; }
  inline bool isReplaceWithNameLock() { return s->replace_with_name_lock; }
  inline bool isWaitingOnCondition() { return s->waiting_on_cond; }                 /* Protection against free */

  /* For TMP tables, should be pulled out as a class */
  void updateCreateInfo(HA_CREATE_INFO *create_info);
  void setup_tmp_table_column_bitmaps(unsigned char *bitmaps);
  bool create_myisam_tmp_table(KEY *keyinfo,
                               MI_COLUMNDEF *start_recinfo,
                               MI_COLUMNDEF **recinfo,
                               uint64_t options);
  void free_tmp_table(Session *session);
  bool open_tmp_table();
  size_t max_row_length(const unsigned char *data);
  uint32_t find_shortest_key(const key_map *usable_keys);
  bool compare_record(Field **ptr);
  bool compare_record();

  bool table_check_intact(const uint32_t table_f_count, const TABLE_FIELD_W_TYPE *table_def);

  /* See if this can be blown away */
  inline uint32_t getDBStat () { return db_stat; }
  inline uint32_t setDBStat () { return db_stat; }
  uint		db_stat;		/* mode of file as in handler.h */

  handler	*file;
  Table *next, *prev;

  Session	*in_use;                        /* Which thread uses this */
  Field **field;			/* Pointer to fields */

  unsigned char *record[2];			/* Pointer to records */
  unsigned char *write_row_record;		/* Used as optimisation in
					   Session::write_row */
  unsigned char *insert_values;                  /* used by INSERT ... UPDATE */
  /*
    Map of keys that can be used to retrieve all data from this table
    needed by the query without reading the row.
  */
  key_map covering_keys;
  key_map quick_keys, merge_keys;
  /*
    A set of keys that can be used in the query that references this
    table.

    All indexes disabled on the table's TABLE_SHARE (see Table::s) will be
    subtracted from this set upon instantiation. Thus for any Table t it holds
    that t.keys_in_use_for_query is a subset of t.s.keys_in_use. Generally we
    must not introduce any new keys here (see setup_tables).

    The set is implemented as a bitmap.
  */
  key_map keys_in_use_for_query;
  /* Map of keys that can be used to calculate GROUP BY without sorting */
  key_map keys_in_use_for_group_by;
  /* Map of keys that can be used to calculate ORDER BY without sorting */
  key_map keys_in_use_for_order_by;
  KEY  *key_info;			/* data of keys in database */

  Field *next_number_field;		/* Set if next_number is activated */
  Field *found_next_number_field;	/* Set on open */
  Field_timestamp *timestamp_field;
  Field **vfield;                       /* Pointer to virtual fields*/

  TableList *pos_in_table_list;/* Element referring to this table */
  order_st *group;
  const char	*alias;            	  /* alias or table name */
  unsigned char		*null_flags;
  my_bitmap_map	*bitmap_init_value;
  MY_BITMAP     def_read_set, def_write_set, tmp_set; /* containers */
  MY_BITMAP     *read_set, *write_set;          /* Active column sets */
  /*
   The ID of the query that opened and is using this table. Has different
   meanings depending on the table type.

   Temporary tables:

   table->query_id is set to session->query_id for the duration of a statement
   and is reset to 0 once it is closed by the same statement. A non-zero
   table->query_id means that a statement is using the table even if it's
   not the current statement (table is in use by some outer statement).

   Non-temporary tables:

   Under pre-locked or LOCK TABLES mode: query_id is set to session->query_id
   for the duration of a statement and is reset to 0 once it is closed by
   the same statement. A non-zero query_id is used to control which tables
   in the list of pre-opened and locked tables are actually being used.
  */
  query_id_t	query_id;

  /*
    For each key that has quick_keys.is_set(key) == true: estimate of #records
    and max #key parts that range access would use.
  */
  ha_rows	quick_rows[MAX_KEY];

  /* Bitmaps of key parts that =const for the entire join. */
  key_part_map  const_key_parts[MAX_KEY];

  uint		quick_key_parts[MAX_KEY];
  uint		quick_n_ranges[MAX_KEY];

  /*
    Estimate of number of records that satisfy SARGable part of the table
    condition, or table->file->records if no SARGable condition could be
    constructed.
    This value is used by join optimizer as an estimate of number of records
    that will pass the table condition (condition that depends on fields of
    this table and constants)
  */
  ha_rows       quick_condition_rows;

  /*
    If this table has TIMESTAMP field with auto-set property (pointed by
    timestamp_field member) then this variable indicates during which
    operations (insert only/on update/in both cases) we should set this
    field to current timestamp. If there are no such field in this table
    or we should not automatically set its value during execution of current
    statement then the variable contains TIMESTAMP_NO_AUTO_SET (i.e. 0).

    Value of this variable is set for each statement in open_table() and
    if needed cleared later in statement processing code (see mysql_update()
    as example).
  */
  timestamp_auto_set_type timestamp_field_type;
  table_map	map;                    /* ID bit of table (1,2,4,8,16...) */

  uint32_t          lock_position;          /* Position in DRIZZLE_LOCK.table */
  uint32_t          lock_data_start;        /* Start pos. in DRIZZLE_LOCK.locks */
  uint32_t          lock_count;             /* Number of locks */
  uint		tablenr,used_fields;
  uint32_t          temp_pool_slot;		/* Used by intern temp tables */
  uint		status;                 /* What's in record[0] */
  /* number of select if it is derived table */
  uint32_t          derived_select_number;
  int		current_lock;           /* Type of lock on table */
  bool copy_blobs;			/* copy_blobs when storing */

  /*
    0 or JOIN_TYPE_{LEFT|RIGHT}. Currently this is only compared to 0.
    If maybe_null !=0, this table is inner w.r.t. some outer join operation,
    and null_row may be true.
  */
  bool maybe_null;

  /*
    If true, the current table row is considered to have all columns set to
    NULL, including columns declared as "not null" (see maybe_null).
  */
  bool null_row;

  bool force_index;
  bool distinct,const_table,no_rows;
  bool key_read, no_keyread;
  /*
    Placeholder for an open table which prevents other connections
    from taking name-locks on this table. Typically used with
    TABLE_SHARE::version member to take an exclusive name-lock on
    this table name -- a name lock that not only prevents other
    threads from opening the table, but also blocks other name
    locks. This is achieved by:
    - setting open_placeholder to 1 - this will block other name
      locks, as wait_for_locked_table_name will be forced to wait,
      see table_is_used for details.
    - setting version to 0 - this will force other threads to close
      the instance of this table and wait (this is the same approach
      as used for usual name locks).
    An exclusively name-locked table currently can have no handler
    object associated with it (db_stat is always 0), but please do
    not rely on that.
  */
  bool open_placeholder;
  bool locked_by_logger;
  bool no_replicate;
  bool locked_by_name;
  bool no_cache;
  /* To signal that the table is associated with a HANDLER statement */
  bool open_by_handler;
  /*
    To indicate that a non-null value of the auto_increment field
    was provided by the user or retrieved from the current record.
    Used only in the MODE_NO_AUTO_VALUE_ON_ZERO mode.
  */
  bool auto_increment_field_not_null;
  bool insert_or_update;             /* Can be used by the handler */
  bool alias_name_used;		/* true if table_name is alias */
  bool get_fields_in_item_tree;      /* Signal to fix_field */

  REGINFO reginfo;			/* field connections */
  MEM_ROOT mem_root;
  filesort_info_st sort;

  bool fill_item_list(List<Item> *item_list) const;
  void reset_item_list(List<Item> *item_list) const;
  void clear_column_bitmaps(void);
  void prepare_for_position(void);
  void mark_columns_used_by_index_no_reset(uint32_t index, MY_BITMAP *map);
  void mark_columns_used_by_index(uint32_t index);
  void restore_column_maps_after_mark_index();
  void mark_auto_increment_column(void);
  void mark_columns_needed_for_update(void);
  void mark_columns_needed_for_delete(void);
  void mark_columns_needed_for_insert(void);
  void mark_virtual_columns(void);
  inline void column_bitmaps_set(MY_BITMAP *read_set_arg,
                                 MY_BITMAP *write_set_arg)
  {
    read_set= read_set_arg;
    write_set= write_set_arg;
    if (file)
      file->column_bitmaps_signal();
  }
  inline void column_bitmaps_set_no_signal(MY_BITMAP *read_set_arg,
                                           MY_BITMAP *write_set_arg)
  {
    read_set= read_set_arg;
    write_set= write_set_arg;
  }

  void restore_column_map(my_bitmap_map *old);

  my_bitmap_map *use_all_columns(MY_BITMAP *bitmap);
  inline void use_all_columns()
  {
    column_bitmaps_set(&s->all_set, &s->all_set);
  }

  inline void default_column_bitmaps()
  {
    read_set= &def_read_set;
    write_set= &def_write_set;
  }

  /* Is table open or should be treated as such by name-locking? */
  inline bool is_name_opened() { return db_stat || open_placeholder; }
  /*
    Is this instance of the table should be reopen or represents a name-lock?
  */
  inline bool needs_reopen_or_name_lock()
  { return s->version != refresh_version; }

  int report_error(int error);
  int closefrm(bool free_share);
  uint32_t tmpkeyval();
};

Table *create_virtual_tmp_table(Session *session,
                                List<Create_field> &field_list);

typedef struct st_foreign_key_info
{
  LEX_STRING *forein_id;
  LEX_STRING *referenced_db;
  LEX_STRING *referenced_table;
  LEX_STRING *update_method;
  LEX_STRING *delete_method;
  LEX_STRING *referenced_key_name;
  List<LEX_STRING> foreign_fields;
  List<LEX_STRING> referenced_fields;
} FOREIGN_KEY_INFO;

typedef struct st_field_info
{
  /**
      This is used as column name.
  */
  const char* field_name;
  /**
     For string-type columns, this is the maximum number of
     characters. Otherwise, it is the 'display-length' for the column.
  */
  uint32_t field_length;
  /**
     This denotes data type for the column. For the most part, there seems to
     be one entry in the enum for each SQL data type, although there seem to
     be a number of additional entries in the enum.
  */
  enum enum_field_types field_type;
  int value;
  /**
     This is used to set column attributes. By default, columns are @c NOT
     @c NULL and @c SIGNED, and you can deviate from the default
     by setting the appopriate flags. You can use either one of the flags
     @c MY_I_S_MAYBE_NULL and @cMY_I_S_UNSIGNED or
     combine them using the bitwise or operator @c |. Both flags are
     defined in table.h.
   */
  uint32_t field_flags;        // Field atributes(maybe_null, signed, unsigned etc.)
  const char* old_name;
  /**
     This should be one of @c SKIP_OPEN_TABLE,
     @c OPEN_FRM_ONLY or @c OPEN_FULL_TABLE.
  */
  uint32_t open_method;
} ST_FIELD_INFO;


class TableList;
typedef class Item COND;

struct ST_SCHEMA_TABLE
{
  const char* table_name;
  ST_FIELD_INFO *fields_info;
  /* Create information_schema table */
  Table *(*create_table)  (Session *session, TableList *table_list);
  /* Fill table with data */
  int (*fill_table) (Session *session, TableList *tables, COND *cond);
  /* Handle fileds for old SHOW */
  int (*old_format) (Session *session, struct ST_SCHEMA_TABLE *schema_table);
  int (*process_table) (Session *session, TableList *tables, Table *table,
                        bool res, LEX_STRING *db_name, LEX_STRING *table_name);
  int idx_field1, idx_field2;
  bool hidden;
  uint32_t i_s_requested_object;  /* the object we need to open(Table | VIEW) */
};


#define JOIN_TYPE_LEFT	1
#define JOIN_TYPE_RIGHT	2

struct st_lex;
class select_union;
class TMP_TABLE_PARAM;

typedef struct st_changed_table_list
{
  struct	st_changed_table_list *next;
  char		*key;
  uint32_t        key_length;
} CHANGED_TableList;


typedef struct st_open_table_list
{
  struct st_open_table_list *next;
  char	*db,*table;
  uint32_t in_use,locked;
} OPEN_TableList;


inline void mark_as_null_row(Table *table)
{
  table->null_row=1;
  table->status|=STATUS_NULL_ROW;
  memset(table->null_flags, 255, table->s->null_bytes);
}

/**
  clean/setup table fields and map.

  @param table        Table structure pointer (which should be setup)
  @param table_list   TableList structure pointer (owner of Table)
  @param tablenr     table number
*/
void setup_table_map(Table *table, TableList *table_list, uint32_t tablenr);

#endif /* DRIZZLED_TABLE_H */
