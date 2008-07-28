/* Copyright (C) 2000-2006 MySQL AB

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


/* Structs that defines the TABLE */

class Item;				/* Needed by ORDER */
class Item_subselect;
class st_select_lex_unit;
class st_select_lex;
class COND_EQUAL;
class Security_context;

/*************************************************************************/

/* Order clause list element */

typedef struct st_order {
  struct st_order *next;
  Item	 **item;			/* Point at item in select fields */
  Item	 *item_ptr;			/* Storage for initial item */
  Item   **item_copy;			/* For SPs; the original item ptr */
  int    counter;                       /* position in SELECT list, correct
                                           only if counter_used is true*/
  bool	 asc;				/* true if ascending */
  bool	 free_me;			/* true if item isn't shared  */
  bool	 in_field_list;			/* true if in select field list */
  bool   counter_used;                  /* parameter was counter of columns */
  Field  *field;			/* If tmp-table group */
  char	 *buff;				/* If tmp-table group */
  table_map used, depend_map;
} ORDER;

enum tmp_table_type
{
  NO_TMP_TABLE, NON_TRANSACTIONAL_TMP_TABLE, TRANSACTIONAL_TMP_TABLE,
  INTERNAL_TMP_TABLE, SYSTEM_TMP_TABLE, TMP_TABLE_FRM_FILE_ONLY
};

enum frm_type_enum
{
  FRMTYPE_ERROR= 0,
  FRMTYPE_TABLE
};

frm_type_enum mysql_frm_type(THD *thd, char *path, enum legacy_db_type *dbt);


enum release_type { RELEASE_NORMAL, RELEASE_WAIT_FOR_DROP };

