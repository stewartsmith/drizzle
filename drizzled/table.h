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

class Item;				/* Needed by order_st */
class Item_subselect;
class st_select_lex_unit;
class st_select_lex;
class COND_EQUAL;
class Security_context;
class TableList;

/*************************************************************************/

enum tmp_table_type
{
  NO_TMP_TABLE, NON_TRANSACTIONAL_TMP_TABLE, TRANSACTIONAL_TMP_TABLE,
  INTERNAL_TMP_TABLE, SYSTEM_TMP_TABLE, TMP_TABLE_FRM_FILE_ONLY
};

bool mysql_frm_type(Session *session, char *path, enum legacy_db_type *dbt);


enum release_type { RELEASE_NORMAL, RELEASE_WAIT_FOR_DROP };

/*
  Values in this enum are used to indicate how a tables TIMESTAMP field
  should be treated. It can be set to the current timestamp on insert or
  update or both.
  WARNING: The values are used for bit operations. If you change the
  enum, you must keep the bitwise relation of the values. For example:
  (int) TIMESTAMP_AUTO_SET_ON_BOTH must be equal to
  (int) TIMESTAMP_AUTO_SET_ON_INSERT | (int) TIMESTAMP_AUTO_SET_ON_UPDATE.
  We use an enum here so that the debugger can display the value names.
*/
enum timestamp_auto_set_type
{
  TIMESTAMP_NO_AUTO_SET= 0, TIMESTAMP_AUTO_SET_ON_INSERT= 1,
  TIMESTAMP_AUTO_SET_ON_UPDATE= 2, TIMESTAMP_AUTO_SET_ON_BOTH= 3
};
#define clear_timestamp_auto_bits(_target_, _bits_) \
  (_target_)= (enum timestamp_auto_set_type)((int)(_target_) & ~(int)(_bits_))

class Field_timestamp;
class Field_blob;

/**
  Category of table found in the table share.
*/
enum enum_table_category
{
  /**
    Unknown value.
  */
  TABLE_UNKNOWN_CATEGORY=0,

  /**
    Temporary table.
    The table is visible only in the session.
    Therefore,
    - FLUSH TABLES WITH READ LOCK
    - SET GLOBAL READ_ONLY = ON
    do not apply to this table.
    Note that LOCK Table t FOR READ/WRITE
    can be used on temporary tables.
    Temporary tables are not part of the table cache.
  */
  TABLE_CATEGORY_TEMPORARY=1,

  /**
    User table.
    These tables do honor:
    - LOCK Table t FOR READ/WRITE
    - FLUSH TABLES WITH READ LOCK
    - SET GLOBAL READ_ONLY = ON
    User tables are cached in the table cache.
  */
  TABLE_CATEGORY_USER=2,

  /**
    Information schema tables.
    These tables are an interface provided by the system
    to inspect the system metadata.
    These tables do *not* honor:
    - LOCK Table t FOR READ/WRITE
    - FLUSH TABLES WITH READ LOCK
    - SET GLOBAL READ_ONLY = ON
    as there is no point in locking explicitely
    an INFORMATION_SCHEMA table.
    Nothing is directly written to information schema tables.
    Note that this value is not used currently,
    since information schema tables are not shared,
    but implemented as session specific temporary tables.
  */
  /*
    TODO: Fixing the performance issues of I_S will lead
    to I_S tables in the table cache, which should use
    this table type.
  */
  TABLE_CATEGORY_INFORMATION
};
typedef enum enum_table_category TABLE_CATEGORY;

TABLE_CATEGORY get_table_category(const LEX_STRING *db,
                                  const LEX_STRING *name);

/*
  This structure is shared between different table objects. There is one
  instance of table share per one table in the database.
*/

