/* -*- mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; -*-
 *  vim:expandtab:shiftwidth=2:tabstop=2:smarttab:
 *
 *  Copyright (C) 2008 Sun Microsystems, Inc.
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

#pragma once

#include <string>
#include <boost/dynamic_bitset.hpp>

#include <drizzled/order.h>
#include <drizzled/filesort_info.h>
#include <drizzled/natural_join_column.h>
#include <drizzled/field_iterator.h>
#include <drizzled/cursor.h>
#include <drizzled/lex_string.h>
#include <drizzled/table/instance.h>
#include <drizzled/atomics.h>

#include <drizzled/visibility.h>

namespace drizzled {

/**
 * Class representing a set of records, either in a temporary, 
 * normal, or derived table.
 */
class DRIZZLED_API Table 
{
  Field **field; /**< Pointer to fields collection */

public:
  Field **getFields() const
  {
    return field;
  }

  Field *getField(uint32_t arg) const
  {
    return field[arg];
  }

  void setFields(Field **arg)
  {
    field= arg;
  }

  void setFieldAt(Field *arg, uint32_t arg_pos)
  {
    field[arg_pos]= arg;
  }

  Cursor *cursor; /**< Pointer to the storage engine's Cursor managing this table */

private:
  Table *next;

public:
  Table *getNext() const
  {
    return next;
  }

  Table **getNextPtr()
  {
    return &next;
  }

  void setNext(Table *arg)
  {
    next= arg;
  }

  void unlink()
  {
    getNext()->setPrev(getPrev());		/* remove from used chain */
    getPrev()->setNext(getNext());
  }

private:
  Table *prev;
public:
  Table *getPrev() const
  {
    return prev;
  }

  Table **getPrevPtr()
  {
    return &prev;
  }

  void setPrev(Table *arg)
  {
    prev= arg;
  }

  boost::dynamic_bitset<> *read_set; /* Active column sets */
  boost::dynamic_bitset<> *write_set; /* Active column sets */

  uint32_t tablenr;
  uint32_t db_stat; /**< information about the cursor as in Cursor.h */

  boost::dynamic_bitset<> def_read_set; /**< Default read set of columns */
  boost::dynamic_bitset<> def_write_set; /**< Default write set of columns */
  boost::dynamic_bitset<> tmp_set; /* Not sure about this... */

  Session *in_use; /**< Pointer to the current session using this object */
  Session *getSession()
  {
    return in_use;
  }

  unsigned char *getInsertRecord() const
  {
    return record[0];
  }

  unsigned char *getUpdateRecord()
  {
    return record[1];
  }

  unsigned char *record[2]; /**< Pointer to "records" */
  std::vector<unsigned char> insert_values; /* used by INSERT ... UPDATE */
  KeyInfo  *key_info; /**< data of keys in database */
  Field *next_number_field; /**< Set if next_number is activated. @TODO What the heck is the difference between this and the next member? */
  Field *found_next_number_field; /**< Points to the "next-number" field (autoincrement field) */
  field::Epoch *timestamp_field; /**< Points to the auto-setting timestamp field, if any */

  TableList *pos_in_table_list; /* Element referring to this table */
  Order *group;
  
  const char *getAlias() const
  {
    return _alias.c_str();
  }

  void clearAlias()
  {
    _alias.clear();
  }

  void setAlias(const char *arg)
  {
    _alias= arg;
  }

private:
  std::string _alias; /**< alias or table name if no alias */
public:

  unsigned char *null_flags;

