/* Copyright (C) 2000-2003 MySQL AB

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


/**
  @file

  @brief
  logging of commands

  @todo
    Abort logging when we get an error in reading or writing log files
*/

#include <drizzled/server_includes.h>
#include <libdrizzle/libdrizzle.h>
#include <drizzled/replicator.h>
#include <mysys/hash.h>

#include <mysys/my_dir.h>
#include <stdarg.h>

#include <drizzled/plugin.h>
#include <drizzled/error.h>
#include <drizzled/errmsg_print.h>
#include <drizzled/gettext.h>
#include <drizzled/data_home.h>
#include <drizzled/log_event.h>

/* max size of the log message */
#define MY_OFF_T_UNDEF (~(my_off_t)0UL)

DRIZZLE_BIN_LOG drizzle_bin_log;
uint64_t sync_binlog_counter= 0; /* We should rationalize the largest possible counters for binlog sync */

static bool test_if_number(const char *str,
                           long *res, bool allow_wildcards);
static int binlog_init(void *p);
static int binlog_close_connection(handlerton *hton, Session *session);
static int binlog_savepoint_set(handlerton *hton, Session *session, void *sv);
static int binlog_savepoint_rollback(handlerton *hton, Session *session, void *sv);
static int binlog_commit(handlerton *hton, Session *session, bool all);
static int binlog_rollback(handlerton *hton, Session *session, bool all);
static int binlog_prepare(handlerton *hton, Session *session, bool all);


handlerton *binlog_hton;


 /*
  Save position of binary log transaction cache.

  SYNPOSIS
    binlog_trans_log_savepos()

    session      The thread to take the binlog data from
    pos      Pointer to variable where the position will be stored

  DESCRIPTION

    Save the current position in the binary log transaction cache into
    the variable pointed to by 'pos'
 */

static void
binlog_trans_log_savepos(Session *, my_off_t *pos)
{
  assert(pos != NULL);

  return;
}


/*
  this function is mostly a placeholder.
  conceptually, binlog initialization (now mostly done in DRIZZLE_BIN_LOG::open)
  should be moved here.
*/

int binlog_init(void *p)
{
  binlog_hton= (handlerton *)p;
  binlog_hton->state=opt_bin_log ? SHOW_OPTION_YES : SHOW_OPTION_NO;
  binlog_hton->savepoint_offset= sizeof(my_off_t);
  binlog_hton->close_connection= binlog_close_connection;
  binlog_hton->savepoint_set= binlog_savepoint_set;
  binlog_hton->savepoint_rollback= binlog_savepoint_rollback;
  binlog_hton->commit= binlog_commit;
  binlog_hton->rollback= binlog_rollback;
  binlog_hton->prepare= binlog_prepare;
  binlog_hton->flags= HTON_NOT_USER_SELECTABLE | HTON_HIDDEN;

  return 0;
}

static int binlog_close_connection(handlerton *, Session *)
{

  return 0;
}

static int binlog_prepare(handlerton *, Session *session, bool)
{
  /*
    do nothing.
    just pretend we can do 2pc, so that MySQL won't
    switch to 1pc.
    real work will be done in DRIZZLE_BIN_LOG::log_xid()
  */

  (void)replicator_prepare(session);

  return 0;
}

/**
  This function is called once after each statement.

  It has the responsibility to flush the transaction cache to the
  binlog file on commits.

  @param hton  The binlog handlerton.
  @param session   The client thread that executes the transaction.
  @param all   This is @c true if this is a real transaction commit, and
               @false otherwise.

  @see handlerton::commit
*/
static int binlog_commit(handlerton *, Session *session, bool all)
{
  /*
    Decision table for committing a transaction. The top part, the
    *conditions* represent different cases that can occur, and hte
    bottom part, the *actions*, represent what should be done in that
    particular case.

    Real transaction        'all' was true

    Statement in cache      There were at least one statement in the
                            transaction cache

    In transaction          We are inside a transaction

    Stmt modified non-trans The statement being committed modified a
                            non-transactional table

    All modified non-trans  Some statement before this one in the
                            transaction modified a non-transactional
                            table


    =============================  = = = = = = = = = = = = = = = =
    Real transaction               N N N N N N N N N N N N N N N N
    Statement in cache             N N N N N N N N Y Y Y Y Y Y Y Y
    In transaction                 N N N N Y Y Y Y N N N N Y Y Y Y
    Stmt modified non-trans        N N Y Y N N Y Y N N Y Y N N Y Y
    All modified non-trans         N Y N Y N Y N Y N Y N Y N Y N Y

    Action: (C)ommit/(A)ccumulate  C C - C A C - C - - - - A A - A
    =============================  = = = = = = = = = = = = = = = =


    =============================  = = = = = = = = = = = = = = = =
    Real transaction               Y Y Y Y Y Y Y Y Y Y Y Y Y Y Y Y
    Statement in cache             N N N N N N N N Y Y Y Y Y Y Y Y
    In transaction                 N N N N Y Y Y Y N N N N Y Y Y Y
    Stmt modified non-trans        N N Y Y N N Y Y N N Y Y N N Y Y
    All modified non-trans         N Y N Y N Y N Y N Y N Y N Y N Y

    (C)ommit/(A)ccumulate/(-)      - - - - C C - C - - - - C C - C
    =============================  = = = = = = = = = = = = = = = =

    In other words, we commit the transaction if and only if both of
    the following are true:
     - We are not in a transaction and committing a statement

     - We are in a transaction and one (or more) of the following are
       true:

       - A full transaction is committed

         OR

       - A non-transactional statement is committed and there is
         no statement cached

    Otherwise, we accumulate the statement
  */

  if (all || (!session_test_options(session, OPTION_NOT_AUTOCOMMIT | OPTION_BEGIN))) 
  {
    return replicator_end_transaction(session, all, true);
  }

  return(0);
}

/**
  This function is called when a transaction involving a transactional
  table is rolled back.

  It has the responsibility to flush the transaction cache to the
  binlog file. However, if the transaction does not involve
  non-transactional tables, nothing needs to be logged.

  @param hton  The binlog handlerton.
  @param session   The client thread that executes the transaction.
  @param all   This is @c true if this is a real transaction rollback, and
               @false otherwise.

  @see handlerton::rollback
*/
static int binlog_rollback(handlerton *, Session *session, bool all)
{
  int error=0;

  /* TODO: Fix return type */
  (void)replicator_end_transaction(session, all, false);

  return(error);
}

/**
  @note
  How do we handle this (unlikely but legal) case:
  @verbatim
    [transaction] + [update to non-trans table] + [rollback to savepoint] ?
  @endverbatim
  The problem occurs when a savepoint is before the update to the
  non-transactional table. Then when there's a rollback to the savepoint, if we
  simply truncate the binlog cache, we lose the part of the binlog cache where
  the update is. If we want to not lose it, we need to write the SAVEPOINT
  command and the ROLLBACK TO SAVEPOINT command to the binlog cache. The latter
  is easy: it's just write at the end of the binlog cache, but the former
  should be *inserted* to the place where the user called SAVEPOINT. The
  solution is that when the user calls SAVEPOINT, we write it to the binlog
  cache (so no need to later insert it). As transactions are never intermixed
  in the binary log (i.e. they are serialized), we won't have conflicts with
  savepoint names when using mysqlbinlog or in the slave SQL thread.
  Then when ROLLBACK TO SAVEPOINT is called, if we updated some
  non-transactional table, we don't truncate the binlog cache but instead write
  ROLLBACK TO SAVEPOINT to it; otherwise we truncate the binlog cache (which
  will chop the SAVEPOINT command from the binlog cache, which is good as in
  that case there is no need to have it in the binlog).
*/

static int binlog_savepoint_set(handlerton *, Session *session, void *sv)
{
  bool error;
  binlog_trans_log_savepos(session, (my_off_t*) sv);
  /* Write it to the binary log */

  error= replicator_statement(session, session->query, session->query_length);

  return(error);
}

static int binlog_savepoint_rollback(handlerton *, Session *session, void *)
{
  bool error;

  error= replicator_statement(session, session->query, session->query_length);

  return error;
}


int check_binlog_magic(IO_CACHE* log, const char** errmsg)
{
  char magic[4];
  assert(my_b_tell(log) == 0);

  if (my_b_read(log, (unsigned char*) magic, sizeof(magic)))
  {
    *errmsg = _("I/O error reading the header from the binary log");
    errmsg_printf(ERRMSG_LVL_ERROR, "%s, errno=%d, io cache code=%d", *errmsg, my_errno,
		    log->error);
    return 1;
  }
  if (memcmp(magic, BINLOG_MAGIC, sizeof(magic)))
  {
    *errmsg = _("Binlog has bad magic number;  It's not a binary log file "
		"that can be used by this version of Drizzle");
    return 1;
  }
  return 0;
}


