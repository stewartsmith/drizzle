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

/**
  @file

  @details
  Mostly this file is used in the server. But a little part of it is used in
  definition of SELECT_DISTINCT and others.

  @TODO Name this file better. "priv" could mean private, privileged, privileges.
*/

#ifndef DRIZZLED_SERVER_INCLUDES_H
#define DRIZZLED_SERVER_INCLUDES_H

/* Cross-platform portability code and standard includes */
#include <drizzled/global.h>
/* Contains system-wide constants and #defines */
#include <drizzled/definitions.h>

/* Lots of system-wide struct definitions like IO_CACHE,
   prototypes for all my_* functions */
#include <mysys/my_sys.h>
/* Custom C string functions */
#include <mystrings/m_string.h>

/* The <strong>INTERNAL</strong> plugin API - not the external, or public, server plugin API */
#include <drizzled/sql_plugin.h>
/* Range optimization API/library */
#include <drizzled/opt_range.h>
/* Simple error injection (crash) module */
#include <drizzled/error_injection.h>
/* Routines for dropping, repairing, checking schema tables */
#include <drizzled/sql_table.h>

/* Routines for printing error messages */
#include <drizzled/errmsg_print.h>

#include <string>
#include <sstream>
#include <bitset>


extern const CHARSET_INFO *system_charset_info, *files_charset_info ;
extern const CHARSET_INFO *national_charset_info, *table_alias_charset;

typedef struct drizzled_lock_st DRIZZLE_LOCK;
typedef struct st_ha_create_information HA_CREATE_INFO;

/* information schema */
static const std::string INFORMATION_SCHEMA_NAME("information_schema");



/* mysqld.cc */
void refresh_status(Session *session);
bool drizzle_rm_tmp_tables(void);
void handle_connection_in_main_thread(Session *session);
void create_thread_to_handle_connection(Session *session);
void unlink_session(Session *session);
bool one_thread_per_connection_end(Session *session, bool put_in_cache);
void flush_thread_cache();

/* item_func.cc */
extern bool check_reserved_words(LEX_STRING *name);
extern enum_field_types agg_field_type(Item **items, uint32_t nitems);

/* strfunc.cc */
uint64_t find_set(TYPELIB *lib, const char *x, uint32_t length, const CHARSET_INFO * const cs,
		   char **err_pos, uint32_t *err_len, bool *set_warning);
uint32_t find_type(const TYPELIB *lib, const char *find, uint32_t length,
               bool part_match);
uint32_t find_type2(const TYPELIB *lib, const char *find, uint32_t length,
                const CHARSET_INFO *cs);
void unhex_type2(TYPELIB *lib);
uint32_t check_word(TYPELIB *lib, const char *val, const char *end,
		const char **end_of_word);
int find_string_in_array(LEX_STRING * const haystack, LEX_STRING * const needle,
                         const CHARSET_INFO * const cs);


/*
  External variables
*/

extern char *drizzle_tmpdir;
extern const LEX_STRING command_name[];
extern const char *first_keyword, *my_localhost, *delayed_user, *binary_keyword;
extern const char *myisam_recover_options_str;
extern const char *in_left_expr_name, *in_additional_cond, *in_having_cond;
extern const char * const TRG_EXT;
extern const char * const TRN_EXT;
extern char language[FN_REFLEN];
extern char glob_hostname[FN_REFLEN], drizzle_home[FN_REFLEN];
extern char pidfile_name[FN_REFLEN], system_time_zone[30], *opt_init_file;
extern char *opt_tc_log_file;
extern const double log_10[309];
extern uint64_t log_10_int[20];
extern uint64_t keybuff_size;
extern uint64_t session_startup_options;
extern ulong thread_id;
extern uint64_t aborted_threads;
extern uint64_t aborted_connects;
extern uint64_t slow_launch_threads;
extern uint64_t slow_launch_time;
extern uint64_t table_cache_size;
extern uint64_t table_def_size;
extern uint64_t max_connect_errors;
extern uint64_t connect_timeout;
extern uint32_t back_log;
extern pid_t current_pid;
extern uint64_t expire_logs_days;
extern uint64_t tc_log_max_pages_used;
extern uint64_t tc_log_page_size;
extern uint64_t opt_tc_log_size;
extern uint64_t tc_log_page_waits;
extern bool opt_innodb;
extern uint32_t test_flags,select_errors,ha_open_options;
extern uint32_t protocol_version, drizzled_port, dropping_tables;
extern uint32_t delay_key_write_options;
extern bool opt_endinfo, using_udf_functions;
extern bool locked_in_memory;
extern bool opt_using_transactions;
extern bool using_update_log, server_id_supplied;
extern bool opt_update_log, opt_bin_log;
extern bool opt_log;
extern ulong log_output_options;
extern bool opt_character_set_client_handshake;
extern bool volatile abort_loop, shutdown_in_progress;
extern uint32_t volatile thread_count, thread_running, global_read_lock;
extern uint32_t connection_count;
extern bool opt_sql_bin_update;
extern bool opt_safe_user_create;
extern bool opt_no_mix_types;
extern bool opt_safe_show_db, opt_myisam_use_mmap;
extern bool opt_local_infile;
extern bool use_temp_pool;
extern bool opt_readonly;
extern char* opt_secure_file_priv;
extern bool opt_noacl;
extern bool opt_old_style_user_limits;
extern char *default_tz_name;
extern char *opt_logname, *opt_slow_logname;
extern const char *log_output_str;