typedef struct st_table_share
{
  st_table_share() {}                    /* Remove gcc warning */

  /** Category of this table. */
  TABLE_CATEGORY table_category;

  /* hash of field names (contains pointers to elements of field array) */
  HASH	name_hash;			/* hash of field names */
  MEM_ROOT mem_root;
  TYPELIB keynames;			/* Pointers to keynames */
  TYPELIB fieldnames;			/* Pointer to fieldnames */
  TYPELIB *intervals;			/* pointer to interval info */
  pthread_mutex_t mutex;                /* For locking the share  */
  pthread_cond_t cond;			/* To signal that share is ready */
  struct st_table_share *next,		/* Link to unused shares */
    **prev;

  /* The following is copied to each Table on OPEN */
  Field **field;
  Field **found_next_number_field;
  Field *timestamp_field;               /* Used only during open */
  KEY  *key_info;			/* data of keys in database */
  uint	*blob_field;			/* Index to blobs in Field arrray*/

  unsigned char	*default_values;		/* row with default values */
  LEX_STRING comment;			/* Comment about table */
  const CHARSET_INFO *table_charset; /* Default charset of string fields */

  MY_BITMAP all_set;
  /*
    Key which is used for looking-up table in table cache and in the list
    of thread's temporary tables. Has the form of:
      "database_name\0table_name\0" + optional part for temporary tables.

    Note that all three 'table_cache_key', 'db' and 'table_name' members
    must be set (and be non-zero) for tables in table cache. They also
    should correspond to each other.
    To ensure this one can use set_table_cache() methods.
  */
  LEX_STRING table_cache_key;
  LEX_STRING db;                        /* Pointer to db */
  LEX_STRING table_name;                /* Table name (for open) */
  LEX_STRING path;	/* Path to .frm file (from datadir) */
  LEX_STRING normalized_path;		/* unpack_filename(path) */
  LEX_STRING connect_string;

  /*
     Set of keys in use, implemented as a Bitmap.
     Excludes keys disabled by ALTER Table ... DISABLE KEYS.
  */
  key_map keys_in_use;
  key_map keys_for_keyread;
  ha_rows min_rows, max_rows;		/* create information */
  uint32_t   avg_row_length;		/* create information */
  uint32_t   block_size;                   /* create information */
  uint32_t   version, mysql_version;
  uint32_t   timestamp_offset;		/* Set to offset+1 of record */
  uint32_t   reclength;			/* Recordlength */
  uint32_t   stored_rec_length;         /* Stored record length 
                                           (no generated-only virtual fields) */

  plugin_ref db_plugin;			/* storage engine plugin */
  inline handlerton *db_type() const	/* table_type for handler */
  {
    // assert(db_plugin);
    return db_plugin ? plugin_data(db_plugin, handlerton*) : NULL;
  }
  enum row_type row_type;		/* How rows are stored */
  enum tmp_table_type tmp_table;
  enum ha_choice transactional;
  enum ha_choice page_checksum;

  uint32_t ref_count;       /* How many Table objects uses this */
  uint32_t open_count;			/* Number of tables in open list */
  uint32_t blob_ptr_size;			/* 4 or 8 */
  uint32_t key_block_size;			/* create key_block_size, if used */
  uint32_t null_bytes, last_null_bit_pos;
  uint32_t fields;				/* Number of fields */
  uint32_t stored_fields;                   /* Number of stored fields 
                                           (i.e. without generated-only ones) */
  uint32_t rec_buff_length;                 /* Size of table->record[] buffer */
  uint32_t keys, key_parts;
  uint32_t max_key_length, max_unique_length, total_key_length;
  uint32_t uniques;                         /* Number of UNIQUE index */
  uint32_t null_fields;			/* number of null fields */
  uint32_t blob_fields;			/* number of blob fields */
  uint32_t timestamp_field_offset;		/* Field number for timestamp field */
  uint32_t varchar_fields;                  /* number of varchar fields */
  uint32_t db_create_options;		/* Create options from database */
  uint32_t db_options_in_use;		/* Options in use */
  uint32_t db_record_offset;		/* if HA_REC_IN_SEQ */
  uint32_t rowid_field_offset;		/* Field_nr +1 to rowid field */
  /* Index of auto-updated TIMESTAMP field in field array */
  uint32_t primary_key;
  uint32_t next_number_index;               /* autoincrement key number */
  uint32_t next_number_key_offset;          /* autoinc keypart offset in a key */
  uint32_t next_number_keypart;             /* autoinc keypart number in a key */
  uint32_t error, open_errno, errarg;       /* error from open_table_def() */
  uint32_t column_bitmap_size;
  unsigned char frm_version;
  uint32_t vfields;                         /* Number of virtual fields */
  bool null_field_first;
  bool db_low_byte_first;		/* Portable row format */
  bool crashed;
  bool name_lock, replace_with_name_lock;
  bool waiting_on_cond;                 /* Protection against free */
  uint32_t table_map_id;                   /* for row-based replication */
  uint64_t table_map_version;

  /*
    Cache for row-based replication table share checks that does not
    need to be repeated. Possible values are: -1 when cache value is
    not calculated yet, 0 when table *shall not* be replicated, 1 when
    table *may* be replicated.
  */
  int cached_row_logging_check;

  /*
    Set share's table cache key and update its db and table name appropriately.

    SYNOPSIS
      set_table_cache_key()
        key_buff    Buffer with already built table cache key to be
                    referenced from share.
        key_length  Key length.

    NOTES
      Since 'key_buff' buffer will be referenced from share it should has same
      life-time as share itself.
      This method automatically ensures that TABLE_SHARE::table_name/db have
      appropriate values by using table cache key as their source.
  */

  void set_table_cache_key(char *key_buff, uint32_t key_length)
  {
    table_cache_key.str= key_buff;
    table_cache_key.length= key_length;
    /*
      Let us use the fact that the key is "db/0/table_name/0" + optional
      part for temporary tables.
    */
    db.str=            table_cache_key.str;
    db.length=         strlen(db.str);
    table_name.str=    db.str + db.length + 1;
    table_name.length= strlen(table_name.str);
  }


  /*
    Set share's table cache key and update its db and table name appropriately.

    SYNOPSIS
      set_table_cache_key()
        key_buff    Buffer to be used as storage for table cache key
                    (should be at least key_length bytes).
        key         Value for table cache key.
        key_length  Key length.

    NOTE
      Since 'key_buff' buffer will be used as storage for table cache key
      it should has same life-time as share itself.
  */

  void set_table_cache_key(char *key_buff, const char *key, uint32_t key_length)
  {
    memcpy(key_buff, key, key_length);
    set_table_cache_key(key_buff, key_length);
  }

  inline bool honor_global_locks()
  {
    return (table_category == TABLE_CATEGORY_USER);
  }

  inline uint32_t get_table_def_version()
  {
    return table_map_id;
  }

} TABLE_SHARE;