File open_binlog(IO_CACHE *log, const char *log_file_name, const char **errmsg)
{
  File file;

  if ((file = my_open(log_file_name, O_RDONLY,
                      MYF(MY_WME))) < 0)
  {
    errmsg_printf(ERRMSG_LVL_ERROR, _("Failed to open log (file '%s', errno %d)"),
                    log_file_name, my_errno);
    *errmsg = _("Could not open log file");
    goto err;
  }
  if (init_io_cache(log, file, IO_SIZE*2, READ_CACHE, 0, 0,
                    MYF(MY_WME|MY_DONT_CHECK_FILESIZE)))
  {
    errmsg_printf(ERRMSG_LVL_ERROR, _("Failed to create a cache on log (file '%s')"),
                    log_file_name);
    *errmsg = _("Could not open log file");
    goto err;
  }
  if (check_binlog_magic(log,errmsg))
    goto err;
  return(file);

err:
  if (file >= 0)
  {
    my_close(file,MYF(0));
    end_io_cache(log);
  }
  return(-1);
}


/**
  Find a unique filename for 'filename.#'.

  Set '#' to a number as low as possible.

  @return
    nonzero if not possible to get unique filename
*/

static int find_uniq_filename(char *name)
{
  long                  number;
  uint32_t                  i;
  char                  buff[FN_REFLEN];
  struct st_my_dir     *dir_info;
  register struct fileinfo *file_info;
  ulong                 max_found=0;
  size_t		buf_length, length;
  char			*start, *end;

  length= dirname_part(buff, name, &buf_length);
  start=  name + length;
  end= strchr(start, '\0');

  *end='.';
  length= (size_t) (end-start+1);

  if (!(dir_info = my_dir(buff,MYF(MY_DONT_SORT))))
  {						// This shouldn't happen
    strcpy(end,".1");				// use name+1
    return(0);
  }
  file_info= dir_info->dir_entry;
  for (i=dir_info->number_off_files ; i-- ; file_info++)
  {
    if (memcmp(file_info->name, start, length) == 0 &&
	test_if_number(file_info->name+length, &number,0))
    {
      set_if_bigger(max_found,(ulong) number);
    }
  }
  my_dirend(dir_info);

  *end++='.';
  sprintf(end,"%06ld",max_found+1);
  return(0);
}


void DRIZZLE_LOG::init(enum_log_type log_type_arg,
                     enum cache_type io_cache_type_arg)
{
  log_type= log_type_arg;
  io_cache_type= io_cache_type_arg;
  return;
}


/*
  Open a (new) log file.

  SYNOPSIS
    open()

    log_name            The name of the log to open
    log_type_arg        The type of the log. E.g. LOG_NORMAL
    new_name            The new name for the logfile. This is only needed
                        when the method is used to open the binlog file.
    io_cache_type_arg   The type of the IO_CACHE to use for this log file

  DESCRIPTION
    Open the logfile, init IO_CACHE and write startup messages
    (in case of general and slow query logs).

  RETURN VALUES
    0   ok
    1   error
*/

bool DRIZZLE_LOG::open(const char *log_name, enum_log_type log_type_arg,
                     const char *new_name, enum cache_type io_cache_type_arg)
{
  char buff[FN_REFLEN];
  File file= -1;
  int open_flags= O_CREAT;

  write_error= 0;

  init(log_type_arg, io_cache_type_arg);

  if (!(name= strdup(log_name)))
  {
    name= (char *)log_name; // for the error message
    goto err;
  }

  if (new_name)
    strcpy(log_file_name, new_name);
  else if (generate_new_name(log_file_name, name))
    goto err;

  if (io_cache_type == SEQ_READ_APPEND)
    open_flags |= O_RDWR | O_APPEND;
  else
    open_flags |= O_WRONLY | (log_type == LOG_BIN ? 0 : O_APPEND);

  db[0]= 0;

  if ((file= my_open(log_file_name, open_flags,
                     MYF(MY_WME | ME_WAITTANG))) < 0 ||
      init_io_cache(&log_file, file, IO_SIZE, io_cache_type,
                    lseek(file, 0, SEEK_CUR), 0,
                    MYF(MY_WME | MY_NABP |
                        ((log_type == LOG_BIN) ? MY_WAIT_IF_FULL : 0))))
    goto err;

  if (log_type == LOG_NORMAL)
  {
    int len= snprintf(buff, sizeof(buff), "%s, Version: %s (%s). "
                      "started with:\nTCP Port: %d, Named Pipe: %s\n",
                      my_progname, server_version, COMPILATION_COMMENT,
                      drizzled_port, "");
    len+= sprintf(buff+len, "Time                 Id Command    Argument\n");

    if (my_b_write(&log_file, (unsigned char*) buff, len) ||
        flush_io_cache(&log_file))
      goto err;
  }

  log_state= LOG_OPENED;
  return(0);

err:
  errmsg_printf(ERRMSG_LVL_ERROR, _("Could not use %s for logging (error %d). "
                    "Turning logging off for the whole duration of the "
                    "Drizzle server process. "
                    "To turn it on again: fix the cause, "
                    "shutdown the Drizzle server and restart it."),
                    name, errno);
  if (file >= 0)
    my_close(file, MYF(0));
  end_io_cache(&log_file);
  if (name)
  {
    free(name);
    name= NULL;
  }
  log_state= LOG_CLOSED;
  return(1);
}

DRIZZLE_LOG::DRIZZLE_LOG()
  : name(0), write_error(false), inited(false), log_type(LOG_UNKNOWN),
    log_state(LOG_CLOSED)
{
  /*
    We don't want to initialize LOCK_Log here as such initialization depends on
    safe_mutex (when using safe_mutex) which depends on MY_INIT(), which is
    called only in main(). Doing initialization here would make it happen
    before main().
  */
  memset(&log_file, 0, sizeof(log_file));
}

void DRIZZLE_LOG::init_pthread_objects()
{
  assert(inited == 0);
  inited= 1;
  (void) pthread_mutex_init(&LOCK_log, MY_MUTEX_INIT_SLOW);
}

/*
  Close the log file

  SYNOPSIS
    close()
    exiting     Bitmask. For the slow and general logs the only used bit is
                LOG_CLOSE_TO_BE_OPENED. This is used if we intend to call
                open at once after close.

  NOTES
    One can do an open on the object at once after doing a close.
    The internal structures are not freed until cleanup() is called
*/

void DRIZZLE_LOG::close(uint32_t exiting)
{					// One can't set log_type here!
  if (log_state == LOG_OPENED)
  {
    end_io_cache(&log_file);

    if (my_sync(log_file.file, MYF(MY_WME)) && ! write_error)
    {
      write_error= 1;
      errmsg_printf(ERRMSG_LVL_ERROR, ER(ER_ERROR_ON_WRITE), name, errno);
    }

    if (my_close(log_file.file, MYF(MY_WME)) && ! write_error)
    {
      write_error= 1;
      errmsg_printf(ERRMSG_LVL_ERROR, ER(ER_ERROR_ON_WRITE), name, errno);
    }
  }

  log_state= (exiting & LOG_CLOSE_TO_BE_OPENED) ? LOG_TO_BE_OPENED : LOG_CLOSED;
  if (name)
  {
    free(name);
    name= NULL;
  }
  return;
}

/** This is called only once. */

void DRIZZLE_LOG::cleanup()
{
  if (inited)
  {
    inited= 0;
    (void) pthread_mutex_destroy(&LOCK_log);
    close(0);
  }
  return;
}


int DRIZZLE_LOG::generate_new_name(char *new_name, const char *log_name)
{
  fn_format(new_name, log_name, drizzle_data_home, "", 4);
  if (log_type == LOG_BIN)
  {
    if (!fn_ext(log_name)[0])
    {
      if (find_uniq_filename(new_name))
      {
	errmsg_printf(ERRMSG_LVL_ERROR, ER(ER_NO_UNIQUE_LOGFILE), log_name);
	return 1;
      }
    }
  }
  return 0;
}


/**
  @todo
  The following should be using fn_format();  We just need to
  first change fn_format() to cut the file name if it's too long.
*/
const char *DRIZZLE_LOG::generate_name(const char *log_name,
                                      const char *suffix,
                                      bool strip_ext, char *buff)
{
  if (!log_name || !log_name[0])
  {
    strncpy(buff, pidfile_name, FN_REFLEN - strlen(suffix) - 1);
    return (const char *)
      fn_format(buff, buff, "", suffix, MYF(MY_REPLACE_EXT|MY_REPLACE_DIR));
  }
  // get rid of extension if the log is binary to avoid problems
  if (strip_ext)
  {
    char *p= fn_ext(log_name);
    uint32_t length= cmin((uint32_t)(p - log_name), FN_REFLEN);
    strncpy(buff, log_name, length);
    buff[length]= '\0';
    return (const char*)buff;
  }
  return log_name;
}



