/* -*- mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; -*-
 *  vim:expandtab:shiftwidth=2:tabstop=2:smarttab:
 *
 *  Copyright (C) 2008 Sun Microsystems, Inc.
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

#include <drizzled/atomics.h>
#include <drizzled/definitions.h>
#include <drizzled/discrete_interval.h>
#include <drizzled/error_t.h>
#include <drizzled/ha_statistics.h>
#include <drizzled/handler_structs.h>
#include <drizzled/identifier.h>
#include <drizzled/key_map.h>
#include <drizzled/message/table.h>
#include <drizzled/sql_list.h>
#include <drizzled/thr_lock.h>

#include <bitset>
#include <algorithm>

#include <drizzled/visibility.h>

namespace drizzled {

#define HA_MAX_ALTER_FLAGS 40

typedef std::bitset<HA_MAX_ALTER_FLAGS> HA_ALTER_FLAGS;

typedef List<Item> List_item;
extern KEY_CREATE_INFO default_key_create_info;

/*
  bitmap with first N+1 bits set
  (keypart_map for a key prefix of [0..N] keyparts)
*/
inline key_part_map make_keypart_map(int a)
{
  return (((key_part_map)2 << a) - 1);
}

/*
  bitmap with first N bits set
  (keypart_map for a key prefix of [0..N-1] keyparts)
*/
inline key_part_map make_prev_keypart_map(int a)
{
  return (((key_part_map)1 << a) - 1);
}

/**
  The Cursor class is the interface for dynamically loadable
  storage engines. Do not add ifdefs and take care when adding or
  changing virtual functions to avoid vtable confusion

  Functions in this class accept and return table columns data. Two data
  representation formats are used:
  1. TableRecordFormat - Used to pass [partial] table records to/from
     storage engine

  2. KeyTupleFormat - used to pass index search tuples (aka "keys") to
     storage engine. See optimizer/range.cc for description of this format.

  TableRecordFormat
  =================
  [Warning: this description is work in progress and may be incomplete]
  The table record is stored in a fixed-size buffer:

    record: null_bytes, column1_data, column2_data, ...

  The offsets of the parts of the buffer are also fixed: every column has
  an offset to its column{i}_data, and if it is nullable it also has its own
  bit in null_bytes.

  The record buffer only includes data about columns that are marked in the
  relevant column set (table->read_set and/or table->write_set, depending on
  the situation).
  <not-sure>It could be that it is required that null bits of non-present
  columns are set to 1</not-sure>

  VARIOUS EXCEPTIONS AND SPECIAL CASES

  f the table has no nullable columns, then null_bytes is still
  present, its length is one byte <not-sure> which must be set to 0xFF
  at all times. </not-sure>

  For blob columns (see Field_blob), the record buffer stores length of the
  data, following by memory pointer to the blob data. The pointer is owned
  by the storage engine and is valid until the next operation.

  If a blob column has NULL value, then its length and blob data pointer
  must be set to 0.
*/
class DRIZZLED_API Cursor
{
  friend class SEAPITesterCursor;
  Table &table;               /* The current open table */
  plugin::StorageEngine &engine;      /* storage engine of this Cursor */

protected:
  ha_rows estimation_rows_to_insert;

public:
  inline plugin::StorageEngine *getEngine() const	/* table_type for handler */
  {
    return &engine;
  }
  unsigned char *ref;				/* Pointer to current row */
  unsigned char *dup_ref;			/* Pointer to duplicate row */

  TableShare *getShare();

  Table *getTable() const
  {
    return &table;
  }

  ha_statistics stats;
  /** MultiRangeRead-related members: */
  range_seq_t mrr_iter;    /* Interator to traverse the range sequence */
  RANGE_SEQ_IF mrr_funcs;  /* Range sequence traversal functions */

  uint32_t ranges_in_seq; /* Total number of ranges in the traversed sequence */
  /* true <=> source MRR ranges and the output are ordered */
  bool mrr_is_output_sorted;

  /** true <=> we're currently traversing a range in mrr_cur_range. */
  bool mrr_have_range;

  bool eq_range;

  /** Current range (the one we're now returning rows from) */
  KEY_MULTI_RANGE mrr_cur_range;

  /** The following are for read_range() */
  key_range save_end_range, *end_range;
  KeyPartInfo *range_key_part;
  int key_compare_result_on_equal;

