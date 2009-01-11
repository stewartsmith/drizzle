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

#ifndef DRIZZLE_SERVER_LOG_H
#define DRIZZLE_SERVER_LOG_H

#include <drizzled/common.h>
#include <mysys/iocache.h>
#include <drizzled/xid.h>

class Table;
class Relay_log_info;
class Session;
class Format_description_log_event;

/*
  Transaction Coordinator log - a base abstract class
  for two different implementations
*/
class TC_LOG
{
  public:
  int using_heuristic_recover();
  TC_LOG() {}
  virtual ~TC_LOG() {}

  virtual int open(const char *opt_name)=0;
  virtual void close()=0;
  virtual int log_xid(Session *session, my_xid xid)=0;
  virtual void unlog(ulong cookie, my_xid xid)=0;
};

class TC_LOG_DUMMY: public TC_LOG // use it to disable the logging
{
public:
  TC_LOG_DUMMY() {}
  int open(const char *)
  { return 0; }
  void close(void)                          { }
  int log_xid(Session *, my_xid)         { return 1; }
  void unlog(ulong, my_xid)  { }
};

#ifdef HAVE_MMAP
class TC_LOG_MMAP: public TC_LOG
{
  public:                // only to keep Sun Forte on sol9x86 happy
  typedef enum {
    POOL,                 // page is in pool
    ERROR,                // last sync failed
    DIRTY                 // new xids added since last sync
  } PAGE_STATE;

  private:
  typedef struct st_page {
    struct st_page *next; // page a linked in a fifo queue
    my_xid *start, *end;  // usable area of a page
    my_xid *ptr;          // next xid will be written here
    int size, free;       // max and current number of free xid slots on the page
    int waiters;          // number of waiters on condition
    PAGE_STATE state;     // see above
    pthread_mutex_t lock; // to access page data or control structure
    pthread_cond_t  cond; // to wait for a sync
  } PAGE;

  char logname[FN_REFLEN];
  File fd;
  my_off_t file_length;
  uint32_t npages, inited;
  unsigned char *data;
  struct st_page *pages, *syncing, *active, *pool, *pool_last;
  /*
    note that, e.g. LOCK_active is only used to protect
    'active' pointer, to protect the content of the active page
    one has to use active->lock.
    Same for LOCK_pool and LOCK_sync
  */
  pthread_mutex_t LOCK_active, LOCK_pool, LOCK_sync;
  pthread_cond_t COND_pool, COND_active;

  public:
  TC_LOG_MMAP(): inited(0) {}
  int open(const char *opt_name);
  void close();
  int log_xid(Session *session, my_xid xid);
  void unlog(ulong cookie, my_xid xid);
  int recover();

  private:
  void get_active_from_pool();
  int sync();
  int overflow();
};
#else
#define TC_LOG_MMAP TC_LOG_DUMMY
#endif

extern TC_LOG *tc_log;
extern TC_LOG_MMAP tc_log_mmap;
extern TC_LOG_DUMMY tc_log_dummy;

/* log info errors */
#define LOG_INFO_EOF -1
#define LOG_INFO_IO  -2
#define LOG_INFO_INVALID -3
#define LOG_INFO_SEEK -4
#define LOG_INFO_MEM -6
#define LOG_INFO_FATAL -7
#define LOG_INFO_IN_USE -8
#define LOG_INFO_EMFILE -9


/* bitmap to SQL_LOG::close() */
#define LOG_CLOSE_INDEX		1
#define LOG_CLOSE_TO_BE_OPENED	2
#define LOG_CLOSE_STOP_EVENT	4

class Relay_log_info;

typedef struct st_log_info
{
  char log_file_name[FN_REFLEN];
  my_off_t index_file_offset, index_file_start_offset;
  my_off_t pos;
  bool fatal; // if the purge happens to give us a negative offset
  pthread_mutex_t lock;
  st_log_info()
    : index_file_offset(0), index_file_start_offset(0),
      pos(0), fatal(0)
    {
      log_file_name[0] = '\0';
      pthread_mutex_init(&lock, MY_MUTEX_INIT_FAST);
    }
  ~st_log_info() { pthread_mutex_destroy(&lock);}
} LOG_INFO;