DRIZZLE_BIN_LOG::DRIZZLE_BIN_LOG()
  :bytes_written(0), prepared_xids(0), file_id(1), open_count(1),
   need_start_event(true), m_table_map_version(0),
   description_event_for_exec(0), description_event_for_queue(0)
{
  /*
    We don't want to initialize locks here as such initialization depends on
    safe_mutex (when using safe_mutex) which depends on MY_INIT(), which is
    called only in main(). Doing initialization here would make it happen
    before main().
  */
  index_file_name[0] = 0;
  memset(&index_file, 0, sizeof(index_file));
}

/* this is called only once */

void DRIZZLE_BIN_LOG::cleanup()
{
  if (inited)
  {
    inited= 0;
    close(LOG_CLOSE_INDEX|LOG_CLOSE_STOP_EVENT);
    delete description_event_for_queue;
    delete description_event_for_exec;
    (void) pthread_mutex_destroy(&LOCK_log);
    (void) pthread_mutex_destroy(&LOCK_index);
    (void) pthread_cond_destroy(&update_cond);
  }
  return;
}


/* Init binlog-specific vars */
void DRIZZLE_BIN_LOG::init(bool no_auto_events_arg, ulong max_size_arg)
{
  no_auto_events= no_auto_events_arg;
  max_size= max_size_arg;
  return;
}


void DRIZZLE_BIN_LOG::init_pthread_objects()
{
  assert(inited == 0);
  inited= 1;
  (void) pthread_mutex_init(&LOCK_log, MY_MUTEX_INIT_SLOW);
  (void) pthread_mutex_init(&LOCK_index, MY_MUTEX_INIT_SLOW);
  (void) pthread_cond_init(&update_cond, 0);
}


bool DRIZZLE_BIN_LOG::open_index_file(const char *index_file_name_arg,
                                const char *log_name)
{
  File index_file_nr= -1;
  assert(!my_b_inited(&index_file));

  /*
    First open of this class instance
    Create an index file that will hold all file names uses for logging.
    Add new entries to the end of it.
  */
  myf opt= MY_UNPACK_FILENAME;
  if (!index_file_name_arg)
  {
    index_file_name_arg= log_name;    // Use same basename for index file
    opt= MY_UNPACK_FILENAME | MY_REPLACE_EXT;
  }
  fn_format(index_file_name, index_file_name_arg, drizzle_data_home,
            ".index", opt);
  if ((index_file_nr= my_open(index_file_name,
                              O_RDWR | O_CREAT,
                              MYF(MY_WME))) < 0 ||
       my_sync(index_file_nr, MYF(MY_WME)) ||
       init_io_cache(&index_file, index_file_nr,
                     IO_SIZE, WRITE_CACHE,
                     lseek(index_file_nr,0,SEEK_END),
			0, MYF(MY_WME | MY_WAIT_IF_FULL)))
  {
    /*
      TODO: all operations creating/deleting the index file or a log, should
      call my_sync_dir() or my_sync_dir_by_file() to be durable.
      TODO: file creation should be done with my_create() not my_open().
    */
    if (index_file_nr >= 0)
      my_close(index_file_nr,MYF(0));
    return true;
  }
  return false;
}


/**
  Open a (new) binlog file.

  - Open the log file and the index file. Register the new
  file name in it
  - When calling this when the file is in use, you must have a locks
  on LOCK_log and LOCK_index.

  @retval
    0	ok
  @retval
    1	error
*/

bool DRIZZLE_BIN_LOG::open(const char *log_name,
                         enum_log_type log_type_arg,
                         const char *new_name,
                         enum cache_type io_cache_type_arg,
                         bool no_auto_events_arg,
                         ulong max_size_arg,
                         bool null_created_arg)
{
  File file= -1;

  write_error=0;

  /* open the main log file */
  if (DRIZZLE_LOG::open(log_name, log_type_arg, new_name, io_cache_type_arg))
    return(1);                            /* all warnings issued */

  init(no_auto_events_arg, max_size_arg);

  open_count++;

  assert(log_type == LOG_BIN);

  {
    bool write_file_name_to_index_file=0;

    if (!my_b_filelength(&log_file))
    {
      /*
	The binary log file was empty (probably newly created)
	This is the normal case and happens when the user doesn't specify
	an extension for the binary log files.
	In this case we write a standard header to it.
      */
      if (my_b_safe_write(&log_file, (unsigned char*) BINLOG_MAGIC,
			  BIN_LOG_HEADER_SIZE))
        goto err;
      bytes_written+= BIN_LOG_HEADER_SIZE;
      write_file_name_to_index_file= 1;
    }

    assert(my_b_inited(&index_file) != 0);
    reinit_io_cache(&index_file, WRITE_CACHE,
                    my_b_filelength(&index_file), 0, 0);
    if (need_start_event && !no_auto_events)
    {
      /*
        In 4.x we set need_start_event=0 here, but in 5.0 we want a Start event
        even if this is not the very first binlog.
      */
      Format_description_log_event s(BINLOG_VERSION);
      /*
        don't set LOG_EVENT_BINLOG_IN_USE_F for SEQ_READ_APPEND io_cache
        as we won't be able to reset it later
      */
      if (io_cache_type == WRITE_CACHE)
        s.flags|= LOG_EVENT_BINLOG_IN_USE_F;
      if (!s.is_valid())
        goto err;
      s.dont_set_created= null_created_arg;
      if (s.write(&log_file))
        goto err;
      bytes_written+= s.data_written;
    }
    if (description_event_for_queue &&
        description_event_for_queue->binlog_version>=4)
    {
      /*
        This is a relay log written to by the I/O slave thread.
        Write the event so that others can later know the format of this relay
        log.
        Note that this event is very close to the original event from the
        master (it has binlog version of the master, event types of the
        master), so this is suitable to parse the next relay log's event. It
        has been produced by
        Format_description_log_event::Format_description_log_event(char* buf,).
        Why don't we want to write the description_event_for_queue if this
        event is for format<4 (3.23 or 4.x): this is because in that case, the
        description_event_for_queue describes the data received from the
        master, but not the data written to the relay log (*conversion*),
        which is in format 4 (slave's).
      */
      /*
        Set 'created' to 0, so that in next relay logs this event does not
        trigger cleaning actions on the slave in
        Format_description_log_event::apply_event_impl().
      */
      description_event_for_queue->created= 0;
      /* Don't set log_pos in event header */
      description_event_for_queue->artificial_event=1;

      if (description_event_for_queue->write(&log_file))
        goto err;
      bytes_written+= description_event_for_queue->data_written;
    }
    if (flush_io_cache(&log_file) ||
        my_sync(log_file.file, MYF(MY_WME)))
      goto err;

    if (write_file_name_to_index_file)
    {
      /*
        As this is a new log file, we write the file name to the index
        file. As every time we write to the index file, we sync it.
      */
      if (my_b_write(&index_file, (unsigned char*) log_file_name,
		     strlen(log_file_name)) ||
	  my_b_write(&index_file, (unsigned char*) "\n", 1) ||
	  flush_io_cache(&index_file) ||
          my_sync(index_file.file, MYF(MY_WME)))
	goto err;
    }
  }
  log_state= LOG_OPENED;

  return(0);

err:
  errmsg_printf(ERRMSG_LVL_ERROR, _("Could not use %s for logging (error %d). "
                    "Turning logging off for the whole duration of the "
                    "Drizzle server process. "
                    "To turn it on again: fix the cause, "
                    "shutdown the Drizzle server and restart it."),
                    name, errno);
  if (file >= 0)
    my_close(file,MYF(0));
  end_io_cache(&log_file);
  end_io_cache(&index_file);
  if (name)
  {
    free(name);
    name= NULL;
  }
  log_state= LOG_CLOSED;
  return(1);
}


int DRIZZLE_BIN_LOG::get_current_log(LOG_INFO* linfo)
{
  pthread_mutex_lock(&LOCK_log);
  int ret = raw_get_current_log(linfo);
  pthread_mutex_unlock(&LOCK_log);
  return ret;
}

int DRIZZLE_BIN_LOG::raw_get_current_log(LOG_INFO* linfo)
{
  strncpy(linfo->log_file_name, log_file_name, sizeof(linfo->log_file_name)-1);
  linfo->pos = my_b_tell(&log_file);
  return 0;
}

/**
  Find the position in the log-index-file for the given log name.

  @param linfo		Store here the found log file name and position to
                       the NEXT log file name in the index file.
  @param log_name	Filename to find in the index file.
                       Is a null pointer if we want to read the first entry
  @param need_lock	Set this to 1 if the parent doesn't already have a
                       lock on LOCK_index

  @note
    On systems without the truncate function the file will end with one or
    more empty lines.  These will be ignored when reading the file.

  @retval
    0			ok
  @retval
    LOG_INFO_EOF	        End of log-index-file found
  @retval
    LOG_INFO_IO		Got IO error while reading file
*/