  uint32_t errkey;				/* Last dup key */
  uint32_t key_used_on_scan;
  uint32_t active_index;
  /** Length of ref (1-8 or the clustered key length) */
  uint32_t ref_length;
  enum {NONE=0, INDEX, RND} inited;
  bool locked;

  /**
    next_insert_id is the next value which should be inserted into the
    auto_increment column: in a inserting-multi-row statement (like INSERT
    SELECT), for the first row where the autoinc value is not specified by the
    statement, get_auto_increment() called and asked to generate a value,
    next_insert_id is set to the next value, then for all other rows
    next_insert_id is used (and increased each time) without calling
    get_auto_increment().
  */
  uint64_t next_insert_id;
  uint64_t getNextInsertId()
  {
    return next_insert_id;
  }

  /**
    Used by SHOW TABLE STATUS to get the current auto_inc from the engine
  */
  uint64_t getAutoIncrement()
  {
    return stats.auto_increment_value;
  }

  /**
    insert id for the current row (*autogenerated*; if not
    autogenerated, it's 0).
    At first successful insertion, this variable is stored into
    Session::first_successful_insert_id_in_cur_stmt.
  */
  uint64_t insert_id_for_cur_row;
  /**
    Interval returned by get_auto_increment() and being consumed by the
    inserter.
  */
  Discrete_interval auto_inc_interval_for_cur_row;

  Cursor(plugin::StorageEngine &engine_arg, Table &share_arg);
  virtual ~Cursor(void);
  virtual Cursor *clone(memory::Root *mem_root);

  /* ha_ methods: pubilc wrappers for private virtual API */

  int ha_open(const identifier::Table &identifier, int mode, int test_if_locked);
  int startIndexScan(uint32_t idx, bool sorted) __attribute__ ((warn_unused_result));
  int endIndexScan();
  int startTableScan(bool scan) __attribute__ ((warn_unused_result));
  int endTableScan();
  int ha_reset();

  /* this is necessary in many places, e.g. in HANDLER command */
  int ha_index_or_rnd_end();

  /**
    These functions represent the public interface to *users* of the
    Cursor class, hence they are *not* virtual. For the inheritance
    interface, see the (private) functions doInsertRecord(), doUpdateRecord(),
    and doDeleteRecord() below.
  */
  int ha_external_lock(Session *session, int lock_type);
  int insertRecord(unsigned char * buf) __attribute__ ((warn_unused_result));
  int updateRecord(const unsigned char * old_data, unsigned char * new_data) __attribute__ ((warn_unused_result));
  int deleteRecord(const unsigned char * buf) __attribute__ ((warn_unused_result));
  void ha_release_auto_increment();

  /** to be actually called to get 'check()' functionality*/
  int ha_check(Session*);

  void ha_start_bulk_insert(ha_rows rows);
  int ha_end_bulk_insert();
  int ha_delete_all_rows();
  int ha_reset_auto_increment(uint64_t value);
  int ha_analyze(Session*);

  int ha_disable_indexes(uint32_t mode);
  int ha_enable_indexes(uint32_t mode);
  int ha_discard_or_import_tablespace(bool discard);
  void closeMarkForDelete(const char *name);

  void adjust_next_insert_id_after_explicit_value(uint64_t nr);
  int update_auto_increment();

  /* Estimates calculation */
  virtual double scan_time(void)
  { return static_cast<double>(stats.data_file_length) / IO_SIZE + 2; }
  virtual double read_time(uint32_t, uint32_t ranges, ha_rows rows)
  { return ranges + rows; }

  virtual double index_only_read_time(uint32_t keynr, double records);

  virtual ha_rows multi_range_read_info_const(uint32_t keyno, RANGE_SEQ_IF *seq,
                                              void *seq_init_param,
                                              uint32_t n_ranges, uint32_t *bufsz,
                                              uint32_t *flags, optimizer::CostVector *cost);
  virtual int multi_range_read_info(uint32_t keyno, uint32_t n_ranges, uint32_t keys,
                                    uint32_t *bufsz, uint32_t *flags, optimizer::CostVector *cost);
  virtual int multi_range_read_init(RANGE_SEQ_IF *seq, void *seq_init_param,
                                    uint32_t n_ranges, uint32_t mode);
  virtual int multi_range_read_next(char **range_info);


  virtual const key_map *keys_to_use_for_scanning();
  bool has_transactions();