typedef struct st_filesort_info
{
  IO_CACHE *io_cache;           /* If sorted through filesort */
  uchar     **sort_keys;        /* Buffer for sorting keys */
  uchar     *buffpek;           /* Buffer for buffpek structures */
  uint      buffpek_len;        /* Max number of buffpeks in the buffer */
  uchar     *addon_buf;         /* Pointer to a buffer if sorted with fields */
  size_t    addon_length;       /* Length of the buffer */
  struct st_sort_addon_field *addon_field;     /* Pointer to the fields info */
  void    (*unpack)(struct st_sort_addon_field *, uchar *); /* To unpack back */
  uchar     *record_pointers;    /* If sorted in memory */
  ha_rows   found_records;      /* How many records in sort */
} FILESORT_INFO;


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
class Table_triggers_list;

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
    Note that LOCK TABLE t FOR READ/WRITE
    can be used on temporary tables.
    Temporary tables are not part of the table cache.
  */
  TABLE_CATEGORY_TEMPORARY=1,

  /**
    User table.
    These tables do honor:
    - LOCK TABLE t FOR READ/WRITE
    - FLUSH TABLES WITH READ LOCK
    - SET GLOBAL READ_ONLY = ON
    User tables are cached in the table cache.
  */
  TABLE_CATEGORY_USER=2,

  /**
    System table, maintained by the server.
    These tables do honor:
    - LOCK TABLE t FOR READ/WRITE
    - FLUSH TABLES WITH READ LOCK
    - SET GLOBAL READ_ONLY = ON
    Typically, writes to system tables are performed by
    the server implementation, not explicitly be a user.
    System tables are cached in the table cache.
  */
  TABLE_CATEGORY_SYSTEM=3,

  /**
    Information schema tables.
    These tables are an interface provided by the system
    to inspect the system metadata.
    These tables do *not* honor:
    - LOCK TABLE t FOR READ/WRITE
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
  TABLE_CATEGORY_INFORMATION=4
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

  /* The following is copied to each TABLE on OPEN */
  Field **field;
  Field **found_next_number_field;
  Field *timestamp_field;               /* Used only during open */
  KEY  *key_info;			/* data of keys in database */
  uint	*blob_field;			/* Index to blobs in Field arrray*/

  uchar	*default_values;		/* row with default values */
  LEX_STRING comment;			/* Comment about table */
  CHARSET_INFO *table_charset;		/* Default charset of string fields */

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
  LEX_STRING path;                	/* Path to .frm file (from datadir) */
  LEX_STRING normalized_path;		/* unpack_filename(path) */
  LEX_STRING connect_string;

  /* 
     Set of keys in use, implemented as a Bitmap.
     Excludes keys disabled by ALTER TABLE ... DISABLE KEYS.
  */
  key_map keys_in_use;
  key_map keys_for_keyread;
  ha_rows min_rows, max_rows;		/* create information */
  ulong   avg_row_length;		/* create information */
  ulong   version, mysql_version;
  ulong   timestamp_offset;		/* Set to offset+1 of record */
  ulong   reclength;			/* Recordlength */

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

  uint ref_count;                       /* How many TABLE objects uses this */
  uint open_count;			/* Number of tables in open list */
  uint blob_ptr_size;			/* 4 or 8 */
  uint key_block_size;			/* create key_block_size, if used */
  uint null_bytes, last_null_bit_pos;
  uint fields;				/* Number of fields */
  uint rec_buff_length;                 /* Size of table->record[] buffer */
  uint keys, key_parts;
  uint max_key_length, max_unique_length, total_key_length;
  uint uniques;                         /* Number of UNIQUE index */
  uint null_fields;			/* number of null fields */
  uint blob_fields;			/* number of blob fields */
  uint timestamp_field_offset;		/* Field number for timestamp field */
  uint varchar_fields;                  /* number of varchar fields */
  uint db_create_options;		/* Create options from database */
  uint db_options_in_use;		/* Options in use */
  uint db_record_offset;		/* if HA_REC_IN_SEQ */
  uint raid_type, raid_chunks;
  uint rowid_field_offset;		/* Field_nr +1 to rowid field */
  /* Index of auto-updated TIMESTAMP field in field array */
  uint primary_key;
  uint next_number_index;               /* autoincrement key number */
  uint next_number_key_offset;          /* autoinc keypart offset in a key */
  uint next_number_keypart;             /* autoinc keypart number in a key */
  uint error, open_errno, errarg;       /* error from open_table_def() */
  uint column_bitmap_size;
  uchar frm_version;
  bool null_field_first;
  bool system;                          /* Set if system table (one record) */
  bool crypted;                         /* If .frm file is crypted */
  bool db_low_byte_first;		/* Portable row format */
  bool crashed;
  bool name_lock, replace_with_name_lock;
  bool waiting_on_cond;                 /* Protection against free */
  ulong table_map_id;                   /* for row-based replication */
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

  void set_table_cache_key(char *key_buff, uint key_length)
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

  void set_table_cache_key(char *key_buff, const char *key, uint key_length)
  {
    memcpy(key_buff, key, key_length);
    set_table_cache_key(key_buff, key_length);
  }

  inline bool honor_global_locks()
  {
    return ((table_category == TABLE_CATEGORY_USER)
            || (table_category == TABLE_CATEGORY_SYSTEM));
  }

  inline ulong get_table_def_version()
  {
    return table_map_id;
  }

} TABLE_SHARE;


extern ulong refresh_version;

/* Information for one open table */
enum index_hint_type
{
  INDEX_HINT_IGNORE,
  INDEX_HINT_USE,
  INDEX_HINT_FORCE
};

struct st_table {
  st_table() {}                               /* Remove gcc warning */

  TABLE_SHARE	*s;
  handler	*file;
  struct st_table *next, *prev;

  THD	*in_use;                        /* Which thread uses this */
  Field **field;			/* Pointer to fields */

