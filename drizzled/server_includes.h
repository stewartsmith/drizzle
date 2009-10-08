/*
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
 * @file
 *
 * Various server-wide declarations and variables.
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

/* Range optimization API/library */
#include <drizzled/opt_range.h>
/* Routines for dropping, repairing, checking schema tables */
#include <drizzled/sql_table.h>

/* Routines for printing error messages */
#include <drizzled/errmsg_print.h>

#include <string>
#include <sstream>
#include <bitset>

typedef struct st_ha_create_information HA_CREATE_INFO;

/* information schema */
static const std::string INFORMATION_SCHEMA_NAME("information_schema");

/* drizzled.cc */
void refresh_status(Session *session);
void unlink_session(Session *session);

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

/*
  External variables
*/

extern const CHARSET_INFO *system_charset_info;
extern const CHARSET_INFO *files_charset_info;
extern const CHARSET_INFO *table_alias_charset;

extern char *drizzle_tmpdir;
extern const LEX_STRING command_name[];
extern const char *first_keyword;
extern const char *binary_keyword;
extern const char *in_left_expr_name;
extern const char *in_additional_cond;
extern const char *in_having_cond;
extern char language[FN_REFLEN];
extern char glob_hostname[FN_REFLEN];
extern char drizzle_home[FN_REFLEN];
extern char pidfile_name[FN_REFLEN];
extern char system_time_zone[30];
extern char *opt_tc_log_file;
extern const double log_10[309];
extern uint64_t log_10_int[20];
extern uint64_t session_startup_options;
extern uint32_t global_thread_id;
extern uint64_t aborted_threads;
extern uint64_t aborted_connects;
extern uint64_t table_cache_size;
extern uint64_t table_def_size;
extern uint64_t max_connect_errors;
extern uint32_t back_log;
extern pid_t current_pid;
extern std::bitset<12> test_flags;
extern uint32_t ha_open_options;
extern uint32_t drizzled_tcp_port;
extern char *drizzled_bind_host;
extern uint32_t drizzled_bind_timeout;
extern uint32_t dropping_tables;
extern bool opt_endinfo;
extern bool locked_in_memory;
extern bool volatile abort_loop;
extern bool volatile shutdown_in_progress;
extern uint32_t volatile thread_running;
extern uint32_t volatile global_read_lock;
extern bool opt_readonly;
extern char* opt_secure_file_priv;
extern char *default_tz_name;

extern FILE *stderror_file;
extern pthread_mutex_t LOCK_create_db;
extern pthread_mutex_t LOCK_open;
extern pthread_mutex_t LOCK_thread_count;
extern pthread_mutex_t LOCK_status;
extern pthread_mutex_t LOCK_global_read_lock;
extern pthread_mutex_t LOCK_global_system_variables;

extern pthread_rwlock_t LOCK_system_variables_hash;
extern pthread_cond_t COND_refresh;
extern pthread_cond_t COND_thread_count;
extern pthread_cond_t COND_global_read_lock;
extern pthread_attr_t connection_attrib;
extern std::vector<Session *> session_list;
extern String my_empty_string;
extern const String my_null_string;
extern SHOW_VAR status_vars[];
extern struct system_variables max_system_variables;
extern struct system_status_var global_status_var;

extern Table *unused_tables;
extern const char* any_db;
extern struct my_option my_long_options[];
extern std::bitset<5> sql_command_flags[];

extern drizzled::plugin::StorageEngine *myisam_engine;
extern drizzled::plugin::StorageEngine *heap_engine;

extern SHOW_COMP_OPTION have_symlink;

extern pthread_t signal_thread;

/* table.cc */
TableShare *alloc_table_share(TableList *table_list, char *key,
                               uint32_t key_length);
int open_table_def(Session *session, TableShare *share);
void open_table_error(TableShare *share, int error, int db_errno, int errarg);
int open_table_from_share(Session *session, TableShare *share, const char *alias,
                          uint32_t db_stat, uint32_t prgflag, uint32_t ha_open_flags,
                          Table *outparam, open_table_mode open_mode);
void free_blobs(Table *table);
int set_zone(int nr,int min_zone,int max_zone);
uint32_t convert_period_to_month(uint32_t period);
uint32_t convert_month_to_period(uint32_t month);
void get_date_from_daynr(long daynr,uint32_t *year, uint32_t *month,
			 uint32_t *day);
bool str_to_time_with_warn(const char *str,uint32_t length,DRIZZLE_TIME *l_time);
enum enum_drizzle_timestamp_type str_to_datetime_with_warn(const char *str, uint32_t length,
                                         DRIZZLE_TIME *l_time, uint32_t flags);
void localtime_to_TIME(DRIZZLE_TIME *to, struct tm *from);
void calc_time_from_sec(DRIZZLE_TIME *to, long seconds, long microseconds);

void make_truncated_value_warning(Session *session, DRIZZLE_ERROR::enum_warning_level level,
                                  const char *str_val,
				  uint32_t str_length, enum enum_drizzle_timestamp_type time_type,
                                  const char *field_name);

bool calc_time_diff(DRIZZLE_TIME *l_time1, DRIZZLE_TIME *l_time2, int l_sign,
                    int64_t *seconds_out, long *microseconds_out);

extern LEX_STRING interval_type_to_name[];

void make_datetime(const DRIZZLE_TIME *l_time, String *str);
void make_date(const DRIZZLE_TIME *l_time, String *str);
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
ulong next_io_size(ulong pos);
void append_unescaped(String *res, const char *pos, uint32_t length);
int rename_table_proto_file(const char *from, const char* to);
int delete_table_proto_file(const char *file_name);
int rename_file_ext(const char * from,const char * to,const char * ext);
bool check_db_name(LEX_STRING *db);
bool check_column_name(const char *name);
bool check_table_name(const char *name, uint32_t length);
char *fn_rext(char *name);

/* Conversion functions */
size_t build_table_filename(char *buff, size_t bufflen, const char *db, 
                            const char *table_name, bool is_tmp);

/* Flags for conversion functions. */
#define FN_FROM_IS_TMP  (1 << 0)
#define FN_TO_IS_TMP    (1 << 1)
#define FN_IS_TMP       (FN_FROM_IS_TMP | FN_TO_IS_TMP)
#define NO_FRM_RENAME   (1 << 2)

inline uint32_t sql_rnd()
{
  return (uint32_t) (rand() * 0xffffffff); /* make all bits random */
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

#endif /* DRIZZLED_SERVER_INCLUDES_H */