  /**
    This method is used to analyse the error to see whether the error
    is ignorable or not, certain handlers can have more error that are
    ignorable than others. E.g. the partition Cursor can get inserts
    into a range where there is no partition and this is an ignorable
    error.
    HA_ERR_FOUND_DUP_UNIQUE is a special case in MyISAM that means the
    same thing as HA_ERR_FOUND_DUP_KEY but can in some cases lead to
    a slightly different error message.
  */
  virtual bool is_fatal_error(int error, uint32_t flags);

  /**
    Number of rows in table. It will only be called if
    (table_flags() & (HA_HAS_RECORDS | HA_STATS_RECORDS_IS_EXACT)) != 0
  */
  virtual ha_rows records();
  virtual uint64_t tableSize();
  virtual uint64_t rowSize();
  /**
    Return upper bound of current number of records in the table
    (max. of how many records one will retrieve when doing a full table scan)
    If upper bound is not known, HA_POS_ERROR should be returned as a max
    possible upper bound.
  */
  virtual ha_rows estimate_rows_upper_bound()
  { return stats.records+EXTRA_RECORDS; }

  virtual const char *index_type(uint32_t)
  { assert(0); return "";}


  uint32_t get_index(void) const { return active_index; }
  virtual int close(void)=0;

  /**
     @brief
     Positions an index cursor to the index specified in the handle. Fetches the
     row if available. If the key value is null, begin at the first key of the
     index.
  */
  virtual int index_read_map(unsigned char * buf, const unsigned char *key,
                             key_part_map keypart_map,
                             enum ha_rkey_function find_flag)
  {
    uint32_t key_len= calculate_key_len(active_index, keypart_map);
    return  index_read(buf, key, key_len, find_flag);
  }
  /**
     @brief
     Positions an index cursor to the index specified in the handle. Fetches the
     row if available. If the key value is null, begin at the first key of the
     index.
  */
  virtual int index_read_idx_map(unsigned char * buf, uint32_t index,
                                 const unsigned char * key,
                                 key_part_map keypart_map,
                                 enum ha_rkey_function find_flag);
  virtual int index_next(unsigned char *) __attribute__ ((warn_unused_result))
   { return  HA_ERR_WRONG_COMMAND; }
  virtual int index_prev(unsigned char *)
   { return  HA_ERR_WRONG_COMMAND; }
  virtual int index_first(unsigned char *)
   { return  HA_ERR_WRONG_COMMAND; }
  virtual int index_last(unsigned char *)
   { return  HA_ERR_WRONG_COMMAND; }
  virtual int index_next_same(unsigned char *, const unsigned char *, uint32_t);

private:
  uint32_t calculate_key_len(uint32_t key_position, key_part_map keypart_map_arg);
public:

  /**
     @brief
     The following functions works like index_read, but it find the last
     row with the current key value or prefix.
  */
  virtual int index_read_last_map(unsigned char * buf, const unsigned char * key,
                                  key_part_map keypart_map)
  {
    uint32_t key_len= calculate_key_len(active_index, keypart_map);
    return index_read_last(buf, key, key_len);
  }
  virtual int read_range_first(const key_range *start_key,
                               const key_range *end_key,
                               bool eq_range, bool sorted);
  virtual int read_range_next();
  int compare_key(key_range *range);
  virtual int rnd_next(unsigned char *)=0;
  virtual int rnd_pos(unsigned char *, unsigned char *)=0;
  virtual int read_first_row(unsigned char *buf, uint32_t primary_key);
  virtual int rnd_same(unsigned char *, uint32_t)
    { return HA_ERR_WRONG_COMMAND; }
  virtual ha_rows records_in_range(uint32_t, key_range *, key_range *)
    { return (ha_rows) 10; }
  virtual void position(const unsigned char *record)=0;
  virtual int info(uint32_t)=0; // see my_base.h for full description
  virtual uint32_t calculate_key_hash_value(Field **)
  { assert(0); return 0; }
  virtual int extra(enum ha_extra_function)
  { return 0; }
  virtual int extra_opt(enum ha_extra_function operation, uint32_t)
  { return extra(operation); }