  uchar *record[2];			/* Pointer to records */
  uchar *write_row_record;		/* Used as optimisation in
					   THD::write_row */
  uchar *insert_values;                  /* used by INSERT ... UPDATE */
  /* 
    Map of keys that can be used to retrieve all data from this table 
    needed by the query without reading the row.
  */
  key_map covering_keys;
  key_map quick_keys, merge_keys;
  /*
    A set of keys that can be used in the query that references this
    table.

    All indexes disabled on the table's TABLE_SHARE (see TABLE::s) will be 
    subtracted from this set upon instantiation. Thus for any TABLE t it holds
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

  /* Table's triggers, 0 if there are no of them */
  Table_triggers_list *triggers;
  TABLE_LIST *pos_in_table_list;/* Element referring to this table */
  ORDER		*group;
  const char	*alias;            	  /* alias or table name */
  uchar		*null_flags;
  my_bitmap_map	*bitmap_init_value;
  MY_BITMAP     def_read_set, def_write_set, tmp_set; /* containers */
  MY_BITMAP     *read_set, *write_set;          /* Active column sets */
  /*
   The ID of the query that opened and is using this table. Has different
   meanings depending on the table type.

   Temporary tables:

   table->query_id is set to thd->query_id for the duration of a statement
   and is reset to 0 once it is closed by the same statement. A non-zero
   table->query_id means that a statement is using the table even if it's
   not the current statement (table is in use by some outer statement).

   Non-temporary tables:

   Under pre-locked or LOCK TABLES mode: query_id is set to thd->query_id
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

  uint          lock_position;          /* Position in MYSQL_LOCK.table */
  uint          lock_data_start;        /* Start pos. in MYSQL_LOCK.locks */
  uint          lock_count;             /* Number of locks */
  uint		tablenr,used_fields;
  uint          temp_pool_slot;		/* Used by intern temp tables */
  uint		status;                 /* What's in record[0] */
  uint		db_stat;		/* mode of file as in handler.h */
  /* number of select if it is derived table */
  uint          derived_select_number;
  int		current_lock;           /* Type of lock on table */
  my_bool copy_blobs;			/* copy_blobs when storing */

  /*
    0 or JOIN_TYPE_{LEFT|RIGHT}. Currently this is only compared to 0.
    If maybe_null !=0, this table is inner w.r.t. some outer join operation,
    and null_row may be true.
  */
  uint maybe_null;
  /*
    If true, the current table row is considered to have all columns set to 
    NULL, including columns declared as "not null" (see maybe_null).
  */
  my_bool null_row;

  /*
    TODO: Each of the following flags take up 8 bits. They can just as easily
    be put into one single unsigned long and instead of taking up 18
    bytes, it would take up 4.
  */
  my_bool force_index;
  my_bool distinct,const_table,no_rows;
  my_bool key_read, no_keyread;
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
  my_bool open_placeholder;
  my_bool locked_by_logger;
  my_bool no_replicate;
  my_bool locked_by_name;
  my_bool no_cache;
  /* To signal that the table is associated with a HANDLER statement */
  my_bool open_by_handler;
  /*
    To indicate that a non-null value of the auto_increment field
    was provided by the user or retrieved from the current record.
    Used only in the MODE_NO_AUTO_VALUE_ON_ZERO mode.
  */
  my_bool auto_increment_field_not_null;
  my_bool insert_or_update;             /* Can be used by the handler */
  my_bool alias_name_used;		/* true if table_name is alias */
  my_bool get_fields_in_item_tree;      /* Signal to fix_field */

  REGINFO reginfo;			/* field connections */
  MEM_ROOT mem_root;
  FILESORT_INFO sort;

  bool fill_item_list(List<Item> *item_list) const;
  void reset_item_list(List<Item> *item_list) const;
  void clear_column_bitmaps(void);
  void prepare_for_position(void);
  void mark_columns_used_by_index_no_reset(uint index, MY_BITMAP *map);
  void mark_columns_used_by_index(uint index);
  void restore_column_maps_after_mark_index();
  void mark_auto_increment_column(void);
  void mark_columns_needed_for_update(void);
  void mark_columns_needed_for_delete(void);
  void mark_columns_needed_for_insert(void);
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
};