extern TableList general_log, slow_log;
extern FILE *stderror_file;
extern pthread_mutex_t LOCK_drizzleclient_create_db,LOCK_open, LOCK_lock_db,
       LOCK_thread_count,LOCK_user_locks, LOCK_status,
       LOCK_timezone,
       LOCK_global_read_lock,
       LOCK_global_system_variables, LOCK_user_conn,
       LOCK_bytes_sent, LOCK_bytes_received, LOCK_connection_count;
extern pthread_mutex_t LOCK_server_started;
extern pthread_rwlock_t LOCK_sys_init_connect;
extern pthread_rwlock_t LOCK_system_variables_hash;
extern pthread_cond_t COND_refresh, COND_thread_count, COND_manager;
extern pthread_cond_t COND_global_read_lock;
extern pthread_attr_t connection_attrib;
extern I_List<Session> threads;
extern MY_BITMAP temp_pool;
extern String my_empty_string;
extern const String my_null_string;
extern SHOW_VAR status_vars[];
extern struct system_variables max_system_variables;
extern struct system_status_var global_status_var;
extern struct rand_struct sql_rand;

extern const char *opt_date_time_formats[];
extern KNOWN_DATE_TIME_FORMAT known_date_time_formats[];

extern Table *unused_tables;
extern const char* any_db;
extern struct my_option my_long_options[];
extern const LEX_STRING view_type;
extern TYPELIB thread_handling_typelib;
extern uint8_t uc_update_queries[SQLCOM_END+1];
extern std::bitset<5> sql_command_flags[];
extern TYPELIB log_output_typelib;

/* optional things, have_* variables */
extern SHOW_COMP_OPTION have_community_features;

extern handlerton *myisam_hton;
extern handlerton *heap_hton;

extern SHOW_COMP_OPTION have_symlink;

extern pthread_t signal_thread;


/* table.cc */
TABLE_SHARE *alloc_table_share(TableList *table_list, char *key,
                               uint32_t key_length);
void init_tmp_table_share(Session *session, TABLE_SHARE *share, const char *key,
                          uint32_t key_length,
                          const char *table_name, const char *path);
void free_table_share(TABLE_SHARE *share);
int open_table_def(Session *session, TABLE_SHARE *share, uint32_t db_flags);
void open_table_error(TABLE_SHARE *share, int error, int db_errno, int errarg);
int open_table_from_share(Session *session, TABLE_SHARE *share, const char *alias,
                          uint32_t db_stat, uint32_t prgflag, uint32_t ha_open_flags,
                          Table *outparam, open_table_mode open_mode);
int readfrm(const char *name, unsigned char **data, size_t *length);
int writefrm(const char* name, const unsigned char* data, size_t len);
int read_string(File file, unsigned char* *to, size_t length);
void free_blobs(Table *table);
int set_zone(int nr,int min_zone,int max_zone);
uint32_t convert_period_to_month(uint32_t period);
uint32_t convert_month_to_period(uint32_t month);
void get_date_from_daynr(long daynr,uint32_t *year, uint32_t *month,
			 uint32_t *day);
time_t TIME_to_timestamp(Session *session, const DRIZZLE_TIME *t, bool *not_exist);
bool str_to_time_with_warn(const char *str,uint32_t length,DRIZZLE_TIME *l_time);
enum enum_drizzle_timestamp_type str_to_datetime_with_warn(const char *str, uint32_t length,
                                         DRIZZLE_TIME *l_time, uint32_t flags);
void localtime_to_TIME(DRIZZLE_TIME *to, struct tm *from);
void calc_time_from_sec(DRIZZLE_TIME *to, long seconds, long microseconds);