  /**
    In an UPDATE or DELETE, if the row under the cursor was locked by another
    transaction, and the engine used an optimistic read of the last
    committed row value under the cursor, then the engine returns 1 from this
    function. MySQL must NOT try to update this optimistic value. If the
    optimistic value does not match the WHERE condition, MySQL can decide to
    skip over this row. Currently only works for InnoDB. This can be used to
    avoid unnecessary lock waits.

    If this method returns nonzero, it will also signal the storage
    engine that the next read will be a locking re-read of the row.
  */
  virtual bool was_semi_consistent_read() { return 0; }
  /**
    Tell the engine whether it should avoid unnecessary lock waits.
    If yes, in an UPDATE or DELETE, if the row under the cursor was locked
    by another transaction, the engine may try an optimistic read of
    the last committed row value under the cursor.
  */
  virtual void try_semi_consistent_read(bool) {}
  virtual void unlock_row(void) {}
  virtual void get_auto_increment(uint64_t offset, uint64_t increment,
                                  uint64_t nb_desired_values,
                                  uint64_t *first_value,
                                  uint64_t *nb_reserved_values)= 0;

  void set_next_insert_id(uint64_t id)
  {
    next_insert_id= id;
  }
  void restore_auto_increment(uint64_t prev_insert_id)
  {
    /*
      Insertion of a row failed, re-use the lastly generated auto_increment
      id, for the next row. This is achieved by resetting next_insert_id to
      what it was before the failed insertion (that old value is provided by
      the caller). If that value was 0, it was the first row of the INSERT;
      then if insert_id_for_cur_row contains 0 it means no id was generated
      for this first row, so no id was generated since the INSERT started, so
      we should set next_insert_id to 0; if insert_id_for_cur_row is not 0, it
      is the generated id of the first and failed row, so we use it.
    */
    next_insert_id= (prev_insert_id > 0) ? prev_insert_id :
      insert_id_for_cur_row;
  }

  /* end of the list of admin commands */

  virtual int indexes_are_disabled(void) {return 0;}
  virtual void append_create_info(String *)
  {}
  /**
      If index == MAX_KEY then a check for table is made and if index <
      MAX_KEY then a check is made if the table has foreign keys and if
      a foreign key uses this index (and thus the index cannot be dropped).

    @param  index            Index to check if foreign key uses it

    @retval   true            Foreign key defined on table or index
    @retval   false           No foreign key defined
  */
  virtual char* get_foreign_key_create_info(void)
  { return NULL;}  /* gets foreign key create string from InnoDB */
  /** used in ALTER Table; if changing storage engine is allowed.
      e.g. not be allowed if table has foreign key constraints in engine.
   */
  virtual bool can_switch_engines(void) { return true; }
  /** used in REPLACE; is > 0 if table is referred by a FOREIGN KEY */
  virtual int get_foreign_key_list(Session *, List<ForeignKeyInfo> *)
  { return 0; }
  virtual uint32_t referenced_by_foreign_key() { return 0;}
  virtual void free_foreign_key_create_info(char *) {}

  /**
    Is not invoked for non-transactional temporary tables.

    @note store_lock() can return more than one lock if the table is MERGE
    or partitioned.

    @note that one can NOT rely on table->in_use in store_lock().  It may
    refer to a different thread if called from abortLockForThread().

    @note If the table is MERGE, store_lock() can return less locks
    than lock_count() claimed. This can happen when the MERGE children
    are not attached when this is called from another thread.
  */
  virtual THR_LOCK_DATA **store_lock(Session *,
                                     THR_LOCK_DATA **to,
                                     enum thr_lock_type)
  {
    assert(0); // Impossible programming situation

    return(to);
  }

 /*
   @retval true   Primary key (if there is one) is clustered
                  key covering all fields
   @retval false  otherwise
 */
 virtual bool primary_key_is_clustered() { return false; }
 virtual int cmp_ref(const unsigned char *ref1, const unsigned char *ref2)
 {
   return memcmp(ref1, ref2, ref_length);
 }

  virtual bool isOrdered(void)
  {
    return false;
  }


protected:
  /* Service methods for use by storage engines. */
  void ha_statistic_increment(uint64_t system_status_var::*offset) const;
  void **ha_data(Session *) const;

private:
  /* Private helpers */
  inline void setTransactionReadWrite();
private:
  /*
    Low-level primitives for storage engines.  These should be
    overridden by the storage engine class. To call these methods, use
    the corresponding 'ha_*' method above.
  */

