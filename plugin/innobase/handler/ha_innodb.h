/*****************************************************************************

Copyright (c) 2000, 2009, MySQL AB & Innobase Oy. All Rights Reserved.

This program is free software; you can redistribute it and/or modify it under
the terms of the GNU General Public License as published by the Free Software
Foundation; version 2 of the License.

This program is distributed in the hope that it will be useful, but WITHOUT
ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along with
this program; if not, write to the Free Software Foundation, Inc., 59 Temple
Place, Suite 330, Boston, MA 02111-1307 USA

*****************************************************************************/

/*
  This file is based on ha_berkeley.h of MySQL distribution

  This file defines the Innodb handler: the interface between MySQL and
  Innodb
*/

#ifndef INNODB_HANDLER_HA_INNODB_H
#define INNODB_HANDLER_HA_INNODB_H

#include <drizzled/handler.h>
#include <mysys/thr_lock.h>

#ifdef USE_PRAGMA_INTERFACE
#pragma interface			/* gcc class implementation */
#endif

typedef struct st_innobase_share {
  THR_LOCK lock;
  pthread_mutex_t mutex;
  const char* table_name;
  uint use_count;
  void* table_name_hash;
} INNOBASE_SHARE;


struct dict_index_struct;
struct row_prebuilt_struct;

typedef struct dict_index_struct dict_index_t;
typedef struct row_prebuilt_struct row_prebuilt_t;

/* The class defining a handle to an Innodb table */
class ha_innobase: public handler
{
	row_prebuilt_t*	prebuilt;	/* prebuilt struct in InnoDB, used
					to save CPU time with prebuilt data
					structures*/
	Session*		user_session;	/* the thread handle of the user
					currently using the handle; this is
					set in external_lock function */
	THR_LOCK_DATA	lock;
	INNOBASE_SHARE	*share;

	unsigned char*		upd_buff;	/* buffer used in updates */
	unsigned char*		key_val_buff;	/* buffer used in converting
					search key values from MySQL format
					to Innodb format */
	ulong		upd_and_key_val_buff_len;
					/* the length of each of the previous
					two buffers */
	Table_flags	int_table_flags;
	uint		primary_key;
	ulong		start_of_scan;	/* this is set to 1 when we are
					starting a table scan but have not
					yet fetched any row, else 0 */
	uint		last_match_mode;/* match mode of the latest search:
					ROW_SEL_EXACT, ROW_SEL_EXACT_PREFIX,
					or undefined */
	uint		num_write_row;	/* number of write_row() calls */

	uint store_key_val_for_row(uint keynr, char* buff, uint buff_len,
                                   const unsigned char* record);
	void update_session(Session* session);
	void update_session();
	int change_active_index(uint32_t keynr);
	int general_fetch(unsigned char* buf, uint32_t direction, uint32_t match_mode);
	ulint innobase_lock_autoinc();
	uint64_t innobase_peek_autoinc();
	ulint innobase_set_max_autoinc(uint64_t auto_inc);
	ulint innobase_reset_autoinc(uint64_t auto_inc);
	ulint innobase_get_autoinc(uint64_t* value);
	ulint innobase_update_autoinc(uint64_t	auto_inc);
	ulint innobase_initialize_autoinc();
	dict_index_t* innobase_get_index(uint keynr);
 	uint64_t innobase_get_int_col_max_value(const Field* field);

	/* Init values for the class: */
 public:
	ha_innobase(StorageEngine *engine, TableShare *table_arg);
	~ha_innobase();
	/*
	  Get the row type from the storage engine.  If this method returns
	  ROW_TYPE_NOT_USED, the information in HA_CREATE_INFO should be used.
	*/
	enum row_type get_row_type() const;

	const char* table_type() const;
	const char* index_type(uint key_number);
	const char** bas_ext() const;
	Table_flags table_flags() const;
	uint32_t index_flags(uint idx, uint part, bool all_parts) const;
	uint32_t max_supported_keys() const;
	uint32_t max_supported_key_length() const;
	uint32_t max_supported_key_part_length() const;
	const key_map* keys_to_use_for_scanning();

	int open(const char *name, int mode, uint test_if_locked);
	int close(void);
	double scan_time();
	double read_time(uint index, uint ranges, ha_rows rows);

	int write_row(unsigned char * buf);
	int update_row(const unsigned char * old_data, unsigned char * new_data);
	int delete_row(const unsigned char * buf);
	bool was_semi_consistent_read();
	void try_semi_consistent_read(bool yes);
	void unlock_row();

#ifdef ROW_MERGE_IS_INDEX_USABLE
	/** Check if an index can be used by this transaction.
	* @param keynr	key number to check
	* @return	true if available, false if the index
	*		does not contain old records that exist
	*		in the read view of this transaction */
	bool is_index_available(uint keynr);
#endif /* ROW_MERGE_IS_INDEX_USABLE */
	int index_init(uint index, bool sorted);
	int index_end();
	int index_read(unsigned char * buf, const unsigned char * key,
		uint key_len, enum ha_rkey_function find_flag);
	int index_read_idx(unsigned char * buf, uint index, const unsigned char * key,
			   uint key_len, enum ha_rkey_function find_flag);
	int index_read_last(unsigned char * buf, const unsigned char * key, uint key_len);
	int index_next(unsigned char * buf);
	int index_next_same(unsigned char * buf, const unsigned char *key, uint keylen);
	int index_prev(unsigned char * buf);
	int index_first(unsigned char * buf);
	int index_last(unsigned char * buf);

	int rnd_init(bool scan);
	int rnd_end();
	int rnd_next(unsigned char *buf);
	int rnd_pos(unsigned char * buf, unsigned char *pos);