int DRIZZLE_BIN_LOG::find_log_pos(LOG_INFO *linfo, const char *log_name,
			    bool need_lock)
{
  int error= 0;
  char *fname= linfo->log_file_name;
  uint32_t log_name_len= log_name ? (uint) strlen(log_name) : 0;

  /*
    Mutex needed because we need to make sure the file pointer does not
    move from under our feet
  */
  if (need_lock)
    pthread_mutex_lock(&LOCK_index);
  safe_mutex_assert_owner(&LOCK_index);

  /* As the file is flushed, we can't get an error here */
  (void) reinit_io_cache(&index_file, READ_CACHE, (my_off_t) 0, 0, 0);

  for (;;)
  {
    uint32_t length;
    my_off_t offset= my_b_tell(&index_file);
    /* If we get 0 or 1 characters, this is the end of the file */

    if ((length= my_b_gets(&index_file, fname, FN_REFLEN)) <= 1)
    {
      /* Did not find the given entry; Return not found or error */
      error= !index_file.error ? LOG_INFO_EOF : LOG_INFO_IO;
      break;
    }

    // if the log entry matches, null string matching anything
    if (!log_name ||
	(log_name_len == length-1 && fname[log_name_len] == '\n' &&
	 !memcmp(fname, log_name, log_name_len)))
    {
      fname[length-1]=0;			// remove last \n
      linfo->index_file_start_offset= offset;
      linfo->index_file_offset = my_b_tell(&index_file);
      break;
    }
  }

  if (need_lock)
    pthread_mutex_unlock(&LOCK_index);
  return(error);
}


/**
  Find the position in the log-index-file for the given log name.

  @param
    linfo		Store here the next log file name and position to
			the file name after that.
  @param
    need_lock		Set this to 1 if the parent doesn't already have a
			lock on LOCK_index

  @note
    - Before calling this function, one has to call find_log_pos()
    to set up 'linfo'
    - Mutex needed because we need to make sure the file pointer does not move
    from under our feet

  @retval
    0			ok
  @retval
    LOG_INFO_EOF	        End of log-index-file found
  @retval
    LOG_INFO_IO		Got IO error while reading file
*/

int DRIZZLE_BIN_LOG::find_next_log(LOG_INFO* linfo, bool need_lock)
{
  int error= 0;
  uint32_t length;
  char *fname= linfo->log_file_name;

  if (need_lock)
    pthread_mutex_lock(&LOCK_index);
  safe_mutex_assert_owner(&LOCK_index);

  /* As the file is flushed, we can't get an error here */
  (void) reinit_io_cache(&index_file, READ_CACHE, linfo->index_file_offset, 0,
			 0);

  linfo->index_file_start_offset= linfo->index_file_offset;
  if ((length=my_b_gets(&index_file, fname, FN_REFLEN)) <= 1)
  {
    error = !index_file.error ? LOG_INFO_EOF : LOG_INFO_IO;
    goto err;
  }
  fname[length-1]=0;				// kill \n
  linfo->index_file_offset = my_b_tell(&index_file);

err:
  if (need_lock)
    pthread_mutex_unlock(&LOCK_index);
  return error;
}


/**
  Delete all logs refered to in the index file.
  Start writing to a new log file.

  The new index file will only contain this file.

  @param session		Thread

  @note
    If not called from slave thread, write start event to new log

  @retval
    0	ok
  @retval
    1   error
*/

bool DRIZZLE_BIN_LOG::reset_logs(Session* session)
{
  LOG_INFO linfo;
  bool error=0;
  const char* save_name;

  /*
    We need to get both locks to be sure that no one is trying to
    write to the index log file.
  */
  pthread_mutex_lock(&LOCK_log);
  pthread_mutex_lock(&LOCK_index);

  /*
    The following mutex is needed to ensure that no threads call
    'delete session' as we would then risk missing a 'rollback' from this
    thread. If the transaction involved MyISAM tables, it should go
    into binlog even on rollback.
  */
  pthread_mutex_lock(&LOCK_thread_count);

  /* Save variables so that we can reopen the log */
  save_name=name;
  name=0;					// Protect against free
  close(LOG_CLOSE_TO_BE_OPENED);

  /* First delete all old log files */

  if (find_log_pos(&linfo, NULL, 0))
  {
    error=1;
    goto err;
  }

  for (;;)
  {
    if ((error= my_delete_allow_opened(linfo.log_file_name, MYF(0))) != 0)
    {
      if (my_errno == ENOENT)
      {
        push_warning_printf(current_session, DRIZZLE_ERROR::WARN_LEVEL_WARN,
                            ER_LOG_PURGE_NO_FILE, ER(ER_LOG_PURGE_NO_FILE),
                            linfo.log_file_name);
        errmsg_printf(ERRMSG_LVL_INFO, _("Failed to delete file '%s'"),
                              linfo.log_file_name);
        my_errno= 0;
        error= 0;
      }
      else
      {
        push_warning_printf(current_session, DRIZZLE_ERROR::WARN_LEVEL_ERROR,
                            ER_BINLOG_PURGE_FATAL_ERR,
                            _("a problem with deleting %s; "
                            "consider examining correspondence "
                            "of your binlog index file "
                            "to the actual binlog files"),
                            linfo.log_file_name);
        error= 1;
        goto err;
      }
    }
    if (find_next_log(&linfo, 0))
      break;
  }

  /* Start logging with a new file */
  close(LOG_CLOSE_INDEX);
  if ((error= my_delete_allow_opened(index_file_name, MYF(0))))	// Reset (open will update)
  {
    if (my_errno == ENOENT)
    {
      push_warning_printf(current_session, DRIZZLE_ERROR::WARN_LEVEL_WARN,
                          ER_LOG_PURGE_NO_FILE, ER(ER_LOG_PURGE_NO_FILE),
                          index_file_name);
      errmsg_printf(ERRMSG_LVL_INFO, _("Failed to delete file '%s'"),
                            index_file_name);
      my_errno= 0;
      error= 0;
    }
    else
    {
      push_warning_printf(current_session, DRIZZLE_ERROR::WARN_LEVEL_ERROR,
                          ER_BINLOG_PURGE_FATAL_ERR,
                          "a problem with deleting %s; "
                          "consider examining correspondence "
                          "of your binlog index file "
                          "to the actual binlog files",
                          index_file_name);
      error= 1;
      goto err;
    }
  }
  if (!session->slave_thread)
    need_start_event=1;
  if (!open_index_file(index_file_name, 0))
    open(save_name, log_type, 0, io_cache_type, no_auto_events, max_size, 0);
  free((unsigned char*) save_name);

err:
  pthread_mutex_unlock(&LOCK_thread_count);
  pthread_mutex_unlock(&LOCK_index);
  pthread_mutex_unlock(&LOCK_log);
  return(error);
}


/**
  Delete relay log files prior to rli->group_relay_log_name
  (i.e. all logs which are not involved in a non-finished group
  (transaction)), remove them from the index file and start on next
  relay log.

  IMPLEMENTATION
  - Protects index file with LOCK_index
  - Delete relevant relay log files
  - Copy all file names after these ones to the front of the index file
  - If the OS has truncate, truncate the file, else fill it with \n'
  - Read the next file name from the index file and store in rli->linfo

  @param rli	       Relay log information
  @param included     If false, all relay logs that are strictly before
                      rli->group_relay_log_name are deleted ; if true, the
                      latter is deleted too (i.e. all relay logs
                      read by the SQL slave thread are deleted).

  @note
    - This is only called from the slave-execute thread when it has read
    all commands from a relay log and want to switch to a new relay log.
    - When this happens, we can be in an active transaction as
    a transaction can span over two relay logs
    (although it is always written as a single block to the master's binary
    log, hence cannot span over two master's binary logs).

  @retval
    0			ok
  @retval
    LOG_INFO_EOF	        End of log-index-file found
  @retval
    LOG_INFO_SEEK	Could not allocate IO cache
  @retval
    LOG_INFO_IO		Got IO error while reading file
*/


int DRIZZLE_BIN_LOG::purge_first_log(Relay_log_info*, bool)
{
  return 0;
}

/**
  Update log index_file.
*/

int DRIZZLE_BIN_LOG::update_log_index(LOG_INFO*, bool)
{
  return 0;
}

/**
  Remove all logs before the given log from disk and from the index file.

  @param to_log	      Delete all log file name before this file.
  @param included            If true, to_log is deleted too.
  @param need_mutex
  @param need_update_threads If we want to update the log coordinates of
                             all threads. False for relay logs, true otherwise.
  @param freed_log_space     If not null, decrement this variable of
                             the amount of log space freed

  @note
    If any of the logs before the deleted one is in use,
    only purge logs up to this one.

  @retval
    0			ok
  @retval
    LOG_INFO_EOF		to_log not found
    LOG_INFO_EMFILE             too many files opened
    LOG_INFO_FATAL              if any other than ENOENT error from
                                stat() or my_delete()
*/

int DRIZZLE_BIN_LOG::purge_logs(const char *, bool, bool, bool, uint64_t *)
{
  return 0;
}