/*
  Currently we have only 3 kinds of logging functions: old-fashioned
  logs, stdout and csv logging routines.
*/
#define MAX_LOG_HANDLERS_NUM 3

/* log event handler flags */
#define LOG_NONE       1
#define LOG_FILE       2

class Log_event;
class Rows_log_event;

enum enum_log_type { LOG_UNKNOWN, LOG_NORMAL, LOG_BIN };
enum enum_log_state { LOG_OPENED, LOG_CLOSED, LOG_TO_BE_OPENED };

/*
  TODO use mmap instead of IO_CACHE for binlog
  (mmap+fsync is two times faster than write+fsync)
*/

class DRIZZLE_LOG
{
public:
  DRIZZLE_LOG();
  void init_pthread_objects();
  void cleanup();
  bool open(const char *log_name,
            enum_log_type log_type,
            const char *new_name,
            enum cache_type io_cache_type_arg);
  void init(enum_log_type log_type_arg,
            enum cache_type io_cache_type_arg);
  void close(uint32_t exiting);
  inline bool is_open() { return log_state != LOG_CLOSED; }
  const char *generate_name(const char *log_name, const char *suffix,
                            bool strip_ext, char *buff);
  int generate_new_name(char *new_name, const char *log_name);
 protected:
  /* LOCK_log is inited by init_pthread_objects() */
  pthread_mutex_t LOCK_log;
  char *name;
  char log_file_name[FN_REFLEN];
  char time_buff[20], db[NAME_LEN + 1];
  bool write_error, inited;
  IO_CACHE log_file;
  enum_log_type log_type;
  volatile enum_log_state log_state;
  enum cache_type io_cache_type;
  friend class Log_event;
};

class DRIZZLE_BIN_LOG: public TC_LOG, private DRIZZLE_LOG
{
 private:
  /* LOCK_log and LOCK_index are inited by init_pthread_objects() */
  pthread_mutex_t LOCK_index;
  pthread_mutex_t LOCK_prep_xids;
  pthread_cond_t  COND_prep_xids;
  pthread_cond_t update_cond;
  uint64_t bytes_written;
  IO_CACHE index_file;
  char index_file_name[FN_REFLEN];
  /*
     The max size before rotation (usable only if log_type == LOG_BIN: binary
     logs and relay logs).
     For a binlog, max_size should be max_binlog_size.
     For a relay log, it should be max_relay_log_size if this is non-zero,
     max_binlog_size otherwise.
     max_size is set in init(), and dynamically changed (when one does SET
     GLOBAL MAX_BINLOG_SIZE|MAX_RELAY_LOG_SIZE) by fix_max_binlog_size and
     fix_max_relay_log_size).
  */
  ulong max_size;
  long prepared_xids; /* for tc log - number of xids to remember */
  // current file sequence number for load data infile binary logging
  uint32_t file_id;
  uint32_t open_count;				// For replication
  int readers_count;
  bool need_start_event;
  /*
    no_auto_events means we don't want any of these automatic events :
    Start/Rotate/Stop. That is, in 4.x when we rotate a relay log, we don't
    want a Rotate_log event to be written to the relay log. When we start a
    relay log etc. So in 4.x this is 1 for relay logs, 0 for binlogs.
    In 5.0 it's 0 for relay logs too!
  */
  bool no_auto_events;

  uint64_t m_table_map_version;

  int write_to_file(IO_CACHE *cache);
  /*
    This is used to start writing to a new log file. The difference from
    new_file() is locking. new_file_without_locking() does not acquire
    LOCK_log.
  */
  void new_file_without_locking();
  void new_file_impl(bool need_lock);

public:
  DRIZZLE_LOG::generate_name;
  DRIZZLE_LOG::is_open;
  /*
    These describe the log's format. This is used only for relay logs.
    _for_exec is used by the SQL thread, _for_queue by the I/O thread. It's
    necessary to have 2 distinct objects, because the I/O thread may be reading
    events in a different format from what the SQL thread is reading (consider
    the case of a master which has been upgraded from 5.0 to 5.1 without doing
    RESET MASTER, or from 4.x to 5.0).
  */
  Format_description_log_event *description_event_for_exec,
    *description_event_for_queue;