	void position(const unsigned char *record);
	int info(uint);
	int analyze(Session* session,HA_CHECK_OPT* check_opt);
	int optimize(Session* session,HA_CHECK_OPT* check_opt);
	int discard_or_import_tablespace(bool discard);
	int extra(enum ha_extra_function operation);
        int reset();
	int external_lock(Session *session, int lock_type);
	int transactional_table_lock(Session *session, int lock_type);
	int start_stmt(Session *session, thr_lock_type lock_type);
	void position(unsigned char *record);
	ha_rows records_in_range(uint inx, key_range *min_key, key_range
								*max_key);
	ha_rows estimate_rows_upper_bound();

	void update_create_info(HA_CREATE_INFO* create_info);
	int create(const char *name, Table *form,
		   HA_CREATE_INFO *create_info);
	int delete_all_rows();
	int delete_table(const char *name);
	int rename_table(const char* from, const char* to);
	int check(Session* session, HA_CHECK_OPT* check_opt);
	char* update_table_comment(const char* comment);
	char* get_foreign_key_create_info();
	int get_foreign_key_list(Session *session, List<FOREIGN_KEY_INFO> *f_key_list);
	bool can_switch_engines();
	uint referenced_by_foreign_key();
	void free_foreign_key_create_info(char* str);
	THR_LOCK_DATA **store_lock(Session *session, THR_LOCK_DATA **to,
					enum thr_lock_type lock_type);
	void init_table_handle_for_HANDLER();
        virtual void get_auto_increment(uint64_t offset, uint64_t increment,
                                        uint64_t nb_desired_values,
                                        uint64_t *first_value,
                                        uint64_t *nb_reserved_values);
	int reset_auto_increment(uint64_t value);

	virtual bool get_error_message(int error, String *buf);

	/*
	  ask handler about permission to cache table during query registration
	*/
        bool register_query_cache_table(Session *session, char *table_key,
                                        uint32_t key_length,
                                        qc_engine_callback *call_back,
                                        uint64_t *engine_data);
	static char *get_mysql_bin_log_name();
	static uint64_t get_mysql_bin_log_pos();
	bool primary_key_is_clustered();
	int cmp_ref(const unsigned char *ref1, const unsigned char *ref2);
	/** Fast index creation (smart ALTER TABLE) @see handler0alter.cc @{ */
	int add_index(TABLE *table_arg, KEY *key_info, uint num_of_keys);
	int prepare_drop_index(TABLE *table_arg, uint *key_num,
			       uint num_of_keys);
	int final_drop_index(TABLE *table_arg);
	/** @} */
	bool check_if_incompatible_data(HA_CREATE_INFO *info,
					uint32_t table_changes);
	bool check_if_supported_virtual_columns(void) { return true; }
public:
  /**
   * Multi Range Read interface
   */
  int multi_range_read_init(RANGE_SEQ_IF *seq, void *seq_init_param,
			    uint32_t n_ranges, uint32_t mode,
			    HANDLER_BUFFER *buf);
  int multi_range_read_next(char **range_info);
  ha_rows multi_range_read_info_const(uint32_t keyno, RANGE_SEQ_IF *seq,
				      void *seq_init_param,
				      uint32_t n_ranges, uint32_t *bufsz,
				      uint32_t *flags, COST_VECT *cost);
  int multi_range_read_info(uint32_t keyno, uint32_t n_ranges, uint32_t keys,
			    uint32_t *bufsz, uint32_t *flags, COST_VECT *cost);
  DsMrr_impl ds_mrr;

  int read_range_first(const key_range *start_key, const key_range *end_key,
		       bool eq_range_arg, bool sorted);
  int read_range_next();
  Item *idx_cond_push(uint32_t keyno, Item* idx_cond);

};


extern "C" {
struct charset_info_st *session_charset(Session *session);
char **session_query(Session *session);

/** Get the file name of the MySQL binlog.
 * @return the name of the binlog file
 */
const char* drizzle_bin_log_file_name(void);

/** Get the current position of the MySQL binlog.
 * @return byte offset from the beginning of the binlog
 */
uint64_t drizzle_bin_log_file_pos(void);

/**
  Check if a user thread is a replication slave thread
  @param session  user thread
  @retval 0 the user thread is not a replication slave thread
  @retval 1 the user thread is a replication slave thread
*/
int session_slave_thread(const Session *session);

/**
  Check if a user thread is running a non-transactional update
  @param session  user thread
  @retval 0 the user thread is not running a non-transactional update
  @retval 1 the user thread is running a non-transactional update
*/
int session_non_transactional_update(const Session *session);

/**
  Get the user thread's binary logging format
  @param session  user thread
  @return Value to be used as index into the binlog_format_names array
*/
int session_binlog_format(const Session *session);

/**
  Mark transaction to rollback and mark error as fatal to a sub-statement.
  @param  session   Thread handle
  @param  all   TRUE <=> rollback main transaction.
*/
void session_mark_transaction_to_rollback(Session *session, bool all);
}

typedef struct trx_struct trx_t;
/************************************************************************
Converts an InnoDB error code to a MySQL error code and also tells to MySQL
about a possible transaction rollback inside InnoDB caused by a lock wait
timeout or a deadlock. */
extern "C"
int
convert_error_code_to_mysql(
/*========================*/
					/* out: MySQL error code */
	int		error,		/* in: InnoDB error code */
	ulint		flags,		/* in: InnoDB table flags, or 0 */
	Session	        *session);	/* in: user thread handle or NULL */

/*************************************************************************
Allocates an InnoDB transaction for a MySQL handler object. */
extern "C"
trx_t*
innobase_trx_allocate(
/*==================*/
					/* out: InnoDB transaction handle */
	Session		*session);	/* in: user thread handle */
#endif /* INNODB_HANDLER_HA_INNODB_H */
