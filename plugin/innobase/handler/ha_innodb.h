/*****************************************************************************

Copyright (C) 2000, 2010, MySQL AB & Innobase Oy. All Rights Reserved.

This program is free software; you can redistribute it and/or modify it under
the terms of the GNU General Public License as published by the Free Software
Foundation; version 2 of the License.

This program is distributed in the hope that it will be useful, but WITHOUT
ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along with
this program; if not, write to the Free Software Foundation, Inc., 51 Franklin
St, Fifth Floor, Boston, MA 02110-1301 USA

*****************************************************************************/

/*
  This file is based on ha_berkeley.h of MySQL distribution

  This file defines the Innodb Cursor: the interface between MySQL and
  Innodb
*/

#pragma once
#ifndef INNODB_HANDLER_HA_INNODB_H
#define INNODB_HANDLER_HA_INNODB_H

#include <drizzled/cursor.h>
#include <drizzled/thr_lock.h>
#include <drizzled/plugin/transactional_storage_engine.h>

using namespace drizzled;

/* Structure defines translation table between mysql index and innodb
index structures */
typedef struct innodb_idx_translate_struct {
	ulint		index_count;	/*!< number of valid index entries
					in the index_mapping array */
	ulint		array_size;	/*!< array size of index_mapping */
	dict_index_t**	index_mapping;	/*!< index pointer array directly
					maps to index in Innodb from MySQL
					array index */
} innodb_idx_translate_t;

/** InnoDB table share */
typedef struct st_innobase_share {
	THR_LOCK	lock;		/*!< MySQL lock protecting
					this structure */
	char	table_name[FN_REFLEN];	/*!< InnoDB table name */
	uint		use_count;	/*!< reference count,
					incremented in get_share()
					and decremented in free_share() */
	void*		table_name_hash;/*!< hash table chain node */
	innodb_idx_translate_t	idx_trans_tbl;	/*!< index translation
						table between MySQL and
						Innodb */

        st_innobase_share(const char *arg) :
          use_count(0)
        {
          strncpy(table_name, arg, FN_REFLEN);
        }

} INNOBASE_SHARE;


/** InnoDB B-tree index */
struct dict_index_struct;
/** Prebuilt structures in an Innobase table handle used within MySQL */
struct row_prebuilt_struct;

/** InnoDB B-tree index */
typedef struct dict_index_struct dict_index_t;
/** Prebuilt structures in an Innobase table handle used within MySQL */
typedef struct row_prebuilt_struct row_prebuilt_t;

/** The class defining a handle to an Innodb table */
class ha_innobase: public Cursor
{
	row_prebuilt_t*	prebuilt;	/*!< prebuilt struct in InnoDB, used
					to save CPU time with prebuilt data
					structures*/
	Session*	user_session;	/*!< the thread handle of the user
					currently using the handle; this is
					set in external_lock function */
	THR_LOCK_DATA	lock;
	INNOBASE_SHARE*	share;		/*!< information for MySQL
					table locking */

        std::vector<unsigned char> upd_buff; /*!< buffer used in updates */
        std::vector<unsigned char> key_val_buff; /*!< buffer used in converting
                                                     search key values from MySQL format
                                                     to Innodb format */
	ulong		upd_and_key_val_buff_len;
					/* the length of each of the previous
					two buffers */
	uint		primary_key;
	ulong		start_of_scan;	/*!< this is set to 1 when we are
					starting a table scan but have not
					yet fetched any row, else 0 */
	uint		last_match_mode;/* match mode of the latest search:
					ROW_SEL_EXACT, ROW_SEL_EXACT_PREFIX,
					or undefined */
	uint		num_write_row;	/*!< number of doInsertRecord() calls */

	UNIV_INTERN uint store_key_val_for_row(uint keynr, char* buff, 
                                   uint buff_len, const unsigned char* record);
	UNIV_INTERN void update_session(Session* session);
	UNIV_INTERN int change_active_index(uint32_t keynr);
	UNIV_INTERN int general_fetch(unsigned char* buf, uint32_t direction, uint32_t match_mode);
	UNIV_INTERN ulint innobase_lock_autoinc();
	UNIV_INTERN uint64_t innobase_peek_autoinc();
	UNIV_INTERN ulint innobase_set_max_autoinc(uint64_t auto_inc);
	UNIV_INTERN ulint innobase_reset_autoinc(uint64_t auto_inc);
	UNIV_INTERN ulint innobase_get_autoinc(uint64_t* value);
	ulint innobase_update_autoinc(uint64_t	auto_inc);
	UNIV_INTERN void innobase_initialize_autoinc();
	UNIV_INTERN dict_index_t* innobase_get_index(uint keynr);

	/* Init values for the class: */
 public:
	UNIV_INTERN ha_innobase(plugin::StorageEngine &engine,
                                Table &table_arg);
	UNIV_INTERN ~ha_innobase();
  /**
   * Returns the plugin::TransactionStorageEngine pointer
   * of the cursor's underlying engine.
   *
   * @todo
   *
   * Have a TransactionalCursor subclass...
   */
  UNIV_INTERN plugin::TransactionalStorageEngine *getTransactionalEngine()
  {
    return static_cast<plugin::TransactionalStorageEngine *>(getEngine());
  }

	UNIV_INTERN const char* index_type(uint key_number);
	UNIV_INTERN const key_map* keys_to_use_for_scanning();

	UNIV_INTERN int doOpen(const drizzled::identifier::Table &identifier, int mode, uint test_if_locked);
	UNIV_INTERN int close(void);
	UNIV_INTERN double scan_time();
	UNIV_INTERN double read_time(uint index, uint ranges, ha_rows rows);