enum enum_schema_table_state
{ 
  NOT_PROCESSED= 0,
  PROCESSED_BY_CREATE_SORT_INDEX,
  PROCESSED_BY_JOIN_EXEC
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
  uint field_length;
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
  uint field_flags;        // Field atributes(maybe_null, signed, unsigned etc.)
  const char* old_name;
  /**
     This should be one of @c SKIP_OPEN_TABLE,
     @c OPEN_FRM_ONLY or @c OPEN_FULL_TABLE.
  */
  uint open_method;
} ST_FIELD_INFO;


struct TABLE_LIST;
typedef class Item COND;

typedef struct st_schema_table
{
  const char* table_name;
  ST_FIELD_INFO *fields_info;
  /* Create information_schema table */
  TABLE *(*create_table)  (THD *thd, TABLE_LIST *table_list);
  /* Fill table with data */
  int (*fill_table) (THD *thd, TABLE_LIST *tables, COND *cond);
  /* Handle fileds for old SHOW */
  int (*old_format) (THD *thd, struct st_schema_table *schema_table);
  int (*process_table) (THD *thd, TABLE_LIST *tables, TABLE *table,
                        bool res, LEX_STRING *db_name, LEX_STRING *table_name);
  int idx_field1, idx_field2; 
  bool hidden;
  uint i_s_requested_object;  /* the object we need to open(TABLE | VIEW) */
} ST_SCHEMA_TABLE;


#define JOIN_TYPE_LEFT	1
#define JOIN_TYPE_RIGHT	2

struct st_lex;
class select_union;
class TMP_TABLE_PARAM;

Item *create_view_field(THD *thd, TABLE_LIST *view, Item **field_ref,
                        const char *name);

struct Field_translator
{
  Item *item;
  const char *name;
};


/*
  Column reference of a NATURAL/USING join. Since column references in
  joins can be both from views and stored tables, may point to either a
  Field (for tables), or a Field_translator (for views).
*/

class Natural_join_column: public Sql_alloc
{
public:
  Field_translator *view_field;  /* Column reference of merge view. */
  Field            *table_field; /* Column reference of table or temp view. */
  TABLE_LIST *table_ref; /* Original base table/view reference. */
  /*
    True if a common join column of two NATURAL/USING join operands. Notice
    that when we have a hierarchy of nested NATURAL/USING joins, a column can
    be common at some level of nesting but it may not be common at higher
    levels of nesting. Thus this flag may change depending on at which level
    we are looking at some column.
  */
  bool is_common;
public:
  Natural_join_column(Field_translator *field_param, TABLE_LIST *tab);
  Natural_join_column(Field *field_param, TABLE_LIST *tab);
  const char *name();
  Item *create_item(THD *thd);
  Field *field();
  const char *table_name();
  const char *db_name();
};


/*
  Table reference in the FROM clause.

  These table references can be of several types that correspond to
  different SQL elements. Below we list all types of TABLE_LISTs with
  the necessary conditions to determine when a TABLE_LIST instance
  belongs to a certain type.

  1) table (TABLE_LIST::view == NULL)
     - base table
       (TABLE_LIST::derived == NULL)
     - subquery - TABLE_LIST::table is a temp table
       (TABLE_LIST::derived != NULL)
     - information schema table
       (TABLE_LIST::schema_table != NULL)
       NOTICE: for schema tables TABLE_LIST::field_translation may be != NULL
  2) view (TABLE_LIST::view != NULL)
     - merge    (TABLE_LIST::effective_algorithm == VIEW_ALGORITHM_MERGE)
           also (TABLE_LIST::field_translation != NULL)
     - tmptable (TABLE_LIST::effective_algorithm == VIEW_ALGORITHM_TMPTABLE)
           also (TABLE_LIST::field_translation == NULL)
  3) nested table reference (TABLE_LIST::nested_join != NULL)
     - table sequence - e.g. (t1, t2, t3)
       TODO: how to distinguish from a JOIN?
     - general JOIN
       TODO: how to distinguish from a table sequence?
     - NATURAL JOIN
       (TABLE_LIST::natural_join != NULL)
       - JOIN ... USING
         (TABLE_LIST::join_using_fields != NULL)
     - semi-join
       ;
*/