  uint32_t lock_position; /**< Position in DRIZZLE_LOCK.table */
  uint32_t lock_data_start; /**< Start pos. in DRIZZLE_LOCK.locks */
  uint32_t lock_count; /**< Number of locks */
  uint32_t used_fields;
  uint32_t status; /* What's in getInsertRecord() */
  /* number of select if it is derived table */
  uint32_t derived_select_number;
  int current_lock; /**< Type of lock on table */
  bool copy_blobs; /**< Should blobs by copied when storing? */

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
  bool distinct;
  bool const_table;
  bool no_rows;
  bool key_read;
  bool no_keyread;
  /*
    Placeholder for an open table which prevents other connections
    from taking name-locks on this table. Typically used with
    TableShare::version member to take an exclusive name-lock on
    this table name -- a name lock that not only prevents other
    threads from opening the table, but also blocks other name
    locks. This is achieved by:
    - setting open_placeholder to 1 - this will block other name
      locks, as wait_for_locked_table_name will be forced to wait,
      see table_is_used for details.
    - setting version to 0 - this will force other threads to close
      the instance of this table and wait (this is the same approach
      as used for usual name locks).
    An exclusively name-locked table currently can have no Cursor
    object associated with it (db_stat is always 0), but please do
    not rely on that.
  */
  bool open_placeholder;
  bool locked_by_name;
  bool no_cache;
  /*
    To indicate that a non-null value of the auto_increment field
    was provided by the user or retrieved from the current record.
    Used only in the MODE_NO_AUTO_VALUE_ON_ZERO mode.
  */
  bool auto_increment_field_not_null;
  bool alias_name_used; /* true if table_name is alias */

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
  query_id_t query_id;

  /**
   * Estimate of number of records that satisfy SARGable part of the table
   * condition, or table->cursor->records if no SARGable condition could be
   * constructed.
   * This value is used by join optimizer as an estimate of number of records
   * that will pass the table condition (condition that depends on fields of
   * this table and constants)
   */
  ha_rows quick_condition_rows;

  /*
    If this table has TIMESTAMP field with auto-set property (pointed by
    timestamp_field member) then this variable indicates during which
    operations (insert only/on update/in both cases) we should set this
    field to current timestamp. If there are no such field in this table
    or we should not automatically set its value during execution of current
    statement then the variable contains TIMESTAMP_NO_AUTO_SET (i.e. 0).

    Value of this variable is set for each statement in open_table() and
    if needed cleared later in statement processing code (see update_query()
    as example).
  */
  timestamp_auto_set_type timestamp_field_type;
  table_map map; ///< ID bit of table (1,2,4,8,16...)

  RegInfo reginfo; /* field connections */

  /*
    Map of keys that can be used to retrieve all data from this table
    needed by the query without reading the row.
  */
  key_map covering_keys;
  key_map quick_keys;
  key_map merge_keys;

  /*
    A set of keys that can be used in the query that references this
    table.

    All indexes disabled on the table's TableShare (see Table::s) will be
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

  /*
    For each key that has quick_keys.test(key) == true: estimate of #records
    and max #key parts that range access would use.
  */
  ha_rows quick_rows[MAX_KEY];

  /* Bitmaps of key parts that =const for the entire join. */
  key_part_map  const_key_parts[MAX_KEY];

  uint32_t quick_key_parts[MAX_KEY];
  uint32_t quick_n_ranges[MAX_KEY];

private:
  memory::Root mem_root;

  void init_mem_root()
  {
    if (not mem_root.alloc_root_inited())
      mem_root.init(TABLE_ALLOC_BLOCK_SIZE);
  }
public:
  memory::Root& mem()
  {
    init_mem_root();
    return mem_root;
  }

  unsigned char* alloc(size_t arg)
  {
    init_mem_root();
    return mem_root.alloc(arg);
  }

  char* strmake(const char* str_arg, size_t len_arg)
  {
    init_mem_root();
    return mem_root.strmake(str_arg, len_arg);
  }

  filesort_info sort;

  Table();
  virtual ~Table();

  int report_error(int error);
  /**
   * Free information allocated by openfrm
   *
   * @param If true if we also want to free table_share
   * @note this should all be the destructor
   */
  int delete_table(bool free_share= false);

  void resetTable(Session *session, TableShare *share, uint32_t db_stat_arg);

  /* SHARE methods */
  virtual const TableShare *getShare() const= 0; /* Get rid of this long term */
  virtual TableShare *getMutableShare()= 0; /* Get rid of this long term */
  virtual bool hasShare() const= 0; /* Get rid of this long term */
  virtual void setShare(TableShare *new_share)= 0; /* Get rid of this long term */

  virtual void release(void)= 0;