	UNIV_INTERN int doInsertRecord(unsigned char * buf);
	UNIV_INTERN int doUpdateRecord(const unsigned char * old_data, unsigned char * new_data);
	UNIV_INTERN int doDeleteRecord(const unsigned char * buf);
	UNIV_INTERN bool was_semi_consistent_read();
	UNIV_INTERN void try_semi_consistent_read(bool yes);
	UNIV_INTERN void unlock_row();

	UNIV_INTERN int doStartIndexScan(uint index, bool sorted);
	UNIV_INTERN int doEndIndexScan();
	UNIV_INTERN int index_read(unsigned char * buf, const unsigned char * key,
		uint key_len, enum ha_rkey_function find_flag);
	UNIV_INTERN int index_read_idx(unsigned char * buf, uint index, const unsigned char * key,
			   uint key_len, enum ha_rkey_function find_flag);
	UNIV_INTERN int index_read_last(unsigned char * buf, const unsigned char * key, uint key_len);
	UNIV_INTERN int index_next(unsigned char * buf);
	UNIV_INTERN int index_next_same(unsigned char * buf, const unsigned char *key, uint keylen);
	UNIV_INTERN int index_prev(unsigned char * buf);
	UNIV_INTERN int index_first(unsigned char * buf);
	UNIV_INTERN int index_last(unsigned char * buf);

	UNIV_INTERN int doStartTableScan(bool scan);
	UNIV_INTERN int doEndTableScan();
	UNIV_INTERN int rnd_next(unsigned char *buf);
	UNIV_INTERN int rnd_pos(unsigned char * buf, unsigned char *pos);

	UNIV_INTERN void position(const unsigned char *record);
	UNIV_INTERN int info(uint);
	UNIV_INTERN int analyze(Session* session);
	UNIV_INTERN int discard_or_import_tablespace(bool discard);
	UNIV_INTERN int extra(enum ha_extra_function operation);
        UNIV_INTERN int reset();
	UNIV_INTERN int external_lock(Session *session, int lock_type);
	void position(unsigned char *record);
	UNIV_INTERN ha_rows records_in_range(uint inx, key_range *min_key, key_range
								*max_key);
	UNIV_INTERN ha_rows estimate_rows_upper_bound();

	UNIV_INTERN int delete_all_rows();
	UNIV_INTERN int check(Session* session);
	UNIV_INTERN char* update_table_comment(const char* comment);
	UNIV_INTERN char* get_foreign_key_create_info();
	UNIV_INTERN int get_foreign_key_list(Session *session, List<ForeignKeyInfo> *f_key_list);
	UNIV_INTERN bool can_switch_engines();
	UNIV_INTERN uint referenced_by_foreign_key();
	UNIV_INTERN void free_foreign_key_create_info(char* str);
	UNIV_INTERN THR_LOCK_DATA **store_lock(Session *session, THR_LOCK_DATA **to,
					enum thr_lock_type lock_type);
        UNIV_INTERN virtual void get_auto_increment(uint64_t offset, 
                                                    uint64_t increment,
                                                    uint64_t nb_desired_values,
                                                    uint64_t *first_value,
                                                    uint64_t *nb_reserved_values);
        UNIV_INTERN int reset_auto_increment(uint64_t value);

	UNIV_INTERN bool primary_key_is_clustered();
	UNIV_INTERN int cmp_ref(const unsigned char *ref1, const unsigned char *ref2);
	/** Fast index creation (smart ALTER TABLE) @see handler0alter.cc @{ */
        // Don't use these, I have just left them in here as reference for
        // the future. -Brian 
#if 0
	UNIV_INTERN int add_index(Session *session, TABLE *table_arg, KeyInfo *key_info, uint num_of_keys);
	UNIV_INTERN int prepare_drop_index(Session *session,
                                           TABLE *table_arg,
                                           uint *key_num,
                                           uint num_of_keys);
        UNIV_INTERN int final_drop_index(Session *session, TABLE *table_arg);
#endif
	/** @} */
public:
  int read_range_first(const key_range *start_key, const key_range *end_key,
		       bool eq_range_arg, bool sorted);
  int read_range_next();
};


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

typedef struct trx_struct trx_t;
/********************************************************************//**
@file Cursor/ha_innodb.h
Converts an InnoDB error code to a MySQL error code and also tells to MySQL
about a possible transaction rollback inside InnoDB caused by a lock wait
timeout or a deadlock.
@return	MySQL error code */
UNIV_INTERN
int
convert_error_code_to_mysql(
/*========================*/
	int		error,		/*!< in: InnoDB error code */
	ulint		flags,		/*!< in: InnoDB table flags, or 0 */
	Session		*session);	/*!< in: user thread handle or NULL */

/*********************************************************************//**
Allocates an InnoDB transaction for a MySQL Cursor object.
@return	InnoDB transaction handle */
UNIV_INTERN
trx_t*
innobase_trx_allocate(
/*==================*/
	Session		*session);	/*!< in: user thread handle */

/***********************************************************************
This function checks each index name for a table against reserved
system default primary index name 'GEN_CLUST_INDEX'. If a name matches,
this function pushes an error message to the client, and returns true. */
bool
innobase_index_name_is_reserved(
/*============================*/
					/* out: true if index name matches a
					reserved name */
	const trx_t*	trx,		/* in: InnoDB transaction handle */
	const drizzled::KeyInfo*	key_info,/* in: Indexes to be created */
	ulint		num_of_keys);	/* in: Number of indexes to
					be created. */

#endif /* INNODB_HANDLER_HA_INNODB_H */