/**
  Remove all logs before the given file date from disk and from the
  index file.

  @param session		Thread pointer
  @param before_date	Delete all log files before given date.

  @note
    If any of the logs before the deleted one is in use,
    only purge logs up to this one.

  @retval
    0				ok
  @retval
    LOG_INFO_PURGE_NO_ROTATE	Binary file that can't be rotated
    LOG_INFO_FATAL              if any other than ENOENT error from
                                stat() or my_delete()
*/

int DRIZZLE_BIN_LOG::purge_logs_before_date(time_t)
{
  return 0;
}


/**
  Create a new log file name.

  @param buf		buf of at least FN_REFLEN where new name is stored

  @note
    If file name will be longer then FN_REFLEN it will be truncated
*/

void DRIZZLE_BIN_LOG::make_log_name(char* buf, const char* log_ident)
{
  uint32_t dir_len = dirname_length(log_file_name);
  if (dir_len >= FN_REFLEN)
    dir_len=FN_REFLEN-1;
  strncpy(buf, log_file_name, dir_len);
  strncpy(buf+dir_len, log_ident, FN_REFLEN - dir_len -1);
}


/**
  Check if we are writing/reading to the given log file.
*/

bool DRIZZLE_BIN_LOG::is_active(const char *log_file_name_arg)
{
  return !strcmp(log_file_name, log_file_name_arg);
}


/*
  Wrappers around new_file_impl to avoid using argument
  to control locking. The argument 1) less readable 2) breaks
  incapsulation 3) allows external access to the class without
  a lock (which is not possible with private new_file_without_locking
  method).
*/

void DRIZZLE_BIN_LOG::new_file()
{
  new_file_impl(1);
}


void DRIZZLE_BIN_LOG::new_file_without_locking()
{
  new_file_impl(0);
}


/**
  Start writing to a new log file or reopen the old file.

  @param need_lock		Set to 1 if caller has not locked LOCK_log

  @note
    The new file name is stored last in the index file
*/

void DRIZZLE_BIN_LOG::new_file_impl(bool need_lock)
{
  char new_name[FN_REFLEN], *new_name_ptr, *old_name;

  if (!is_open())
  {
    return;
  }

  if (need_lock)
    pthread_mutex_lock(&LOCK_log);
  pthread_mutex_lock(&LOCK_index);

  safe_mutex_assert_owner(&LOCK_log);
  safe_mutex_assert_owner(&LOCK_index);

  /*
    if binlog is used as tc log, be sure all xids are "unlogged",
    so that on recover we only need to scan one - latest - binlog file
    for prepared xids. As this is expected to be a rare event,
    simple wait strategy is enough. We're locking LOCK_log to be sure no
    new Xid_log_event's are added to the log (and prepared_xids is not
    increased), and waiting on COND_prep_xids for late threads to
    catch up.
  */
  if (prepared_xids)
  {
    tc_log_page_waits++;
    pthread_mutex_lock(&LOCK_prep_xids);
    while (prepared_xids) {
      pthread_cond_wait(&COND_prep_xids, &LOCK_prep_xids);
    }
    pthread_mutex_unlock(&LOCK_prep_xids);
  }

  /* Reuse old name if not binlog and not update log */
  new_name_ptr= name;

  /*
    If user hasn't specified an extension, generate a new log name
    We have to do this here and not in open as we want to store the
    new file name in the current binary log file.
  */
  if (generate_new_name(new_name, name))
    goto end;
  new_name_ptr=new_name;

  if (log_type == LOG_BIN)
  {
    if (!no_auto_events)
    {
      /*
        We log the whole file name for log file as the user may decide
        to change base names at some point.
      */
      Rotate_log_event r(new_name+dirname_length(new_name),
                         0, LOG_EVENT_OFFSET, 0);
      r.write(&log_file);
      bytes_written += r.data_written;
    }
    /*
      Update needs to be signalled even if there is no rotate event
      log rotation should give the waiting thread a signal to
      discover EOF and move on to the next log.
    */
    signal_update();
  }
  old_name=name;
  name=0;				// Don't free name
  close(LOG_CLOSE_TO_BE_OPENED);

  /*
     Note that at this point, log_state != LOG_CLOSED (important for is_open()).
  */

  /*
     new_file() is only used for rotation (in FLUSH LOGS or because size >
     max_binlog_size or max_relay_log_size).
     If this is a binary log, the Format_description_log_event at the beginning of
     the new file should have created=0 (to distinguish with the
     Format_description_log_event written at server startup, which should
     trigger temp tables deletion on slaves.
  */

  open(old_name, log_type, new_name_ptr,
       io_cache_type, no_auto_events, max_size, 1);
  free(old_name);

end:
  if (need_lock)
    pthread_mutex_unlock(&LOCK_log);
  pthread_mutex_unlock(&LOCK_index);

  return;
}


bool DRIZZLE_BIN_LOG::append(Log_event* ev)
{
  bool error = 0;
  pthread_mutex_lock(&LOCK_log);

  assert(log_file.type == SEQ_READ_APPEND);
  /*
    Log_event::write() is smart enough to use my_b_write() or
    my_b_append() depending on the kind of cache we have.
  */
  if (ev->write(&log_file))
  {
    error=1;
    goto err;
  }
  bytes_written+= ev->data_written;
  if ((uint) my_b_append_tell(&log_file) > max_size)
    new_file_without_locking();

err:
  pthread_mutex_unlock(&LOCK_log);
  signal_update();				// Safe as we don't call close
  return(error);
}


bool DRIZZLE_BIN_LOG::appendv(const char* buf, uint32_t len,...)
{
  bool error= 0;
  va_list(args);
  va_start(args,len);

  assert(log_file.type == SEQ_READ_APPEND);

  safe_mutex_assert_owner(&LOCK_log);
  do
  {
    if (my_b_append(&log_file,(unsigned char*) buf,len))
    {
      error= 1;
      goto err;
    }
    bytes_written += len;
  } while ((buf=va_arg(args,const char*)) && (len=va_arg(args,uint)));
  if ((uint) my_b_append_tell(&log_file) > max_size)
    new_file_without_locking();

err:
  if (!error)
    signal_update();
  return(error);
}


bool DRIZZLE_BIN_LOG::flush_and_sync()
{
  int err=0, fd=log_file.file;
  safe_mutex_assert_owner(&LOCK_log);
  if (flush_io_cache(&log_file))
    return 1;
  if (++sync_binlog_counter >= sync_binlog_period && sync_binlog_period)
  {
    sync_binlog_counter= 0;
    err=my_sync(fd, MYF(MY_WME));
  }
  return err;
}

void DRIZZLE_BIN_LOG::start_union_events(Session *session, query_id_t query_id_param)
{
  assert(!session->binlog_evt_union.do_union);
  session->binlog_evt_union.do_union= true;
  session->binlog_evt_union.unioned_events= false;
  session->binlog_evt_union.unioned_events_trans= false;
  session->binlog_evt_union.first_query_id= query_id_param;
}

void DRIZZLE_BIN_LOG::stop_union_events(Session *session)
{
  assert(session->binlog_evt_union.do_union);
  session->binlog_evt_union.do_union= false;
}

bool DRIZZLE_BIN_LOG::is_query_in_union(Session *session, query_id_t query_id_param)
{
  return (session->binlog_evt_union.do_union &&
          query_id_param >= session->binlog_evt_union.first_query_id);
}

/*
  Moves the last bunch of rows from the pending Rows event to the binlog
  (either cached binlog if transaction, or disk binlog). Sets a new pending
  event.
*/
int
DRIZZLE_BIN_LOG::flush_and_set_pending_rows_event(Session *, Rows_log_event*)
{
  assert(drizzle_bin_log.is_open());

  return false;
}

/**
  Write an event to the binary log.
*/

bool DRIZZLE_BIN_LOG::write(Log_event *event_info)
{
  Session *session= event_info->session;
  bool error= 1;

  if (session->binlog_evt_union.do_union)
  {
    /*
      In Stored function; Remember that function call caused an update.
      We will log the function call to the binary log on function exit
    */
    session->binlog_evt_union.unioned_events= true;
    session->binlog_evt_union.unioned_events_trans |= event_info->cache_stmt;
    return(0);
  }

  /*
    Flush the pending rows event to the transaction cache or to the
    log file.  Since this function potentially aquire the LOCK_log
    mutex, we do this before aquiring the LOCK_log mutex in this
    function.

    We only end the statement if we are in a top-level statement.  If
    we are inside a stored function, we do not end the statement since
    this will close all tables on the slave.
  */

  pthread_mutex_lock(&LOCK_log);

  /*
     In most cases this is only called if 'is_open()' is true; in fact this is
     mostly called if is_open() *was* true a few instructions before, but it
     could have changed since.
  */
  if (likely(is_open()))
  {
    IO_CACHE *file= &log_file;
    /*
      In the future we need to add to the following if tests like
      "do the involved tables match (to be implemented)
      binlog_[wild_]{do|ignore}_table?" (WL#1049)"
    */
    if (session && !(session->options & OPTION_BIN_LOG))
    {
      pthread_mutex_unlock(&LOCK_log);
      return(0);
    }

    /*
       Write the SQL command
     */

    if (event_info->write(file))
      goto err;

    if (file == &log_file) // we are writing to the real log (disk)
    {
      if (flush_and_sync())
	goto err;
      signal_update();
      rotate_and_purge(RP_LOCK_LOG_IS_ALREADY_LOCKED);
    }
    error=0;

err:
    if (error)
    {
      if (my_errno == EFBIG)
	my_message(ER_TRANS_CACHE_FULL, ER(ER_TRANS_CACHE_FULL), MYF(0));
      else
	my_error(ER_ERROR_ON_WRITE, MYF(0), name, errno);
      write_error=1;
    }
  }

  if (event_info->flags & LOG_EVENT_UPDATE_TABLE_MAP_VERSION_F)
    ++m_table_map_version;

  pthread_mutex_unlock(&LOCK_log);
  return(error);
}