  virtual int open(const char *, int , uint32_t ) { assert(0); return -1; }
  virtual int doOpen(const identifier::Table &identifier, int mode, uint32_t test_if_locked);
  virtual int doStartIndexScan(uint32_t idx, bool)
  { active_index= idx; return 0; }
  virtual int doEndIndexScan() { active_index= MAX_KEY; return 0; }
  /**
    doStartTableScan() can be called two times without doEndTableScan() in between
    (it only makes sense if scan=1).
    then the second call should prepare for the new table scan (e.g
    if rnd_init allocates the cursor, second call should position it
    to the start of the table, no need to deallocate and allocate it again
  */
  virtual int doStartTableScan(bool scan) __attribute__ ((warn_unused_result)) = 0;
  virtual int doEndTableScan() { return 0; }
  virtual int doInsertRecord(unsigned char *)
  {
    return HA_ERR_WRONG_COMMAND;
  }

  virtual int doUpdateRecord(const unsigned char *, unsigned char *)
  {
    return HA_ERR_WRONG_COMMAND;
  }

  virtual int doDeleteRecord(const unsigned char *)
  {
    return HA_ERR_WRONG_COMMAND;
  }
  /**
    Reset state of file to after 'open'.
    This function is called after every statement for all tables used
    by that statement.
  */
  virtual int reset() { return 0; }

  /**
    Is not invoked for non-transactional temporary tables.

    Tells the storage engine that we intend to read or write data
    from the table. This call is prefixed with a call to Cursor::store_lock()
    and is invoked only for those Cursor instances that stored the lock.

    Calls to rnd_init/index_init are prefixed with this call. When table
    IO is complete, we call external_lock(F_UNLCK).
    A storage engine writer should expect that each call to
    ::external_lock(F_[RD|WR]LOCK is followed by a call to
    ::external_lock(F_UNLCK). If it is not, it is a bug in MySQL.

    The name and signature originate from the first implementation
    in MyISAM, which would call fcntl to set/clear an advisory
    lock on the data file in this method.

    @param   lock_type    F_RDLCK, F_WRLCK, F_UNLCK

    @return  non-0 in case of failure, 0 in case of success.
    When lock_type is F_UNLCK, the return value is ignored.
  */
  virtual int external_lock(Session *, int)
  {
    return 0;
  }
  virtual void release_auto_increment(void) { return; }
  /** admin commands - called from mysql_admin_table */
  virtual int check(Session *)
  { return HA_ADMIN_NOT_IMPLEMENTED; }

  virtual void start_bulk_insert(ha_rows)
  {}
  virtual int end_bulk_insert(void) { return 0; }
  virtual int index_read(unsigned char *, const unsigned char *,
                         uint32_t, enum ha_rkey_function)
   { return  HA_ERR_WRONG_COMMAND; }
  virtual int index_read_last(unsigned char *, const unsigned char *, uint32_t)
   { return (errno= HA_ERR_WRONG_COMMAND); }
  /**
    This is called to delete all rows in a table
    If the Cursor don't support this, then this function will
    return HA_ERR_WRONG_COMMAND and MySQL will delete the rows one
    by one.
  */
  virtual int delete_all_rows(void)
  { return (errno=HA_ERR_WRONG_COMMAND); }
  /**
    Reset the auto-increment counter to the given value, i.e. the next row
    inserted will get the given value. This is called e.g. after TRUNCATE
    is emulated by doing a 'DELETE FROM t'. HA_ERR_WRONG_COMMAND is
    returned by storage engines that don't support this operation.
  */
  virtual int reset_auto_increment(uint64_t)
  { return HA_ERR_WRONG_COMMAND; }

  virtual int analyze(Session *)
  { return HA_ADMIN_NOT_IMPLEMENTED; }

  virtual int disable_indexes(uint32_t)
  { return HA_ERR_WRONG_COMMAND; }

  virtual int enable_indexes(uint32_t)
  { return HA_ERR_WRONG_COMMAND; }

  virtual int discard_or_import_tablespace(bool)
  { return (errno=HA_ERR_WRONG_COMMAND); }

  /* 
    @todo this is just for the HEAP engine, it should
    be removed at some point in the future (and
    no new engine should ever use it). Right
    now HEAP does rely on it, so we cannot remove it.
  */
  virtual void drop_table(const char *name);
};

extern const char *ha_row_type[];

/* basic stuff */
void ha_init_errors(void);

SortField *make_unireg_sortorder(Order *order, uint32_t *length,
                                 SortField *sortorder);
int setup_order(Session *session, Item **ref_pointer_array, TableList *tables,
                List<Item> &fields, List <Item> &all_fields, Order *order);
int setup_group(Session *session, Item **ref_pointer_array, TableList *tables,
                List<Item> &fields, List<Item> &all_fields, Order *order,
                bool *hidden_group_fields);