  uint32_t sizeKeys() { return getMutableShare()->sizeKeys(); }
  uint32_t sizeFields() { return getMutableShare()->sizeFields(); }
  uint32_t getRecordLength() const { return getShare()->getRecordLength(); }
  uint32_t sizeBlobFields() { return getMutableShare()->blob_fields; }
  uint32_t *getBlobField() { return &getMutableShare()->blob_field[0]; }

public:
  virtual bool hasVariableWidth() const
  {
    return getShare()->hasVariableWidth(); // We should calculate this.
  }

  virtual void setVariableWidth(void);

  Field_blob *getBlobFieldAt(uint32_t arg) const
  {
    if (arg < getShare()->blob_fields)
      return (Field_blob*) field[getShare()->blob_field[arg]]; /*NOTE: Using 'Table.field' NOT SharedTable.field. */

    return NULL;
  }
  inline uint8_t getBlobPtrSize() const { return getShare()->sizeBlobPtr(); }
  inline uint32_t getNullBytes() const { return getShare()->null_bytes; }
  inline uint32_t getNullFields() const { return getShare()->null_fields; }
  inline unsigned char *getDefaultValues() { return  getMutableShare()->getDefaultValues(); }
  inline const char *getSchemaName()  const { return getShare()->getSchemaName(); }
  inline const char *getTableName()  const { return getShare()->getTableName(); }

  inline bool isDatabaseLowByteFirst() const { return getShare()->db_low_byte_first; } /* Portable row format */
  inline bool isNameLock() const { return open_placeholder; }

  uint32_t index_flags(uint32_t idx) const;

  inline plugin::StorageEngine *getEngine() const   /* table_type for handler */
  {
    return getShare()->getEngine();
  }

  Cursor &getCursor() const /* table_type for handler */
  {
    assert(cursor);
    return *cursor;
  }

  size_t max_row_length(const unsigned char *data);
  uint32_t find_shortest_key(const key_map *usable_keys);
  bool compare_record(Field **ptr);
  bool records_are_comparable();
  bool compare_records();
  /* TODO: the (re)storeRecord's may be able to be further condensed */
  void storeRecord();
  void storeRecordAsInsert();
  void storeRecordAsDefault();
  void restoreRecord();
  void restoreRecordAsDefault();
  void emptyRecord();


  /* See if this can be blown away */
  inline uint32_t getDBStat () { return db_stat; }
  inline uint32_t setDBStat () { return db_stat; }
  /**
   * Create Item_field for each column in the table.
   *
   * @param[out] a pointer to an empty list used to store items
   *
   * @details
   *
   * Create Item_field object for each column in the table and
   * initialize it with the corresponding Field. New items are
   * created in the current Session memory root.
   *
   * @retval
   *  false on success
   * @retval
   *  true when out of memory
   */
  void fill_item_list(List<Item>&) const;
  void clear_column_bitmaps(void);
  void prepare_for_position(void);
  void mark_columns_used_by_index_no_reset(uint32_t index, boost::dynamic_bitset<>& bitmap);
  void mark_columns_used_by_index_no_reset(uint32_t index);
  void mark_columns_used_by_index(uint32_t index);
  void restore_column_maps_after_mark_index();
  void mark_auto_increment_column(void);
  void mark_columns_needed_for_update(void);
  void mark_columns_needed_for_delete(void);
  void mark_columns_needed_for_insert(void);
  void column_bitmaps_set(boost::dynamic_bitset<>& read_set_arg,
                          boost::dynamic_bitset<>& write_set_arg);

  void restore_column_map(const boost::dynamic_bitset<>& old);

  const boost::dynamic_bitset<> use_all_columns(boost::dynamic_bitset<>& map);
  inline void use_all_columns()
  {
    column_bitmaps_set(getMutableShare()->all_set, getMutableShare()->all_set);
  }

  inline void default_column_bitmaps()
  {
    read_set= &def_read_set;
    write_set= &def_write_set;
  }

  /* Both of the below should go away once we can move this bit to the field objects */
  inline bool isReadSet(uint32_t index) const
  {
    return read_set->test(index);
  }