void DRIZZLE_BIN_LOG::rotate_and_purge(uint32_t flags)
{
  if (!(flags & RP_LOCK_LOG_IS_ALREADY_LOCKED))
    pthread_mutex_lock(&LOCK_log);
  if ((flags & RP_FORCE_ROTATE) ||
      (my_b_tell(&log_file) >= (my_off_t) max_size))
  {
    new_file_without_locking();
    if (expire_logs_days)
    {
      time_t purge_time= time(0) - expire_logs_days*24*60*60;
      if (purge_time >= 0)
        purge_logs_before_date(purge_time);
    }
  }
  if (!(flags & RP_LOCK_LOG_IS_ALREADY_LOCKED))
    pthread_mutex_unlock(&LOCK_log);
}

uint32_t DRIZZLE_BIN_LOG::next_file_id()
{
  uint32_t res;
  pthread_mutex_lock(&LOCK_log);
  res = file_id++;
  pthread_mutex_unlock(&LOCK_log);
  return res;
}


/*
  Write the contents of a cache to the binary log.

  SYNOPSIS
    write_cache()
    cache    Cache to write to the binary log
    lock_log True if the LOCK_log mutex should be aquired, false otherwise
    sync_log True if the log should be flushed and sync:ed

  DESCRIPTION
    Write the contents of the cache to the binary log. The cache will
    be reset as a READ_CACHE to be able to read the contents from it.
 */

int DRIZZLE_BIN_LOG::write_cache(IO_CACHE *, bool, bool)
{
  return 0;                                     // All OK
}

/**
  Write a cached log entry to the binary log.
  - To support transaction over replication, we wrap the transaction
  with BEGIN/COMMIT or BEGIN/ROLLBACK in the binary log.
  We want to write a BEGIN/ROLLBACK block when a non-transactional table
  was updated in a transaction which was rolled back. This is to ensure
  that the same updates are run on the slave.

  @param session
  @param cache		The cache to copy to the binlog
  @param commit_event   The commit event to print after writing the
                        contents of the cache.

  @note
    We only come here if there is something in the cache.
  @note
    The thing in the cache is always a complete transaction.
  @note
    'cache' needs to be reinitialized after this functions returns.
*/

bool DRIZZLE_BIN_LOG::write(Session *, IO_CACHE *, Log_event *)
{
  return false;
}


/**
  Wait until we get a signal that the relay log has been updated

  @param[in] session   a Session struct
  @note
    LOCK_log must be taken before calling this function.
    It will be released at the end of the function.
*/

void DRIZZLE_BIN_LOG::wait_for_update_relay_log(Session*)
{
  return;
}


/**
  Wait until we get a signal that the binary log has been updated.
  Applies to master only.

  NOTES
  @param[in] session        a Session struct
  @param[in] timeout    a pointer to a timespec;
                        NULL means to wait w/o timeout.
  @retval    0          if got signalled on update
  @retval    non-0      if wait timeout elapsed
  @note
    LOCK_log must be taken before calling this function.
    LOCK_log is being released while the thread is waiting.
    LOCK_log is released by the caller.
*/

int DRIZZLE_BIN_LOG::wait_for_update_bin_log(Session* session,
                                           const struct timespec *timeout)
{
  int ret= 0;
  const char* old_msg = session->get_proc_info();
  old_msg= session->enter_cond(&update_cond, &LOCK_log,
                           "Master has sent all binlog to slave; "
                           "waiting for binlog to be updated");
  if (!timeout)
    pthread_cond_wait(&update_cond, &LOCK_log);
  else
    ret= pthread_cond_timedwait(&update_cond, &LOCK_log,
                                const_cast<struct timespec *>(timeout));
  return(ret);
}


/**
  Close the log file.

  @param exiting     Bitmask for one or more of the following bits:
          - LOG_CLOSE_INDEX : if we should close the index file
          - LOG_CLOSE_TO_BE_OPENED : if we intend to call open
                                     at once after close.
          - LOG_CLOSE_STOP_EVENT : write a 'stop' event to the log

  @note
    One can do an open on the object at once after doing a close.
    The internal structures are not freed until cleanup() is called
*/

void DRIZZLE_BIN_LOG::close(uint32_t)
{					// One can't set log_type here!
  return;
}


void DRIZZLE_BIN_LOG::set_max_size(ulong)
{
  return;
}


/**
  Check if a string is a valid number.

  @param str			String to test
  @param res			Store value here
  @param allow_wildcards	Set to 1 if we should ignore '%' and '_'

  @note
    For the moment the allow_wildcards argument is not used
    Should be move to some other file.

  @retval
    1	String is a number
  @retval
    0	Error
*/

static bool test_if_number(register const char *, long *, bool)
{
  return(1);			/* Number ok */
} /* test_if_number */

void DRIZZLE_BIN_LOG::signal_update()
{
  return;
}

/********* transaction coordinator log for 2pc - mmap() based solution *******/

/*
  the log consists of a file, mmapped to a memory.
  file is divided on pages of tc_log_page_size size.
  (usable size of the first page is smaller because of log header)
  there's PAGE control structure for each page
  each page (or rather PAGE control structure) can be in one of three
  states - active, syncing, pool.
  there could be only one page in active or syncing states,
  but many in pool - pool is fifo queue.
  usual lifecycle of a page is pool->active->syncing->pool
  "active" page - is a page where new xid's are logged.
  the page stays active as long as syncing slot is taken.
  "syncing" page is being synced to disk. no new xid can be added to it.
  when the sync is done the page is moved to a pool and an active page
  becomes "syncing".

  the result of such an architecture is a natural "commit grouping" -
  If commits are coming faster than the system can sync, they do not
  stall. Instead, all commit that came since the last sync are
  logged to the same page, and they all are synced with the next -
  one - sync. Thus, thought individual commits are delayed, throughput
  is not decreasing.

  when a xid is added to an active page, the thread of this xid waits
  for a page's condition until the page is synced. when syncing slot
  becomes vacant one of these waiters is awaken to take care of syncing.
  it syncs the page and signals all waiters that the page is synced.
  PAGE::waiters is used to count these waiters, and a page may never
  become active again until waiters==0 (that is all waiters from the
  previous sync have noticed the sync was completed)

  note, that the page becomes "dirty" and has to be synced only when a
  new xid is added into it. Removing a xid from a page does not make it
  dirty - we don't sync removals to disk.
*/

uint64_t tc_log_page_waits= 0;

#ifdef HAVE_MMAP

#define TC_LOG_HEADER_SIZE (sizeof(tc_log_magic)+1)

static const char tc_log_magic[]={(char) 254, 0x23, 0x05, 0x74};

uint64_t opt_tc_log_size= TC_LOG_MIN_SIZE;
uint64_t tc_log_max_pages_used= 0;
uint64_t tc_log_page_size= 0;
uint64_t tc_log_cur_pages_used= 0;