class Index_hint;
struct TABLE_LIST
{
  TABLE_LIST() {}                          /* Remove gcc warning */

  /**
    Prepare TABLE_LIST that consists of one table instance to use in
    simple_open_and_lock_tables
  */
  inline void init_one_table(const char *db_name_arg,
                             const char *table_name_arg,
                             enum thr_lock_type lock_type_arg)
  {
    memset(this, 0, sizeof(*this));
    db= (char*) db_name_arg;
    table_name= alias= (char*) table_name_arg;
    lock_type= lock_type_arg;
  }

  /*
    List of tables local to a subquery (used by SQL_LIST). Considers
    views as leaves (unlike 'next_leaf' below). Created at parse time
    in st_select_lex::add_table_to_list() -> table_list.link_in_list().
  */
  TABLE_LIST *next_local;
  /* link in a global list of all queries tables */
  TABLE_LIST *next_global, **prev_global;
  char		*db, *alias, *table_name, *schema_table_name;
  char          *option;                /* Used by cache index  */
  Item		*on_expr;		/* Used with outer join */
  Item          *sj_on_expr;
  /*
    (Valid only for semi-join nests) Bitmap of tables that are within the
    semi-join (this is different from bitmap of all nest's children because
    tables that were pulled out of the semi-join nest remain listed as
    nest's children).
  */
  table_map     sj_inner_tables;
  /* Number of IN-compared expressions */
  uint          sj_in_exprs; 
  /*
    The structure of ON expression presented in the member above
    can be changed during certain optimizations. This member
    contains a snapshot of AND-OR structure of the ON expression
    made after permanent transformations of the parse tree, and is
    used to restore ON clause before every reexecution of a prepared
    statement or stored procedure.
  */
  Item          *prep_on_expr;
  COND_EQUAL    *cond_equal;            /* Used with outer join */
  /*
    During parsing - left operand of NATURAL/USING join where 'this' is
    the right operand. After parsing (this->natural_join == this) iff
    'this' represents a NATURAL or USING join operation. Thus after
    parsing 'this' is a NATURAL/USING join iff (natural_join != NULL).
  */
  TABLE_LIST *natural_join;
  /*
    True if 'this' represents a nested join that is a NATURAL JOIN.
    For one of the operands of 'this', the member 'natural_join' points
    to the other operand of 'this'.
  */
  bool is_natural_join;
  /* Field names in a USING clause for JOIN ... USING. */
  List<String> *join_using_fields;
  /*
    Explicitly store the result columns of either a NATURAL/USING join or
    an operand of such a join.
  */
  List<Natural_join_column> *join_columns;
  /* true if join_columns contains all columns of this table reference. */
  bool is_join_columns_complete;