bool fix_inner_refs(Session *session, List<Item> &all_fields, Select_Lex *select,
                    Item **ref_pointer_array);

bool handle_select(Session *session, LEX *lex, select_result *result,
                   uint64_t setup_tables_done_option);
void free_underlaid_joins(Session *session, Select_Lex *select);

bool handle_derived(LEX *lex, bool (*processor)(Session *session,
                                                      LEX *lex,
                                                      TableList *table));
bool derived_prepare(Session *session, LEX *lex, TableList *t);
bool derived_filling(Session *session, LEX *lex, TableList *t);
int prepare_create_field(CreateField *sql_field,
                         uint32_t *blob_columns,
                         int *timestamps, int *timestamps_with_niladic);

bool create_table(Session *session,
                  const identifier::Table &identifier,
                  HA_CREATE_INFO *create_info,
                  message::Table &table_proto,
                  AlterInfo *alter_info,
                  bool tmp_table, uint32_t select_field_count,
                  bool is_if_not_exists);

bool create_table_no_lock(Session *session,
                          const identifier::Table &identifier,
                          HA_CREATE_INFO *create_info,
                          message::Table &table_proto,
                          AlterInfo *alter_info,
                          bool tmp_table, uint32_t select_field_count,
                          bool is_if_not_exists);

bool create_like_table(Session* session,
                       const identifier::Table& destination_identifier,
                       const identifier::Table& source_identifier,
                       message::Table &create_table_proto,
                       bool is_if_not_exists,
                       bool is_engine_set);

bool rename_table(Session &session,
                        plugin::StorageEngine *base,
                        const identifier::Table &old_identifier,
                        const identifier::Table &new_identifier);

bool prepare_update(Session *session, TableList *table_list,
                          Item **conds, uint32_t order_num, Order *order);
int update_query(Session *session,TableList *tables,List<Item> &fields,
                 List<Item> &values,COND *conds,
                 uint32_t order_num, Order *order, ha_rows limit,
                 enum enum_duplicates handle_duplicates, bool ignore);
bool prepare_insert(Session *session, TableList *table_list, Table *table,
                          List<Item> &fields, List_item *values,
                          List<Item> &update_fields,
                          List<Item> &update_values, enum_duplicates duplic,
                          COND **where, bool select_insert,
                          bool check_fields, bool abort_on_warning);
bool insert_query(Session *session,TableList *table,List<Item> &fields,
                  List<List_item> &values, List<Item> &update_fields,
                  List<Item> &update_values, enum_duplicates flag,
                  bool ignore);
int check_that_all_fields_are_given_values(Session *session, Table *entry,
                                           TableList *table_list);
int prepare_delete(Session *session, TableList *table_list, Item **conds);
bool delete_query(Session *session, TableList *table_list, COND *conds,
                  SQL_LIST *order, ha_rows rows, uint64_t options,
                  bool reset_auto_increment);
bool truncate(Session& session, TableList *table_list);
TableShare *get_table_share(Session *session, TableList *table_list, char *key,
                             uint32_t key_length, uint32_t db_flags, int *error);
TableShare *get_cached_table_share(const char *db, const char *table_name);
bool reopen_name_locked_table(Session* session, TableList* table_list, bool link_in);
bool reopen_tables(Session *session,bool get_locks,bool in_refresh);
void close_handle_and_leave_table_as_lock(Table *table);
bool wait_for_tables(Session *session);
bool table_is_used(Table *table, bool wait_for_name_lock);
Table *drop_locked_tables(Session *session, const drizzled::identifier::Table &identifier);
void abort_locked_tables(Session *session, const drizzled::identifier::Table &identifier);
extern Field *not_found_field;
extern Field *view_ref_found;

Field *
find_field_in_tables(Session *session, Item_ident *item,
                     TableList *first_table, TableList *last_table,
                     Item **ref, find_item_error_report_type report_error,
                     bool register_tree_change);
Field *
find_field_in_table_ref(Session *session, TableList *table_list,
                        const char *name, uint32_t length,
                        const char *item_name, const char *db_name,
                        const char *table_name, Item **ref,
                        bool allow_rowid,
                        uint32_t *cached_field_index_ptr,
                        bool register_tree_change, TableList **actual_table);
Field *
find_field_in_table(Session *session, Table *table, const char *name, uint32_t length,
                    bool allow_rowid, uint32_t *cached_field_index_ptr);

} /* namespace drizzled */