int TC_LOG_MMAP::open(const char *opt_name)
{
  uint32_t i;
  bool crashed= false;
  PAGE *pg;

  assert(total_ha_2pc > 1);
  assert(opt_name && opt_name[0]);

  tc_log_page_size= getpagesize();
  assert(TC_LOG_PAGE_SIZE % tc_log_page_size == 0);

  fn_format(logname,opt_name,drizzle_data_home,"",MY_UNPACK_FILENAME);
  if ((fd= my_open(logname, O_RDWR, MYF(0))) < 0)
  {
    if (my_errno != ENOENT)
      goto err;
    if (using_heuristic_recover())
      return 1;
    if ((fd= my_create(logname, CREATE_MODE, O_RDWR, MYF(MY_WME))) < 0)
      goto err;
    inited=1;
    file_length= opt_tc_log_size;
    if (ftruncate(fd, file_length))
      goto err;
  }
  else
  {
    inited= 1;
    crashed= true;
    errmsg_printf(ERRMSG_LVL_INFO, _("Recovering after a crash using %s"), opt_name);
    if (tc_heuristic_recover)
    {
      errmsg_printf(ERRMSG_LVL_ERROR, _("Cannot perform automatic crash recovery when "
                      "--tc-heuristic-recover is used"));
      goto err;
    }
    file_length= lseek(fd, 0, SEEK_END);
    if (file_length == OFF_T_MAX || file_length % tc_log_page_size)
      goto err;
  }

  data= (unsigned char *)my_mmap(0, (size_t)file_length, PROT_READ|PROT_WRITE,
                        MAP_NOSYNC|MAP_SHARED, fd, 0);
  if (data == MAP_FAILED)
  {
    my_errno=errno;
    goto err;
  }
  inited=2;

  npages=(uint)file_length/tc_log_page_size;
  assert(npages >= 3);             // to guarantee non-empty pool
  if (!(pages=(PAGE *)malloc(npages*sizeof(PAGE))))
    goto err;
  memset(pages, 0, npages*sizeof(PAGE));
  inited=3;
  for (pg=pages, i=0; i < npages; i++, pg++)
  {
    pg->next=pg+1;
    pg->waiters=0;
    pg->state=POOL;
    pthread_mutex_init(&pg->lock, MY_MUTEX_INIT_FAST);
    pthread_cond_init (&pg->cond, 0);
    pg->start=(my_xid *)(data + i*tc_log_page_size);
    pg->ptr=pg->start;
    pg->end=(my_xid *)(pg->start + tc_log_page_size);
    pg->size=pg->free=tc_log_page_size/sizeof(my_xid);
  }
  pages[0].size=pages[0].free=
                (tc_log_page_size-TC_LOG_HEADER_SIZE)/sizeof(my_xid);
  pages[0].start=pages[0].end-pages[0].size;
  pages[npages-1].next=0;
  inited=4;

  if (crashed && recover())
      goto err;

  memcpy(data, tc_log_magic, sizeof(tc_log_magic));
  data[sizeof(tc_log_magic)]= (unsigned char)total_ha_2pc;
  // must cast data to (char *) for solaris. Arg1 is (void *) on linux
  //   so the cast should be fine.
  msync((char *)data, tc_log_page_size, MS_SYNC);
  my_sync(fd, MYF(0));
  inited=5;

  pthread_mutex_init(&LOCK_sync,    MY_MUTEX_INIT_FAST);
  pthread_mutex_init(&LOCK_active,  MY_MUTEX_INIT_FAST);
  pthread_mutex_init(&LOCK_pool,    MY_MUTEX_INIT_FAST);
  pthread_cond_init(&COND_active, 0);
  pthread_cond_init(&COND_pool, 0);

  inited=6;

  syncing= 0;
  active=pages;
  pool=pages+1;
  pool_last=pages+npages-1;

  return 0;

err:
  close();
  return 1;
}

/**
  there is no active page, let's got one from the pool.

  Two strategies here:
    -# take the first from the pool
    -# if there're waiters - take the one with the most free space.

  @todo
    TODO page merging. try to allocate adjacent page first,
    so that they can be flushed both in one sync
*/

void TC_LOG_MMAP::get_active_from_pool()
{
  PAGE **p, **best_p=0;
  int best_free;

  if (syncing)
    pthread_mutex_lock(&LOCK_pool);

  do
  {
    best_p= p= &pool;
    if ((*p)->waiters == 0) // can the first page be used ?
      break;                // yes - take it.

    best_free=0;            // no - trying second strategy
    for (p=&(*p)->next; *p; p=&(*p)->next)
    {
      if ((*p)->waiters == 0 && (*p)->free > best_free)
      {
        best_free=(*p)->free;
        best_p=p;
      }
    }
  }
  while ((*best_p == 0 || best_free == 0) && overflow());

  active=*best_p;
  if (active->free == active->size) // we've chosen an empty page
  {
    tc_log_cur_pages_used++;
    set_if_bigger(tc_log_max_pages_used, tc_log_cur_pages_used);
  }

  if ((*best_p)->next)              // unlink the page from the pool
    *best_p=(*best_p)->next;
  else
    pool_last=*best_p;

  if (syncing)
    pthread_mutex_unlock(&LOCK_pool);
}

/**
  @todo
  perhaps, increase log size ?
*/
int TC_LOG_MMAP::overflow()
{
  /*
    simple overflow handling - just wait
    TODO perhaps, increase log size ?
    let's check the behaviour of tc_log_page_waits first
  */
  tc_log_page_waits++;
  pthread_cond_wait(&COND_pool, &LOCK_pool);
  return 1; // always return 1
}

/**
  Record that transaction XID is committed on the persistent storage.

    This function is called in the middle of two-phase commit:
    First all resources prepare the transaction, then tc_log->log() is called,
    then all resources commit the transaction, then tc_log->unlog() is called.

    All access to active page is serialized but it's not a problem, as
    we're assuming that fsync() will be a main bottleneck.
    That is, parallelizing writes to log pages we'll decrease number of
    threads waiting for a page, but then all these threads will be waiting
    for a fsync() anyway

   If tc_log == DRIZZLE_LOG then tc_log writes transaction to binlog and
   records XID in a special Xid_log_event.
   If tc_log = TC_LOG_MMAP then xid is written in a special memory-mapped
   log.

  @retval
    0  - error
  @retval
    \# - otherwise, "cookie", a number that will be passed as an argument
    to unlog() call. tc_log can define it any way it wants,
    and use for whatever purposes. TC_LOG_MMAP sets it
    to the position in memory where xid was logged to.
*/

int TC_LOG_MMAP::log_xid(Session *, my_xid xid)
{
  int err;
  PAGE *p;
  ulong cookie;

  pthread_mutex_lock(&LOCK_active);

  /*
    if active page is full - just wait...
    frankly speaking, active->free here accessed outside of mutex
    protection, but it's safe, because it only means we may miss an
    unlog() for the active page, and we're not waiting for it here -
    unlog() does not signal COND_active.
  */
  while (unlikely(active && active->free == 0))
    pthread_cond_wait(&COND_active, &LOCK_active);

  /* no active page ? take one from the pool */
  if (active == 0)
    get_active_from_pool();

  p=active;
  pthread_mutex_lock(&p->lock);

  /* searching for an empty slot */
  while (*p->ptr)
  {
    p->ptr++;
    assert(p->ptr < p->end);               // because p->free > 0
  }

  /* found! store xid there and mark the page dirty */
  cookie= (ulong)((unsigned char *)p->ptr - data);      // can never be zero
  *p->ptr++= xid;
  p->free--;
  p->state= DIRTY;

  /* to sync or not to sync - this is the question */
  pthread_mutex_unlock(&LOCK_active);
  pthread_mutex_lock(&LOCK_sync);
  pthread_mutex_unlock(&p->lock);

  if (syncing)
  {                                          // somebody's syncing. let's wait
    p->waiters++;
    /*
      note - it must be while (), not do ... while () here
      as p->state may be not DIRTY when we come here
    */
    while (p->state == DIRTY && syncing)
      pthread_cond_wait(&p->cond, &LOCK_sync);
    p->waiters--;
    err= p->state == ERROR;
    if (p->state != DIRTY)                   // page was synced
    {
      if (p->waiters == 0)
        pthread_cond_signal(&COND_pool);     // in case somebody's waiting
      pthread_mutex_unlock(&LOCK_sync);
      goto done;                             // we're done
    }
  }                                          // page was not synced! do it now
  assert(active == p && syncing == 0);
  pthread_mutex_lock(&LOCK_active);
  syncing=p;                                 // place is vacant - take it
  active=0;                                  // page is not active anymore
  pthread_cond_broadcast(&COND_active);      // in case somebody's waiting
  pthread_mutex_unlock(&LOCK_active);
  pthread_mutex_unlock(&LOCK_sync);
  err= sync();

done:
  return err ? 0 : cookie;
}

int TC_LOG_MMAP::sync()
{
  int err;

  assert(syncing != active);

  /*
    sit down and relax - this can take a while...
    note - no locks are held at this point
  */
  // must cast data to (char *) for solaris. Arg1 is (void *) on linux
  //   so the cast should be fine.
  err= msync((char *)syncing->start, 1, MS_SYNC);
  if(err==0)
    err= my_sync(fd, MYF(0));

  /* page is synced. let's move it to the pool */
  pthread_mutex_lock(&LOCK_pool);
  pool_last->next=syncing;
  pool_last=syncing;
  syncing->next=0;
  syncing->state= err ? ERROR : POOL;
  pthread_cond_broadcast(&syncing->cond);    // signal "sync done"
  pthread_cond_signal(&COND_pool);           // in case somebody's waiting
  pthread_mutex_unlock(&LOCK_pool);

  /* marking 'syncing' slot free */
  pthread_mutex_lock(&LOCK_sync);
  syncing=0;
  pthread_cond_signal(&active->cond);        // wake up a new syncer
  pthread_mutex_unlock(&LOCK_sync);
  return err;
}

/**
  erase xid from the page, update page free space counters/pointers.
  cookie points directly to the memory where xid was logged.
*/