void make_truncated_value_warning(Session *session, DRIZZLE_ERROR::enum_warning_level level,
                                  const char *str_val,
				  uint32_t str_length, enum enum_drizzle_timestamp_type time_type,
                                  const char *field_name);

bool date_add_interval(DRIZZLE_TIME *ltime, interval_type int_type, INTERVAL interval);
bool calc_time_diff(DRIZZLE_TIME *l_time1, DRIZZLE_TIME *l_time2, int l_sign,
                    int64_t *seconds_out, long *microseconds_out);

extern LEX_STRING interval_type_to_name[];

extern DATE_TIME_FORMAT *date_time_format_make(enum enum_drizzle_timestamp_type format_type,
					       const char *format_str,
					       uint32_t format_length);
extern DATE_TIME_FORMAT *date_time_format_copy(Session *session,
					       DATE_TIME_FORMAT *format);
const char *get_date_time_format_str(KNOWN_DATE_TIME_FORMAT *format,
				                             enum enum_drizzle_timestamp_type type);
void make_datetime(const DATE_TIME_FORMAT *format, const DRIZZLE_TIME *l_time,
                   String *str);
void make_date(const DATE_TIME_FORMAT *format, const DRIZZLE_TIME *l_time,
               String *str);
void make_time(const DATE_TIME_FORMAT *format, const DRIZZLE_TIME *l_time,
               String *str);
int my_time_compare(DRIZZLE_TIME *a, DRIZZLE_TIME *b);
uint64_t get_datetime_value(Session *session, Item ***item_arg, Item **cache_arg,
                             Item *warn_item, bool *is_null);

int test_if_number(char *str,int *res,bool allow_wildcards);
void change_byte(unsigned char *,uint,char,char);
void init_read_record(READ_RECORD *info, Session *session, Table *reg_form,
		      SQL_SELECT *select,
		      int use_record_cache, bool print_errors);
void init_read_record_idx(READ_RECORD *info, Session *session, Table *table,
                          bool print_error, uint32_t idx);
void end_read_record(READ_RECORD *info);
ha_rows filesort(Session *session, Table *form,struct st_sort_field *sortorder,
		 uint32_t s_length, SQL_SELECT *select,
		 ha_rows max_rows, bool sort_positions,
                 ha_rows *examined_rows);
void filesort_free_buffers(Table *table, bool full);
void change_double_for_sort(double nr,unsigned char *to);
double my_double_round(double value, int64_t dec, bool dec_unsigned,
                       bool truncate);
int get_quick_record(SQL_SELECT *select);

int calc_weekday(long daynr,bool sunday_first_day_of_week);
uint32_t calc_week(DRIZZLE_TIME *l_time, uint32_t week_behaviour, uint32_t *year);
void find_date(char *pos,uint32_t *vek,uint32_t flag);
TYPELIB *convert_strings_to_array_type(char * *typelibs, char * *end);
TYPELIB *typelib(MEM_ROOT *mem_root, List<String> &strings);
ulong get_form_pos(File file, unsigned char *head, TYPELIB *save_names);
ulong make_new_entry(File file,unsigned char *fileinfo,TYPELIB *formnames,
		     const char *newname);
ulong next_io_size(ulong pos);
void append_unescaped(String *res, const char *pos, uint32_t length);
int rename_table_proto_file(const char *from, const char* to);
int delete_table_proto_file(const char *file_name);
int rename_file_ext(const char * from,const char * to,const char * ext);
bool check_db_name(LEX_STRING *db);
bool check_column_name(const char *name);
bool check_table_name(const char *name, uint32_t length);
char *get_field(MEM_ROOT *mem, Field *field);
bool get_field(MEM_ROOT *mem, Field *field, class String *res);
char *fn_rext(char *name);

/* Conversion functions */
uint32_t build_table_filename(char *buff, size_t bufflen, const char *db,
                          const char *table, const char *ext, uint32_t flags);

/* Flags for conversion functions. */
#define FN_FROM_IS_TMP  (1 << 0)
#define FN_TO_IS_TMP    (1 << 1)
#define FN_IS_TMP       (FN_FROM_IS_TMP | FN_TO_IS_TMP)
#define NO_FRM_RENAME   (1 << 2)


inline ulong sql_rnd()
{
  ulong tmp= (ulong) (rand() * 0xffffffff); /* make all bits random */

  return tmp;
}



/**
  convert a hex digit into number.
*/

inline int hexchar_to_int(char c)
{
  if (c <= '9' && c >= '0')
    return c-'0';
  c|=32;
  if (c <= 'f' && c >= 'a')
    return c-'a'+10;
  return -1;
}


#endif /* DRIZZLE_SERVER_SERVER_INCLUDES_H */