extern uint32_t refresh_version;

/* Information for one open table */
enum index_hint_type
{
  INDEX_HINT_IGNORE,
  INDEX_HINT_USE,
  INDEX_HINT_FORCE
};

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
};

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

/*
  Make sure that the order of schema_tables and enum_schema_tables are the same.
*/

enum enum_schema_tables
{
  SCH_CHARSETS= 0,
  SCH_COLLATIONS,
  SCH_COLLATION_CHARACTER_SET_APPLICABILITY,
  SCH_COLUMNS,
  SCH_GLOBAL_STATUS,
  SCH_GLOBAL_VARIABLES,
  SCH_KEY_COLUMN_USAGE,
  SCH_OPEN_TABLES,
  SCH_PLUGINS,
  SCH_PROCESSLIST,
  SCH_REFERENTIAL_CONSTRAINTS,
  SCH_SCHEMATA,
  SCH_SESSION_STATUS,
  SCH_SESSION_VARIABLES,
  SCH_STATISTICS,
  SCH_STATUS,
  SCH_TABLES,
  SCH_TABLE_CONSTRAINTS,
  SCH_TABLE_NAMES,
  SCH_VARIABLES
};


#define MY_I_S_MAYBE_NULL 1
#define MY_I_S_UNSIGNED   2


#define SKIP_OPEN_TABLE 0                // do not open table
#define OPEN_FRM_ONLY   1                // open FRM file only
#define OPEN_FULL_TABLE 2                // open FRM,MYD, MYI files

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

struct Field_translator
{
  Item *item;
  const char *name;
};


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


#endif /* DRIZZLED_TABLE_H */