void TC_LOG_MMAP::unlog(ulong cookie, my_xid xid)
{
  PAGE *p=pages+(cookie/tc_log_page_size);
  my_xid *x=(my_xid *)(data+cookie);

  assert(*x == xid);
  assert(x >= p->start && x < p->end);
  *x=0;

  pthread_mutex_lock(&p->lock);
  p->free++;
  assert(p->free <= p->size);
  set_if_smaller(p->ptr, x);
  if (p->free == p->size)               // the page is completely empty
    statistic_decrement(tc_log_cur_pages_used, &LOCK_status);
  if (p->waiters == 0)                 // the page is in pool and ready to rock
    pthread_cond_signal(&COND_pool);   // ping ... for overflow()
  pthread_mutex_unlock(&p->lock);
}

void TC_LOG_MMAP::close()
{
  uint32_t i;
  switch (inited) {
  case 6:
    pthread_mutex_destroy(&LOCK_sync);
    pthread_mutex_destroy(&LOCK_active);
    pthread_mutex_destroy(&LOCK_pool);
    pthread_cond_destroy(&COND_pool);
  case 5:
    data[0]='A'; // garble the first (signature) byte, in case my_delete fails
  case 4:
    for (i=0; i < npages; i++)
    {
      if (pages[i].ptr == 0)
        break;
      pthread_mutex_destroy(&pages[i].lock);
      pthread_cond_destroy(&pages[i].cond);
    }
  case 3:
    free((unsigned char*)pages);
  case 2:
    my_munmap((char*)data, (size_t)file_length);
  case 1:
    my_close(fd, MYF(0));
  }
  if (inited>=5) // cannot do in the switch because of Windows
    my_delete(logname, MYF(MY_WME));
  inited=0;
}

int TC_LOG_MMAP::recover()
{
  HASH xids;
  PAGE *p=pages, *end_p=pages+npages;

  if (memcmp(data, tc_log_magic, sizeof(tc_log_magic)))
  {
    errmsg_printf(ERRMSG_LVL_ERROR, _("Bad magic header in tc log"));
    goto err1;
  }

  /*
    the first byte after magic signature is set to current
    number of storage engines on startup
  */
  if (data[sizeof(tc_log_magic)] != total_ha_2pc)
  {
    errmsg_printf(ERRMSG_LVL_ERROR, _("Recovery failed! You must enable "
                    "exactly %d storage engines that support "
                    "two-phase commit protocol"),
                    data[sizeof(tc_log_magic)]);
    goto err1;
  }

  if (hash_init(&xids, &my_charset_bin, tc_log_page_size/3, 0,
                sizeof(my_xid), 0, 0, MYF(0)))
    goto err1;

  for ( ; p < end_p ; p++)
  {
    for (my_xid *x=p->start; x < p->end; x++)
      if (*x && my_hash_insert(&xids, (unsigned char *)x))
        goto err2; // OOM
  }

  if (ha_recover(&xids))
    goto err2;

  hash_free(&xids);
  memset(data, 0, (size_t)file_length);
  return 0;

err2:
  hash_free(&xids);
err1:
  errmsg_printf(ERRMSG_LVL_ERROR, _("Crash recovery failed. Either correct the problem "
                  "(if it's, for example, out of memory error) and restart, "
                  "or delete tc log and start drizzled with "
                  "--tc-heuristic-recover={commit|rollback}"));
  return 1;
}
#endif

TC_LOG *tc_log;
TC_LOG_DUMMY tc_log_dummy;
TC_LOG_MMAP  tc_log_mmap;

/**
  Perform heuristic recovery, if --tc-heuristic-recover was used.

  @note
    no matter whether heuristic recovery was successful or not
    mysqld must exit. So, return value is the same in both cases.

  @retval
    0	no heuristic recovery was requested
  @retval
    1   heuristic recovery was performed
*/

int TC_LOG::using_heuristic_recover()
{
  if (!tc_heuristic_recover)
    return 0;

  errmsg_printf(ERRMSG_LVL_INFO, _("Heuristic crash recovery mode"));
  if (ha_recover(0))
    errmsg_printf(ERRMSG_LVL_ERROR, _("Heuristic crash recovery failed"));
  errmsg_printf(ERRMSG_LVL_INFO, _("Please restart mysqld without --tc-heuristic-recover"));
  return 1;
}

/****** transaction coordinator log for 2pc - binlog() based solution ******/
#define TC_LOG_BINLOG DRIZZLE_BIN_LOG

/**
  @todo
  keep in-memory list of prepared transactions
  (add to list in log(), remove on unlog())
  and copy it to the new binlog if rotated
  but let's check the behaviour of tc_log_page_waits first!
*/

int TC_LOG_BINLOG::open(const char *opt_name)
{
  LOG_INFO log_info;
  int      error= 1;

  assert(total_ha_2pc > 1);
  assert(opt_name && opt_name[0]);

  pthread_mutex_init(&LOCK_prep_xids, MY_MUTEX_INIT_FAST);
  pthread_cond_init (&COND_prep_xids, 0);

  if (!my_b_inited(&index_file))
  {
    /* There was a failure to open the index file, can't open the binlog */
    cleanup();
    return 1;
  }

  if (using_heuristic_recover())
  {
    /* generate a new binlog to mask a corrupted one */
    open(opt_name, LOG_BIN, 0, WRITE_CACHE, 0, max_binlog_size, 0);
    cleanup();
    return 1;
  }

  if ((error= find_log_pos(&log_info, NULL, 1)))
  {
    if (error != LOG_INFO_EOF)
      errmsg_printf(ERRMSG_LVL_ERROR, _("find_log_pos() failed (error: %d)"), error);
    else
      error= 0;
    goto err;
  }

  {
    const char *errmsg;
    IO_CACHE    log;
    File        file;
    Log_event  *ev=0;
    Format_description_log_event fdle(BINLOG_VERSION);
    char        log_name[FN_REFLEN];

    if (! fdle.is_valid())
      goto err;

    do
    {
      strncpy(log_name, log_info.log_file_name, sizeof(log_name)-1);
    } while (!(error= find_next_log(&log_info, 1)));

    if (error !=  LOG_INFO_EOF)
    {
      errmsg_printf(ERRMSG_LVL_ERROR, _("find_log_pos() failed (error: %d)"), error);
      goto err;
    }

    if ((file= open_binlog(&log, log_name, &errmsg)) < 0)
    {
      errmsg_printf(ERRMSG_LVL_ERROR, "%s", errmsg);
      goto err;
    }

    if ((ev= Log_event::read_log_event(&log, 0, &fdle)) &&
        ev->get_type_code() == FORMAT_DESCRIPTION_EVENT &&
        ev->flags & LOG_EVENT_BINLOG_IN_USE_F)
    {
      errmsg_printf(ERRMSG_LVL_INFO, _("Recovering after a crash using %s"), opt_name);
      error= recover(&log, (Format_description_log_event *)ev);
    }
    else
      error=0;

    delete ev;
    end_io_cache(&log);
    my_close(file, MYF(MY_WME));

    if (error)
      goto err;
  }

err:
  return error;
}

/** This is called on shutdown, after ha_panic. */
void TC_LOG_BINLOG::close()
{
  assert(prepared_xids==0);
  pthread_mutex_destroy(&LOCK_prep_xids);
  pthread_cond_destroy (&COND_prep_xids);
}

/**
  @todo
  group commit

  @retval
    0    error
  @retval
    1    success
*/
int TC_LOG_BINLOG::log_xid(Session *session, my_xid xid)
{
  Xid_log_event xle(session, xid);
  /* TODO: Fix return type */
  /*
    We always commit the entire transaction when writing an XID. Also
    note that the return value is inverted.

    TODO: fix backasswards logic on this method
   */

  return replicator_end_transaction(session, true, true) ? false : true;
}

void TC_LOG_BINLOG::unlog(ulong, my_xid)
{
}

int TC_LOG_BINLOG::recover(IO_CACHE *, Format_description_log_event *)
{
  return 0;
}


bool DRIZZLE_BIN_LOG::is_table_mapped(Table *table) const
{
  return table->s->table_map_version == table_map_version();
}

/**
  Get the file name of the MySQL binlog.
  @return the name of the binlog file
*/
extern "C"
const char* drizzle_bin_log_file_name(void)
{
  return drizzle_bin_log.get_log_fname();
}


/**
  Get the current position of the MySQL binlog.
  @return byte offset from the beginning of the binlog
*/
extern "C"
uint64_t drizzle_bin_log_file_pos(void)
{
  return (uint64_t) drizzle_bin_log.get_log_file()->pos_in_file;
}


mysql_declare_plugin(binlog)
{
  DRIZZLE_STORAGE_ENGINE_PLUGIN,
  "binlog",
  "1.0",
  "MySQL AB",
  "This is a pseudo storage engine to represent the binlog in a transaction",
  PLUGIN_LICENSE_GPL,
  binlog_init, /* Plugin Init */
  NULL, /* Plugin Deinit */
  NULL,                       /* status variables                */
  NULL,                       /* system variables                */
  NULL                        /* config options                  */
}
mysql_declare_plugin_end;