  /*
    List of nodes in a nested join tree, that should be considered as
    leaves with respect to name resolution. The leaves are: views,
    top-most nodes representing NATURAL/USING joins, subqueries, and
    base tables. All of these TABLE_LIST instances contain a
    materialized list of columns. The list is local to a subquery.
  */
  TABLE_LIST *next_name_resolution_table;
  /* Index names in a "... JOIN ... USE/IGNORE INDEX ..." clause. */
  List<Index_hint> *index_hints;
  TABLE        *table;                          /* opened table */
  uint          table_id; /* table id (from binlog) for opened table */
  /*
    select_result for derived table to pass it from table creation to table
    filling procedure
  */
  select_union  *derived_result;
  /*
    Reference from aux_tables to local list entry of main select of
    multi-delete statement:
    delete t1 from t2,t1 where t1.a<'B' and t2.b=t1.b;
    here it will be reference of first occurrence of t1 to second (as you
    can see this lists can't be merged)
  */
  TABLE_LIST	*correspondent_table;
  st_select_lex_unit *derived;		/* SELECT_LEX_UNIT of derived table */
  ST_SCHEMA_TABLE *schema_table;        /* Information_schema table */
  st_select_lex	*schema_select_lex;
  /*
    True when the view field translation table is used to convert
    schema table fields for backwards compatibility with SHOW command.
  */
  bool schema_table_reformed;
  TMP_TABLE_PARAM *schema_table_param;
  /* link to select_lex where this table was used */
  st_select_lex	*select_lex;
  Field_translator *field_translation;	/* array of VIEW fields */
  /* pointer to element after last one in translation table above */
  Field_translator *field_translation_end;
  /*
    List (based on next_local) of underlying tables of this view. I.e. it
    does not include the tables of subqueries used in the view. Is set only
    for merged views.
  */
  TABLE_LIST	*merge_underlying_list;
  /*
    - 0 for base tables
    - in case of the view it is the list of all (not only underlying
    tables but also used in subquery ones) tables of the view.
  */
  List<TABLE_LIST> *view_tables;
  /* most upper view this table belongs to */
  TABLE_LIST	*belong_to_view;
  /* Ptr to parent MERGE table list item. See top comment in ha_myisammrg.cc */
  TABLE_LIST    *parent_l;
  /*
    List of all base tables local to a subquery including all view
    tables. Unlike 'next_local', this in this list views are *not*
    leaves. Created in setup_tables() -> make_leaves_list().
  */
  TABLE_LIST	*next_leaf;
  /* data need by some engines in query cache*/
  uint64_t     engine_data;
  /* call back function for asking handler about caching in query cache */
  qc_engine_callback callback_func;
  thr_lock_type lock_type;
  uint		outer_join;		/* Which join type */
  uint		shared;			/* Used in multi-upd */
  size_t        db_length;
  size_t        table_name_length;
  bool		straight;		/* optimize with prev table */
  bool          updating;               /* for replicate-do/ignore table */
  bool		force_index;		/* prefer index over table scan */
  bool          ignore_leaves;          /* preload only non-leaf nodes */
  table_map     dep_tables;             /* tables the table depends on      */
  table_map     on_expr_dep_tables;     /* tables on expression depends on  */
  struct st_nested_join *nested_join;   /* if the element is a nested join  */
  TABLE_LIST *embedding;             /* nested join containing the table */
  List<TABLE_LIST> *join_list;/* join list the table belongs to   */
  bool		cacheable_table;	/* stop PS caching */
  /* FRMTYPE_ERROR if any type is acceptable */
  enum frm_type_enum required_type;
  handlerton	*db_type;		/* table_type for handler */
  char		timestamp_buffer[20];	/* buffer for timestamp (19+1) */
  /*
    This TABLE_LIST object corresponds to the table to be created
    so it is possible that it does not exist (used in CREATE TABLE
    ... SELECT implementation).
  */
  bool          create;
  /* For transactional locking. */
  int           lock_timeout;           /* NOWAIT or WAIT [X]               */
  bool          lock_transactional;     /* If transactional lock requested. */
  bool          internal_tmp_table;
  /** true if an alias for this table was specified in the SQL. */
  bool          is_alias;
  /** true if the table is referred to in the statement using a fully
      qualified name (<db_name>.<table_name>).
  */
  bool          is_fqtn;