  inline void setReadSet(uint32_t index)
  {
    read_set->set(index);
  }

  inline void setReadSet()
  {
    read_set->set();
  }

  inline void clearReadSet(uint32_t index)
  {
    read_set->reset(index);
  }

  inline void clearReadSet()
  {
    read_set->reset();
  }

  inline bool isWriteSet(uint32_t index)
  {
    return write_set->test(index);
  }

  inline void setWriteSet(uint32_t index)
  {
    write_set->set(index);
  }

  inline void setWriteSet()
  {
    write_set->set();
  }

  inline void clearWriteSet(uint32_t index)
  {
    write_set->reset(index);
  }

  inline void clearWriteSet()
  {
    write_set->reset();
  }

  /* Is table open or should be treated as such by name-locking? */
  inline bool is_name_opened()
  {
    return db_stat || open_placeholder;
  }

  /*
    Is this instance of the table should be reopen or represents a name-lock?
  */
  bool needs_reopen_or_name_lock() const;

  /**
    clean/setup table fields and map.

    @param table        Table structure pointer (which should be setup)
    @param table_list   TableList structure pointer (owner of Table)
    @param tablenr     table number
  */
  void setup_table_map(TableList *table_list, uint32_t tablenr);
  inline void mark_as_null_row()
  {
    null_row= 1;
    status|= STATUS_NULL_ROW;
    memset(null_flags, 255, getShare()->null_bytes);
  }

  void free_io_cache();
  void filesort_free_buffers(bool full= false);
  void intern_close_table();

  void print_error(int error, myf errflag) const;

  /**
    @return
    key if error because of duplicated keys
  */
  uint32_t get_dup_key(int error) const
  {
    cursor->errkey  = (uint32_t) -1;
    if (error == HA_ERR_FOUND_DUPP_KEY || error == HA_ERR_FOREIGN_DUPLICATE_KEY ||
        error == HA_ERR_FOUND_DUPP_UNIQUE ||
        error == HA_ERR_DROP_INDEX_FK)
      cursor->info(HA_STATUS_ERRKEY | HA_STATUS_NO_LOCK);

    return(cursor->errkey);
  }

  /*
    This is a short term fix. Long term we will used the TableIdentifier to do the actual comparison.
  */
  bool operator<(const Table &right) const
  {
    return getShare()->getCacheKey() < right.getShare()->getCacheKey();
  }

  static bool compare(const Table *a, const Table *b)
  {
    return *a < *b;
  }

  friend std::ostream& operator<<(std::ostream& output, const Table &table)
  {
    if (table.getShare())
    {
      output << "Table:(";
      output << table.getShare()->getSchemaName();
      output << ", ";
      output <<  table.getShare()->getTableName();
      output << ", ";
      output <<  table.getShare()->getTableTypeAsString();
      output << ")";
    }
    else
    {
      output << "Table:(has no share)";
    }

    return output;  // for multiple << operators.
  }

public:
  virtual bool isPlaceHolder(void) const
  {
    return false;
  }
};

/**
 * @class
 *  ForeignKeyInfo
 *
 * @brief
 *  This class defines the information for foreign keys.
 */
class ForeignKeyInfo
{
public:
    /**
     * @brief
     *  This is the constructor with all properties set.
     *
     * @param[in] in_foreign_id The id of the foreign key
     * @param[in] in_referenced_db The referenced database name of the foreign key
     * @param[in] in_referenced_table The referenced table name of the foreign key
     * @param[in] in_update_method The update method of the foreign key.
     * @param[in] in_delete_method The delete method of the foreign key.
     * @param[in] in_referenced_key_name The name of referenced key
     * @param[in] in_foreign_fields The foreign fields
     * @param[in] in_referenced_fields The referenced fields
     */
    ForeignKeyInfo(LEX_STRING *in_foreign_id,
                   LEX_STRING *in_referenced_db,
                   LEX_STRING *in_referenced_table,
                   LEX_STRING *in_update_method,
                   LEX_STRING *in_delete_method,
                   LEX_STRING *in_referenced_key_name,
                   List<LEX_STRING> in_foreign_fields,
                   List<LEX_STRING> in_referenced_fields)
    :
      foreign_id(in_foreign_id),
      referenced_db(in_referenced_db),
      referenced_table(in_referenced_table),
      update_method(in_update_method),
      delete_method(in_delete_method),
      referenced_key_name(in_referenced_key_name),
      foreign_fields(in_foreign_fields),
      referenced_fields(in_referenced_fields)
    {}