  DRIZZLE_BIN_LOG();
  /*
    note that there's no destructor ~DRIZZLE_BIN_LOG() !
    The reason is that we don't want it to be automatically called
    on exit() - but only during the correct shutdown process
  */

  int open(const char *opt_name);
  void close();
  int log_xid(Session *session, my_xid xid);
  void unlog(ulong cookie, my_xid xid);
  int recover(IO_CACHE *log, Format_description_log_event *fdle);
  bool is_table_mapped(Table *table) const;

  uint64_t table_map_version() const { return m_table_map_version; }
  void update_table_map_version() { ++m_table_map_version; }

  int flush_and_set_pending_rows_event(Session *session, Rows_log_event* event);

  void reset_bytes_written()
  {
    bytes_written = 0;
  }
  void harvest_bytes_written(uint64_t* counter)
  {
    (*counter)+=bytes_written;
    bytes_written=0;
    return;
  }
  void set_max_size(ulong max_size_arg);
  void signal_update();
  void wait_for_update_relay_log(Session* session);
  int  wait_for_update_bin_log(Session* session, const struct timespec * timeout);
  void set_need_start_event() { need_start_event = 1; }
  void init(bool no_auto_events_arg, ulong max_size);
  void init_pthread_objects();
  void cleanup();
  bool open(const char *log_name,
            enum_log_type log_type,
            const char *new_name,
	    enum cache_type io_cache_type_arg,
	    bool no_auto_events_arg, ulong max_size,
            bool null_created);
  bool open_index_file(const char *index_file_name_arg,
                       const char *log_name);
  /* Use this to start writing a new log file */
  void new_file();

  bool write(Log_event* event_info); // binary log write
  bool write(Session *session, IO_CACHE *cache, Log_event *commit_event);

  int  write_cache(IO_CACHE *cache, bool lock_log, bool flush_and_sync);

  void start_union_events(Session *session, query_id_t query_id_param);
  void stop_union_events(Session *session);
  bool is_query_in_union(Session *session, query_id_t query_id_param);

  /*
    v stands for vector
    invoked as appendv(buf1,len1,buf2,len2,...,bufn,lenn,0)
  */
  bool appendv(const char* buf,uint32_t len,...);
  bool append(Log_event* ev);

  void make_log_name(char* buf, const char* log_ident);
  bool is_active(const char* log_file_name);
  int update_log_index(LOG_INFO* linfo, bool need_update_threads);
  void rotate_and_purge(uint32_t flags);
  bool flush_and_sync();
  int purge_logs(const char *to_log, bool included,
                 bool need_mutex, bool need_update_threads,
                 uint64_t *decrease_log_space);
  int purge_logs_before_date(time_t purge_time);
  int purge_first_log(Relay_log_info* rli, bool included);
  bool reset_logs(Session* session);
  void close(uint32_t exiting);

  // iterating through the log index file
  int find_log_pos(LOG_INFO* linfo, const char* log_name,
		   bool need_mutex);
  int find_next_log(LOG_INFO* linfo, bool need_mutex);
  int get_current_log(LOG_INFO* linfo);
  int raw_get_current_log(LOG_INFO* linfo);
  uint32_t next_file_id();
  inline char* get_index_fname() { return index_file_name;}
  inline char* get_log_fname() { return log_file_name; }
  inline char* get_name() { return name; }
  inline pthread_mutex_t* get_log_lock() { return &LOCK_log; }
  inline IO_CACHE* get_log_file() { return &log_file; }

  inline void lock_index() { pthread_mutex_lock(&LOCK_index);}
  inline void unlock_index() { pthread_mutex_unlock(&LOCK_index);}
  inline IO_CACHE *get_index_file() { return &index_file;}
  inline uint32_t get_open_count() { return open_count; }
};


File open_binlog(IO_CACHE *log, const char *log_file_name,
                 const char **errmsg);

class Log_event_handler
{
public:
  Log_event_handler() {}
  virtual bool init()= 0;
  virtual void cleanup()= 0;

  virtual bool log_error(enum loglevel level, const char *format,
                         va_list args)= 0;
  virtual ~Log_event_handler() {}
};

extern TYPELIB binlog_format_typelib;

#endif /* DRIZZLE_SERVER_LOG_H */