  uint i_s_requested_object;
  bool has_db_lookup_value;
  bool has_table_lookup_value;
  uint table_open_method;
  enum enum_schema_table_state schema_table_state;
  void set_underlying_merge();
  bool setup_underlying(THD *thd);
  void cleanup_items();
  /*
    If you change placeholder(), please check the condition in
    check_transactional_lock() too.
  */
  bool placeholder()
  {
    return derived || schema_table || (create && !table->db_stat) || !table;
  }
  void print(THD *thd, String *str, enum_query_type query_type);
  bool set_insert_values(MEM_ROOT *mem_root);
  TABLE_LIST *find_underlying_table(TABLE *table);
  TABLE_LIST *first_leaf_for_name_resolution();
  TABLE_LIST *last_leaf_for_name_resolution();
  bool is_leaf_for_name_resolution();
  inline TABLE_LIST *top_table()
    { return belong_to_view ? belong_to_view : this; }

  /*
    Cleanup for re-execution in a prepared statement or a stored
    procedure.
  */
  void reinit_before_use(THD *thd);
  Item_subselect *containing_subselect();

  /* 
    Compiles the tagged hints list and fills up st_table::keys_in_use_for_query,
    st_table::keys_in_use_for_group_by, st_table::keys_in_use_for_order_by,
    st_table::force_index and st_table::covering_keys.
  */
  bool process_index_hints(TABLE *table);

private:
  bool prep_where(THD *thd, Item **conds, bool no_where_clause);
  /*
    Cleanup for re-execution in a prepared statement or a stored
    procedure.
  */
};

class Item;

/*
  Iterator over the fields of a generic table reference.
*/

class Field_iterator: public Sql_alloc
{
public:
  Field_iterator() {}                         /* Remove gcc warning */
  virtual ~Field_iterator() {}
  virtual void set(TABLE_LIST *)= 0;
  virtual void next()= 0;
  virtual bool end_of_fields()= 0;              /* Return 1 at end of list */
  virtual const char *name()= 0;
  virtual Item *create_item(THD *)= 0;
  virtual Field *field()= 0;
};


/* 
  Iterator over the fields of a base table, view with temporary
  table, or subquery.
*/

class Field_iterator_table: public Field_iterator
{
  Field **ptr;
public:
  Field_iterator_table() :ptr(0) {}
  void set(TABLE_LIST *table) { ptr= table->table->field; }
  void set_table(TABLE *table) { ptr= table->field; }
  void next() { ptr++; }
  bool end_of_fields() { return *ptr == 0; }
  const char *name();
  Item *create_item(THD *thd);
  Field *field() { return *ptr; }
};


/* Iterator over the fields of a merge view. */

class Field_iterator_view: public Field_iterator
{
  Field_translator *ptr, *array_end;
  TABLE_LIST *view;
public:
  Field_iterator_view() :ptr(0), array_end(0) {}
  void set(TABLE_LIST *table);
  void next() { ptr++; }
  bool end_of_fields() { return ptr == array_end; }
  const char *name();
  Item *create_item(THD *thd);
  Item **item_ptr() {return &ptr->item; }
  Field *field() { return 0; }
  inline Item *item() { return ptr->item; }
  Field_translator *field_translator() { return ptr; }
};


/*
  Field_iterator interface to the list of materialized fields of a
  NATURAL/USING join.
*/

class Field_iterator_natural_join: public Field_iterator
{
  List_iterator_fast<Natural_join_column> column_ref_it;
  Natural_join_column *cur_column_ref;
public:
  Field_iterator_natural_join() :cur_column_ref(NULL) {}
  ~Field_iterator_natural_join() {}
  void set(TABLE_LIST *table);
  void next();
  bool end_of_fields() { return !cur_column_ref; }
  const char *name() { return cur_column_ref->name(); }
  Item *create_item(THD *thd) { return cur_column_ref->create_item(thd); }
  Field *field() { return cur_column_ref->field(); }
  Natural_join_column *column_ref() { return cur_column_ref; }
};