    /**
     * @brief
     *  This is the default constructor. All properties are set to default values for their types.
     */
    ForeignKeyInfo()
    : foreign_id(NULL), referenced_db(NULL), referenced_table(NULL),
      update_method(NULL), delete_method(NULL), referenced_key_name(NULL)
    {}

    /**
     * @brief
     *  Gets the foreign id.
     *
     * @ retval  the foreign id
     */
    const LEX_STRING *getForeignId() const
    {
        return foreign_id;
    }

    /**
     * @brief
     *  Gets the name of the referenced database.
     *
     * @ retval  the name of the referenced database
     */
    const LEX_STRING *getReferencedDb() const
    {
        return referenced_db;
    }

    /**
     * @brief
     *  Gets the name of the referenced table.
     *
     * @ retval  the name of the referenced table
     */
    const LEX_STRING *getReferencedTable() const
    {
        return referenced_table;
    }

    /**
     * @brief
     *  Gets the update method.
     *
     * @ retval  the update method
     */
    const LEX_STRING *getUpdateMethod() const
    {
        return update_method;
    }

    /**
     * @brief
     *  Gets the delete method.
     *
     * @ retval  the delete method
     */
    const LEX_STRING *getDeleteMethod() const
    {
        return delete_method;
    }

    /**
     * @brief
     *  Gets the name of the referenced key.
     *
     * @ retval  the name of the referenced key
     */
    const LEX_STRING *getReferencedKeyName() const
    {
        return referenced_key_name;
    }

    /**
     * @brief
     *  Gets the foreign fields.
     *
     * @ retval  the foreign fields
     */
    const List<LEX_STRING> &getForeignFields() const
    {
        return foreign_fields;
    }

    /**
     * @brief
     *  Gets the referenced fields.
     *
     * @ retval  the referenced fields
     */
    const List<LEX_STRING> &getReferencedFields() const
    {
        return referenced_fields;
    }
private:
    /**
     * The foreign id.
     */
    LEX_STRING *foreign_id;
    /**
     * The name of the reference database.
     */
    LEX_STRING *referenced_db;
    /**
     * The name of the reference table.
     */
    LEX_STRING *referenced_table;
    /**
     * The update method.
     */
    LEX_STRING *update_method;
    /**
     * The delete method.
     */
    LEX_STRING *delete_method;
    /**
     * The name of the referenced key.
     */
    LEX_STRING *referenced_key_name;
    /**
     * The foreign fields.
     */
    List<LEX_STRING> foreign_fields;
    /**
     * The referenced fields.
     */
    List<LEX_STRING> referenced_fields;
};

#define JOIN_TYPE_LEFT  1
#define JOIN_TYPE_RIGHT 2

void free_blobs(Table *table);
int set_zone(int nr,int min_zone,int max_zone);
uint32_t convert_period_to_month(uint32_t period);
uint32_t convert_month_to_period(uint32_t month);

int test_if_number(char *str,int *res,bool allow_wildcards);
void change_byte(unsigned char *,uint,char,char);
void change_double_for_sort(double nr,unsigned char *to);
int get_quick_record(optimizer::SqlSelect *select);

void find_date(char *pos,uint32_t *vek,uint32_t flag);
TYPELIB* convert_strings_to_array_type(char** typelibs, char** end);
TYPELIB* typelib(memory::Root&, List<String>&);
ulong get_form_pos(int file, unsigned char *head, TYPELIB *save_names);
void append_unescaped(String *res, const char *pos, uint32_t length);

bool check_column_name(const char *name);
bool check_table_name(const char *name, uint32_t length);

} /* namespace drizzled */

#include <drizzled/table/singular.h>
#include <drizzled/table/concurrent.h>