/*
  Generic iterator over the fields of an arbitrary table reference.

  DESCRIPTION
    This class unifies the various ways of iterating over the columns
    of a table reference depending on the type of SQL entity it
    represents. If such an entity represents a nested table reference,
    this iterator encapsulates the iteration over the columns of the
    members of the table reference.

  IMPLEMENTATION
    The implementation assumes that all underlying NATURAL/USING table
    references already contain their result columns and are linked into
    the list TABLE_LIST::next_name_resolution_table.
*/

class Field_iterator_table_ref: public Field_iterator
{
  TABLE_LIST *table_ref, *first_leaf, *last_leaf;
  Field_iterator_table        table_field_it;
  Field_iterator_view         view_field_it;
  Field_iterator_natural_join natural_join_it;
  Field_iterator *field_it;
  void set_field_iterator();
public:
  Field_iterator_table_ref() :field_it(NULL) {}
  void set(TABLE_LIST *table);
  void next();
  bool end_of_fields()
  { return (table_ref == last_leaf && field_it->end_of_fields()); }
  const char *name() { return field_it->name(); }
  const char *table_name();
  const char *db_name();
  Item *create_item(THD *thd) { return field_it->create_item(thd); }
  Field *field() { return field_it->field(); }
  Natural_join_column *get_or_create_column_ref(TABLE_LIST *parent_table_ref);
  Natural_join_column *get_natural_column_ref();
};


typedef struct st_nested_join
{
  List<TABLE_LIST>  join_list;       /* list of elements in the nested join */
  table_map         used_tables;     /* bitmap of tables in the nested join */
  table_map         not_null_tables; /* tables that rejects nulls           */
  struct st_join_table *first_nested;/* the first nested table in the plan  */
  /* 
    Used to count tables in the nested join in 2 isolated places:
    1. In make_outerjoin_info(). 
    2. check_interleaving_with_nj/restore_prev_nj_state (these are called
       by the join optimizer. 
    Before each use the counters are zeroed by reset_nj_counters.
  */
  uint              counter_;
  nested_join_map   nj_map;          /* Bit used to identify this nested join*/
  /*
    (Valid only for semi-join nests) Bitmap of tables outside the semi-join
    that are used within the semi-join's ON condition.
  */
  table_map         sj_depends_on;
  /* Outer non-trivially correlated tables */
  table_map         sj_corr_tables;
  List<Item>        sj_outer_expr_list;
} NESTED_JOIN;


typedef struct st_changed_table_list
{
  struct	st_changed_table_list *next;
  char		*key;
  uint32_t        key_length;
} CHANGED_TABLE_LIST;


typedef struct st_open_table_list{
  struct st_open_table_list *next;
  char	*db,*table;
  uint32_t in_use,locked;
} OPEN_TABLE_LIST;

typedef struct st_table_field_w_type
{
  LEX_STRING name;
  LEX_STRING type;
  LEX_STRING cset;
} TABLE_FIELD_W_TYPE;


bool
table_check_intact(TABLE *table, const uint table_f_count,
                   const TABLE_FIELD_W_TYPE *table_def);

static inline my_bitmap_map *tmp_use_all_columns(TABLE *table,
                                                 MY_BITMAP *bitmap)
{
  my_bitmap_map *old= bitmap->bitmap;
  bitmap->bitmap= table->s->all_set.bitmap;
  return old;
}


static inline void tmp_restore_column_map(MY_BITMAP *bitmap,
                                          my_bitmap_map *old)
{
  bitmap->bitmap= old;
}

/* The following is only needed for debugging */

static inline my_bitmap_map *dbug_tmp_use_all_columns(TABLE *table __attribute__((__unused__)),
                                                      MY_BITMAP *bitmap __attribute__((__unused__)))
{
  return 0;
}

static inline void dbug_tmp_restore_column_map(MY_BITMAP *bitmap __attribute__((__unused__)),
                                               my_bitmap_map *old __attribute__((__unused__)))
{
  return;
}

size_t max_row_length(TABLE *table, const uchar *data);

