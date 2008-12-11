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

#include <drizzled/server_includes.h>
#include <drizzled/log_event.h>
#include <drizzled/replication/rli.h>
#include <drizzled/replication/mi.h>
#include <libdrizzle/libdrizzle.h>
#include <mysys/hash.h>
#include <drizzled/replication/utility.h>
#include <drizzled/replication/record.h>
#include <mysys/my_dir.h>
#include <drizzled/error.h>
#include <libdrizzle/pack.h>
#include <drizzled/sql_parse.h>
#include <drizzled/sql_base.h>
#include <drizzled/sql_load.h>

#include <algorithm>
#include <string>

#include <mysys/base64.h>
#include <mysys/my_bitmap.h>

#include <drizzled/gettext.h>
#include <libdrizzle/libdrizzle.h>
#include <drizzled/error.h>
#include <drizzled/query_id.h>
#include <drizzled/tztime.h>
#include <drizzled/slave.h>
#include <drizzled/lock.h>

using namespace std;

static const char *HA_ERR(int i)
{
  switch (i) {
  case HA_ERR_KEY_NOT_FOUND: return "HA_ERR_KEY_NOT_FOUND";
  case HA_ERR_FOUND_DUPP_KEY: return "HA_ERR_FOUND_DUPP_KEY";
  case HA_ERR_RECORD_CHANGED: return "HA_ERR_RECORD_CHANGED";
  case HA_ERR_WRONG_INDEX: return "HA_ERR_WRONG_INDEX";
  case HA_ERR_CRASHED: return "HA_ERR_CRASHED";
  case HA_ERR_WRONG_IN_RECORD: return "HA_ERR_WRONG_IN_RECORD";
  case HA_ERR_OUT_OF_MEM: return "HA_ERR_OUT_OF_MEM";
  case HA_ERR_NOT_A_TABLE: return "HA_ERR_NOT_A_TABLE";
  case HA_ERR_WRONG_COMMAND: return "HA_ERR_WRONG_COMMAND";
  case HA_ERR_OLD_FILE: return "HA_ERR_OLD_FILE";
  case HA_ERR_NO_ACTIVE_RECORD: return "HA_ERR_NO_ACTIVE_RECORD";
  case HA_ERR_RECORD_DELETED: return "HA_ERR_RECORD_DELETED";
  case HA_ERR_RECORD_FILE_FULL: return "HA_ERR_RECORD_FILE_FULL";
  case HA_ERR_INDEX_FILE_FULL: return "HA_ERR_INDEX_FILE_FULL";
  case HA_ERR_END_OF_FILE: return "HA_ERR_END_OF_FILE";
  case HA_ERR_UNSUPPORTED: return "HA_ERR_UNSUPPORTED";
  case HA_ERR_TO_BIG_ROW: return "HA_ERR_TO_BIG_ROW";
  case HA_WRONG_CREATE_OPTION: return "HA_WRONG_CREATE_OPTION";
  case HA_ERR_FOUND_DUPP_UNIQUE: return "HA_ERR_FOUND_DUPP_UNIQUE";
  case HA_ERR_UNKNOWN_CHARSET: return "HA_ERR_UNKNOWN_CHARSET";
  case HA_ERR_WRONG_MRG_TABLE_DEF: return "HA_ERR_WRONG_MRG_TABLE_DEF";
  case HA_ERR_CRASHED_ON_REPAIR: return "HA_ERR_CRASHED_ON_REPAIR";
  case HA_ERR_CRASHED_ON_USAGE: return "HA_ERR_CRASHED_ON_USAGE";
  case HA_ERR_LOCK_WAIT_TIMEOUT: return "HA_ERR_LOCK_WAIT_TIMEOUT";
  case HA_ERR_LOCK_TABLE_FULL: return "HA_ERR_LOCK_TABLE_FULL";
  case HA_ERR_READ_ONLY_TRANSACTION: return "HA_ERR_READ_ONLY_TRANSACTION";
  case HA_ERR_LOCK_DEADLOCK: return "HA_ERR_LOCK_DEADLOCK";
  case HA_ERR_CANNOT_ADD_FOREIGN: return "HA_ERR_CANNOT_ADD_FOREIGN";
  case HA_ERR_NO_REFERENCED_ROW: return "HA_ERR_NO_REFERENCED_ROW";
  case HA_ERR_ROW_IS_REFERENCED: return "HA_ERR_ROW_IS_REFERENCED";
  case HA_ERR_NO_SAVEPOINT: return "HA_ERR_NO_SAVEPOINT";
  case HA_ERR_NON_UNIQUE_BLOCK_SIZE: return "HA_ERR_NON_UNIQUE_BLOCK_SIZE";
  case HA_ERR_NO_SUCH_TABLE: return "HA_ERR_NO_SUCH_TABLE";
  case HA_ERR_TABLE_EXIST: return "HA_ERR_TABLE_EXIST";
  case HA_ERR_NO_CONNECTION: return "HA_ERR_NO_CONNECTION";
  case HA_ERR_NULL_IN_SPATIAL: return "HA_ERR_NULL_IN_SPATIAL";
  case HA_ERR_TABLE_DEF_CHANGED: return "HA_ERR_TABLE_DEF_CHANGED";
  case HA_ERR_NO_PARTITION_FOUND: return "HA_ERR_NO_PARTITION_FOUND";
  case HA_ERR_RBR_LOGGING_FAILED: return "HA_ERR_RBR_LOGGING_FAILED";
  case HA_ERR_DROP_INDEX_FK: return "HA_ERR_DROP_INDEX_FK";
  case HA_ERR_FOREIGN_DUPLICATE_KEY: return "HA_ERR_FOREIGN_DUPLICATE_KEY";
  case HA_ERR_TABLE_NEEDS_UPGRADE: return "HA_ERR_TABLE_NEEDS_UPGRADE";
  case HA_ERR_TABLE_READONLY: return "HA_ERR_TABLE_READONLY";
  case HA_ERR_AUTOINC_READ_FAILED: return "HA_ERR_AUTOINC_READ_FAILED";
  case HA_ERR_AUTOINC_ERANGE: return "HA_ERR_AUTOINC_ERANGE";
  case HA_ERR_GENERIC: return "HA_ERR_GENERIC";
  case HA_ERR_RECORD_IS_THE_SAME: return "HA_ERR_RECORD_IS_THE_SAME";
  case HA_ERR_LOGGING_IMPOSSIBLE: return "HA_ERR_LOGGING_IMPOSSIBLE";
  case HA_ERR_CORRUPT_EVENT: return "HA_ERR_CORRUPT_EVENT";
  case HA_ERR_ROWS_EVENT_APPLY : return "HA_ERR_ROWS_EVENT_APPLY";
  }
  return 0;
}

/**
   Error reporting facility for Rows_log_event::do_apply_event

   @param level     error, warning or info
   @param ha_error  HA_ERR_ code
   @param rli       pointer to the active Relay_log_info instance
   @param session       pointer to the slave thread's session
   @param table     pointer to the event's table object
   @param type      the type of the event
   @param log_name  the master binlog file name
   @param pos       the master binlog file pos (the next after the event)

*/
static void inline slave_rows_error_report(enum loglevel level, int ha_error,
                                           Relay_log_info const *rli, Session *session,
                                           Table *table, const char * type,
                                           const char *log_name, ulong pos)
{
  const char *handler_error= HA_ERR(ha_error);
  char buff[MAX_SLAVE_ERRMSG], *slider;
  const char *buff_end= buff + sizeof(buff);
  uint32_t len;
  List_iterator_fast<DRIZZLE_ERROR> it(session->warn_list);
  DRIZZLE_ERROR *err;
  buff[0]= 0;

  for (err= it++, slider= buff; err && slider < buff_end - 1;
       slider += len, err= it++)
  {
    len= snprintf(slider, buff_end - slider,
                  _(" %s, Error_code: %d;"), err->msg, err->code);
  }

  rli->report(level, session->is_error()? session->main_da.sql_errno() : 0,
              _("Could not execute %s event on table %s.%s;"
                "%s handler error %s; "
                "the event's master log %s, end_log_pos %lu"),
              type, table->s->db.str,
              table->s->table_name.str,
              buff,
              handler_error == NULL? _("<unknown>") : handler_error,
              log_name, pos);
}


/*
  Cache that will automatically be written to a dedicated file on
  destruction.

  DESCRIPTION

 */
class Write_on_release_cache
{
public:
  enum flag
  {
    FLUSH_F
  };

  typedef unsigned short flag_set;

  /*
    Constructor.

    SYNOPSIS
      Write_on_release_cache
      cache  Pointer to cache to use
      file   File to write cache to upon destruction
      flags  Flags for the cache

    DESCRIPTION

      Class used to guarantee copy of cache to file before exiting the
      current block.  On successful copy of the cache, the cache will
      be reinited as a WRITE_CACHE.

      Currently, a pointer to the cache is provided in the
      constructor, but it would be possible to create a subclass
      holding the IO_CACHE itself.
   */
  Write_on_release_cache(IO_CACHE *cache, FILE *file, flag_set flags = 0)
    : m_cache(cache), m_file(file), m_flags(flags)
  {
    reinit_io_cache(m_cache, WRITE_CACHE, 0L, false, true);
  }

  ~Write_on_release_cache()
  {
    copy_event_cache_to_file_and_reinit(m_cache, m_file);
    if (m_flags | FLUSH_F)
      fflush(m_file);
  }

  /*
    Return a pointer to the internal IO_CACHE.

    SYNOPSIS
      operator&()

    DESCRIPTION

      Function to return a pointer to the internal cache, so that the
      object can be treated as a IO_CACHE and used with the my_b_*
      IO_CACHE functions

    RETURN VALUE
      A pointer to the internal IO_CACHE.
   */
  IO_CACHE *operator&()
  {
    return m_cache;
  }

private:
  // Hidden, to prevent usage.
  Write_on_release_cache(Write_on_release_cache const&);

  IO_CACHE *m_cache;
  FILE *m_file;
  flag_set m_flags;
};

uint32_t debug_not_change_ts_if_art_event= 1; // bug#29309 simulation

/*
  pretty_print_str()
*/

static void clear_all_errors(Session *session, Relay_log_info *rli)
{
  session->is_slave_error = 0;
  session->clear_error();
  rli->clear_error();
}


/**
  Ignore error code specified on command line.
*/

inline int ignored_error_code(int err_code)
{
  return ((err_code == ER_SLAVE_IGNORED_TABLE) ||
          (use_slave_mask && bitmap_is_set(&slave_error_mask, err_code)));
}


/*
  pretty_print_str()
*/

static char *pretty_print_str(char *packet, const char *str, int len)
{
  const char *end= str + len;
  char *pos= packet;
  *pos++= '\'';
  while (str < end)
  {
    char c;
    switch ((c=*str++)) {
    case '\n': *pos++= '\\'; *pos++= 'n'; break;
    case '\r': *pos++= '\\'; *pos++= 'r'; break;
    case '\\': *pos++= '\\'; *pos++= '\\'; break;
    case '\b': *pos++= '\\'; *pos++= 'b'; break;
    case '\t': *pos++= '\\'; *pos++= 't'; break;
    case '\'': *pos++= '\\'; *pos++= '\''; break;
    case 0   : *pos++= '\\'; *pos++= '0'; break;
    default:
      *pos++= c;
      break;
    }
  }
  *pos++= '\'';
  return pos;
}


/**
  Creates a temporary name for load data infile:.

  @param buf		      Store new filename here
  @param file_id	      File_id (part of file name)
  @param event_server_id     Event_id (part of file name)
  @param ext		      Extension for file name

  @return
    Pointer to start of extension
*/

static char *slave_load_file_stem(char *buf, uint32_t file_id,
                                  int event_server_id, const char *ext)
{
  char *res;
  fn_format(buf,"SQL_LOAD-",slave_load_tmpdir, "", MY_UNPACK_FILENAME);
  to_unix_path(buf);

  buf= strchr(buf, '\0');
  buf= int10_to_str(::server_id, buf, 10);
  *buf++ = '-';
  buf= int10_to_str(event_server_id, buf, 10);
  *buf++ = '-';
  res= int10_to_str(file_id, buf, 10);
  strcpy(res, ext);                             // Add extension last
  return res;                                   // Pointer to extension
}


/**
  Delete all temporary files used for SQL_LOAD.
*/

static void cleanup_load_tmpdir()
{
  MY_DIR *dirp;
  FILEINFO *file;
  uint32_t i;
  char fname[FN_REFLEN], prefbuf[31], *p;

  if (!(dirp=my_dir(slave_load_tmpdir,MYF(MY_WME))))
    return;

  /*
     When we are deleting temporary files, we should only remove
     the files associated with the server id of our server.
     We don't use event_server_id here because since we've disabled
     direct binlogging of Create_file/Append_file/Exec_load events
     we cannot meet Start_log event in the middle of events from one
     LOAD DATA.
  */
  p= strncpy(prefbuf, STRING_WITH_LEN("SQL_LOAD-")) + 9;
  p= int10_to_str(::server_id, p, 10);
  *(p++)= '-';
  *p= 0;

  for (i=0 ; i < (uint)dirp->number_off_files; i++)
  {
    file=dirp->dir_entry+i;
    if (is_prefix(file->name, prefbuf))
    {
      fn_format(fname,file->name,slave_load_tmpdir,"",MY_UNPACK_FILENAME);
      my_delete(fname, MYF(0));
    }
  }

  my_dirend(dirp);
}


/*
  write_str()
*/

static bool write_str(IO_CACHE *file, const char *str, uint32_t length)
{
  unsigned char tmp[1];
  tmp[0]= (unsigned char) length;
  return (my_b_safe_write(file, tmp, sizeof(tmp)) ||
	  my_b_safe_write(file, (unsigned char*) str, length));
}


/*
  read_str()
*/

static inline int read_str(const char **buf, const char *buf_end,
                           const char **str, uint8_t *len)
{
  if (*buf + ((uint) (unsigned char) **buf) >= buf_end)
    return 1;
  *len= (uint8_t) **buf;
  *str= (*buf)+1;
  (*buf)+= (uint) *len+1;
  return 0;
}


/**
  Transforms a string into "" or its expression in 0x... form.
*/

char *str_to_hex(char *to, const char *from, uint32_t len)
{
  if (len)
  {
    *to++= '0';
    *to++= 'x';
    to= octet2hex(to, from, len);
  }
  else
    to= strcpy(to, "\"\"")+2;
  return to;                               // pointer to end 0 of 'to'
}


/**
  Append a version of the 'from' string suitable for use in a query to
  the 'to' string.  To generate a correct escaping, the character set
  information in 'csinfo' is used.
*/

int
append_query_string(const CHARSET_INFO * const csinfo,
                    String const *from, String *to)
{
  char *beg, *ptr;
  uint32_t const orig_len= to->length();
  if (to->reserve(orig_len + from->length()*2+3))
    return 1;

  beg= to->c_ptr_quick() + to->length();
  ptr= beg;
  if (csinfo->escape_with_backslash_is_dangerous)
    ptr= str_to_hex(ptr, from->ptr(), from->length());
  else
  {
    *ptr++= '\'';
    ptr+= drizzle_escape_string(ptr, from->ptr(), from->length());
    *ptr++='\'';
  }
  to->length(orig_len + ptr - beg);
  return 0;
}


/**************************************************************************
	Log_event methods (= the parent class of all events)
**************************************************************************/

/**
  @return
  returns the human readable name of the event's type
*/

const char* Log_event::get_type_str(Log_event_type type)
{
  switch(type) {
  case START_EVENT_V3:  return "Start_v3";
  case STOP_EVENT:   return "Stop";
  case QUERY_EVENT:  return "Query";
  case ROTATE_EVENT: return "Rotate";
  case LOAD_EVENT:   return "Load";
  case NEW_LOAD_EVENT:   return "New_load";
  case SLAVE_EVENT:  return "Slave";
  case CREATE_FILE_EVENT: return "Create_file";
  case APPEND_BLOCK_EVENT: return "Append_block";
  case DELETE_FILE_EVENT: return "Delete_file";
  case EXEC_LOAD_EVENT: return "Exec_load";
  case XID_EVENT: return "Xid";
  case FORMAT_DESCRIPTION_EVENT: return "Format_desc";
  case TABLE_MAP_EVENT: return "Table_map";
  case WRITE_ROWS_EVENT: return "Write_rows";
  case UPDATE_ROWS_EVENT: return "Update_rows";
  case DELETE_ROWS_EVENT: return "Delete_rows";
  case BEGIN_LOAD_QUERY_EVENT: return "Begin_load_query";
  case EXECUTE_LOAD_QUERY_EVENT: return "Execute_load_query";
  case INCIDENT_EVENT: return "Incident";
  default: return "Unknown";				/* impossible */
  }
}

const char* Log_event::get_type_str()
{
  return get_type_str(get_type_code());
}


/*
  Log_event::Log_event()
*/

Log_event::Log_event(Session* session_arg, uint16_t flags_arg, bool using_trans)
  :log_pos(0), temp_buf(0), exec_time(0), flags(flags_arg), session(session_arg)
{
  server_id=	session->server_id;
  when=		session->start_time;
  cache_stmt=	using_trans;
}


/**
  This minimal constructor is for when you are not even sure that there
  is a valid Session. For example in the server when we are shutting down or
  flushing logs after receiving a SIGHUP (then we must write a Rotate to
  the binlog but we have no Session, so we need this minimal constructor).
*/

Log_event::Log_event()
  :temp_buf(0), exec_time(0), flags(0), cache_stmt(0),
   session(0)
{
  server_id=	::server_id;
  /*
    We can't call my_time() here as this would cause a call before
    my_init() is called
  */
  when=		0;
  log_pos=	0;
}


/*
  Log_event::Log_event()
*/

Log_event::Log_event(const char* buf,
                     const Format_description_log_event* description_event)
  :temp_buf(0), cache_stmt(0)
{
  session= 0;
  when= uint4korr(buf);
  server_id= uint4korr(buf + SERVER_ID_OFFSET);
  data_written= uint4korr(buf + EVENT_LEN_OFFSET);
  if (description_event->binlog_version==1)
  {
    log_pos= 0;
    flags= 0;
    return;
  }
  /* 4.0 or newer */
  log_pos= uint4korr(buf + LOG_POS_OFFSET);
  /*
    If the log is 4.0 (so here it can only be a 4.0 relay log read by
    the SQL thread or a 4.0 master binlog read by the I/O thread),
    log_pos is the beginning of the event: we transform it into the end
    of the event, which is more useful.
    But how do you know that the log is 4.0: you know it if
    description_event is version 3 *and* you are not reading a
    Format_desc (remember that mysqlbinlog starts by assuming that 5.0
    logs are in 4.0 format, until it finds a Format_desc).
  */
  if (description_event->binlog_version==3 &&
      buf[EVENT_TYPE_OFFSET]<FORMAT_DESCRIPTION_EVENT && log_pos)
  {
      /*
        If log_pos=0, don't change it. log_pos==0 is a marker to mean
        "don't change rli->group_master_log_pos" (see
        inc_group_relay_log_pos()). As it is unreal log_pos, adding the
        event len's is nonsense. For example, a fake Rotate event should
        not have its log_pos (which is 0) changed or it will modify
        Exec_master_log_pos in SHOW SLAVE STATUS, displaying a nonsense
        value of (a non-zero offset which does not exist in the master's
        binlog, so which will cause problems if the user uses this value
        in CHANGE MASTER).
      */
    log_pos+= data_written; /* purecov: inspected */
  }

  flags= uint2korr(buf + FLAGS_OFFSET);
  if ((buf[EVENT_TYPE_OFFSET] == FORMAT_DESCRIPTION_EVENT) ||
      (buf[EVENT_TYPE_OFFSET] == ROTATE_EVENT))
  {
    /*
      These events always have a header which stops here (i.e. their
      header is FROZEN).
    */
    /*
      Initialization to zero of all other Log_event members as they're
      not specified. Currently there are no such members; in the future
      there will be an event UID (but Format_description and Rotate
      don't need this UID, as they are not propagated through
      --log-slave-updates (remember the UID is used to not play a query
      twice when you have two masters which are slaves of a 3rd master).
      Then we are done.
    */
    return;
  }
  /* otherwise, go on with reading the header from buf (nothing now) */
}


int Log_event::do_update_pos(Relay_log_info *rli)
{
  /*
    rli is null when (as far as I (Guilhem) know) the caller is
    Load_log_event::do_apply_event *and* that one is called from
    Execute_load_log_event::do_apply_event.  In this case, we don't
    do anything here ; Execute_load_log_event::do_apply_event will
    call Log_event::do_apply_event again later with the proper rli.
    Strictly speaking, if we were sure that rli is null only in the
    case discussed above, 'if (rli)' is useless here.  But as we are
    not 100% sure, keep it for now.

    Matz: I don't think we will need this check with this refactoring.
  */
  if (rli)
  {
    /*
      bug#29309 simulation: resetting the flag to force
      wrong behaviour of artificial event to update
      rli->last_master_timestamp for only one time -
      the first FLUSH LOGS in the test.
    */
    if (debug_not_change_ts_if_art_event == 1
        && is_artificial_event())
      debug_not_change_ts_if_art_event= 0;
    rli->stmt_done(log_pos,
                   is_artificial_event() &&
                   debug_not_change_ts_if_art_event > 0 ? 0 : when);
    if (debug_not_change_ts_if_art_event == 0)
      debug_not_change_ts_if_art_event= 2;
  }
  return 0;                                   // Cannot fail currently
}


Log_event::enum_skip_reason
Log_event::do_shall_skip(Relay_log_info *rli)
{
  if ((server_id == ::server_id && !rli->replicate_same_server_id) || (rli->slave_skip_counter == 1 && rli->is_in_group()))
    return EVENT_SKIP_IGNORE;
  else if (rli->slave_skip_counter > 0)
    return EVENT_SKIP_COUNT;
  else
    return EVENT_SKIP_NOT;
}


/*
  Log_event::pack_info()
*/

void Log_event::pack_info(Protocol *protocol)
{
  protocol->store("", &my_charset_bin);
}


const char* Log_event::get_db()
{
  return session ? session->db : 0;
}


/**
  init_show_field_list() prepares the column names and types for the
  output of SHOW BINLOG EVENTS; it is used only by SHOW BINLOG
  EVENTS.
*/

void Log_event::init_show_field_list(List<Item>* field_list)
{
  field_list->push_back(new Item_empty_string("Log_name", 20));
  field_list->push_back(new Item_return_int("Pos", MY_INT32_NUM_DECIMAL_DIGITS,
                                            DRIZZLE_TYPE_LONGLONG));
  field_list->push_back(new Item_empty_string("Event_type", 20));
  field_list->push_back(new Item_return_int("Server_id", 10,
                                            DRIZZLE_TYPE_LONG));
  field_list->push_back(new Item_return_int("End_log_pos",
                                            MY_INT32_NUM_DECIMAL_DIGITS,
                                            DRIZZLE_TYPE_LONGLONG));
  field_list->push_back(new Item_empty_string("Info", 20));
}

/*
  Log_event::write()
*/

bool Log_event::write_header(IO_CACHE* file, ulong event_data_length)
{
  unsigned char header[LOG_EVENT_HEADER_LEN];
  ulong now;

  /* Store number of bytes that will be written by this event */
  data_written= event_data_length + sizeof(header);

  /*
    log_pos != 0 if this is relay-log event. In this case we should not
    change the position
  */

  if (is_artificial_event())
  {
    /*
      We should not do any cleanup on slave when reading this. We
      mark this by setting log_pos to 0.  Start_log_event_v3() will
      detect this on reading and set artificial_event=1 for the event.
    */
    log_pos= 0;
  }
  else  if (!log_pos)
  {
    /*
      Calculate position of end of event

      Note that with a SEQ_READ_APPEND cache, my_b_tell() does not
      work well.  So this will give slightly wrong positions for the
      Format_desc/Rotate/Stop events which the slave writes to its
      relay log. For example, the initial Format_desc will have
      end_log_pos=91 instead of 95. Because after writing the first 4
      bytes of the relay log, my_b_tell() still reports 0. Because
      my_b_append() does not update the counter which my_b_tell()
      later uses (one should probably use my_b_append_tell() to work
      around this).  To get right positions even when writing to the
      relay log, we use the (new) my_b_safe_tell().

      Note that this raises a question on the correctness of all these
      assert(my_b_tell()=rli->event_relay_log_pos).

      If in a transaction, the log_pos which we calculate below is not
      very good (because then my_b_safe_tell() returns start position
      of the BEGIN, so it's like the statement was at the BEGIN's
      place), but it's not a very serious problem (as the slave, when
      it is in a transaction, does not take those end_log_pos into
      account (as it calls inc_event_relay_log_pos()). To be fixed
      later, so that it looks less strange. But not bug.
    */

    log_pos= my_b_safe_tell(file)+data_written;
  }

  now= (ulong) get_time();                              // Query start time

  /*
    Header will be of size LOG_EVENT_HEADER_LEN for all events, except for
    FORMAT_DESCRIPTION_EVENT and ROTATE_EVENT, where it will be
    LOG_EVENT_MINIMAL_HEADER_LEN (remember these 2 have a frozen header,
    because we read them before knowing the format).
  */

  int4store(header, now);              // timestamp
  header[EVENT_TYPE_OFFSET]= get_type_code();
  int4store(header+ SERVER_ID_OFFSET, server_id);
  int4store(header+ EVENT_LEN_OFFSET, data_written);
  int4store(header+ LOG_POS_OFFSET, log_pos);
  int2store(header+ FLAGS_OFFSET, flags);

  return(my_b_safe_write(file, header, sizeof(header)) != 0);
}


time_t Log_event::get_time()
{
  Session *tmp_session;
  if (when)
    return when;
  if (session)
    return session->start_time;
  if ((tmp_session= current_session))
    return tmp_session->start_time;
  return my_time(0);
}


/**
  This needn't be format-tolerant, because we only read
  LOG_EVENT_MINIMAL_HEADER_LEN (we just want to read the event's length).
*/

int Log_event::read_log_event(IO_CACHE* file, String* packet,
                              pthread_mutex_t* log_lock)
{
  ulong data_len;
  int result=0;
  char buf[LOG_EVENT_MINIMAL_HEADER_LEN];

  if (log_lock)
    pthread_mutex_lock(log_lock);
  if (my_b_read(file, (unsigned char*) buf, sizeof(buf)))
  {
    /*
      If the read hits eof, we must report it as eof so the caller
      will know it can go into cond_wait to be woken up on the next
      update to the log.
    */
    if (!file->error)
      result= LOG_READ_EOF;
    else
      result= (file->error > 0 ? LOG_READ_TRUNC : LOG_READ_IO);
    goto end;
  }
  data_len= uint4korr(buf + EVENT_LEN_OFFSET);
  if (data_len < LOG_EVENT_MINIMAL_HEADER_LEN ||
      data_len > current_session->variables.max_allowed_packet)
  {
    result= ((data_len < LOG_EVENT_MINIMAL_HEADER_LEN) ? LOG_READ_BOGUS :
	     LOG_READ_TOO_LARGE);
    goto end;
  }

  /* Append the log event header to packet */
  if (packet->append(buf, sizeof(buf)))
  {
    /* Failed to allocate packet */
    result= LOG_READ_MEM;
    goto end;
  }
  data_len-= LOG_EVENT_MINIMAL_HEADER_LEN;
  if (data_len)
  {
    /* Append rest of event, read directly from file into packet */
    if (packet->append(file, data_len))
    {
      /*
        Fatal error occured when appending rest of the event
        to packet, possible failures:
	1. EOF occured when reading from file, it's really an error
           as data_len is >=0 there's supposed to be more bytes available.
           file->error will have been set to number of bytes left to read
        2. Read was interrupted, file->error would normally be set to -1
        3. Failed to allocate memory for packet, my_errno
           will be ENOMEM(file->error shuold be 0, but since the
           memory allocation occurs before the call to read it might
           be uninitialized)
      */
      result= (my_errno == ENOMEM ? LOG_READ_MEM :
               (file->error >= 0 ? LOG_READ_TRUNC: LOG_READ_IO));
      /* Implicit goto end; */
    }
  }

end:
  if (log_lock)
    pthread_mutex_unlock(log_lock);
  return(result);
}

#define UNLOCK_MUTEX if (log_lock) pthread_mutex_unlock(log_lock);
#define LOCK_MUTEX if (log_lock) pthread_mutex_lock(log_lock);

/**
  @note
    Allocates memory;  The caller is responsible for clean-up.
*/
Log_event* Log_event::read_log_event(IO_CACHE* file,
				     pthread_mutex_t* log_lock,
                                     const Format_description_log_event
                                     *description_event)
{
  assert(description_event != 0);
  char head[LOG_EVENT_MINIMAL_HEADER_LEN];
  /*
    First we only want to read at most LOG_EVENT_MINIMAL_HEADER_LEN, just to
    check the event for sanity and to know its length; no need to really parse
    it. We say "at most" because this could be a 3.23 master, which has header
    of 13 bytes, whereas LOG_EVENT_MINIMAL_HEADER_LEN is 19 bytes (it's
    "minimal" over the set {MySQL >=4.0}).
  */
  uint32_t header_size= cmin(description_event->common_header_len,
                        LOG_EVENT_MINIMAL_HEADER_LEN);

  LOCK_MUTEX;
  if (my_b_read(file, (unsigned char *) head, header_size))
  {
    UNLOCK_MUTEX;
    /*
      No error here; it could be that we are at the file's end. However
      if the next my_b_read() fails (below), it will be an error as we
      were able to read the first bytes.
    */
    return(0);
  }
  uint32_t data_len = uint4korr(head + EVENT_LEN_OFFSET);
  char *buf= 0;
  const char *error= 0;
  Log_event *res=  0;
#ifndef max_allowed_packet
  Session *session=current_session;
  uint32_t max_allowed_packet= session ? session->variables.max_allowed_packet : ~(ulong)0;
#endif

  if (data_len > max_allowed_packet)
  {
    error = "Event too big";
    goto err;
  }

  if (data_len < header_size)
  {
    error = "Event too small";
    goto err;
  }

  // some events use the extra byte to null-terminate strings
  if (!(buf = (char*) malloc(data_len+1)))
  {
    error = "Out of memory";
    goto err;
  }
  buf[data_len] = 0;
  memcpy(buf, head, header_size);
  if (my_b_read(file, (unsigned char*) buf + header_size, data_len - header_size))
  {
    error = "read error";
    goto err;
  }
  if ((res= read_log_event(buf, data_len, &error, description_event)))
    res->register_temp_buf(buf);

err:
  UNLOCK_MUTEX;
  if (!res)
  {
    assert(error != 0);
    sql_print_error(_("Error in Log_event::read_log_event(): "
                    "'%s', data_len: %d, event_type: %d"),
		    error,data_len,head[EVENT_TYPE_OFFSET]);
    free(buf);
    /*
      The SQL slave thread will check if file->error<0 to know
      if there was an I/O error. Even if there is no "low-level" I/O errors
      with 'file', any of the high-level above errors is worrying
      enough to stop the SQL thread now ; as we are skipping the current event,
      going on with reading and successfully executing other events can
      only corrupt the slave's databases. So stop.
    */
    file->error= -1;
  }
  return(res);
}


/**
  Binlog format tolerance is in (buf, event_len, description_event)
  constructors.
*/

Log_event* Log_event::read_log_event(const char* buf, uint32_t event_len,
				     const char **error,
                                     const Format_description_log_event *description_event)
{
  Log_event* ev;
  assert(description_event != 0);

  /* Check the integrity */
  if (event_len < EVENT_LEN_OFFSET ||
      buf[EVENT_TYPE_OFFSET] >= ENUM_END_EVENT ||
      (uint) event_len != uint4korr(buf+EVENT_LEN_OFFSET))
  {
    *error="Sanity check failed";		// Needed to free buffer
    return(NULL); // general sanity check - will fail on a partial read
  }

  uint32_t event_type= buf[EVENT_TYPE_OFFSET];
  if (event_type > description_event->number_of_event_types &&
      event_type != FORMAT_DESCRIPTION_EVENT)
  {
    /*
      It is unsafe to use the description_event if its post_header_len
      array does not include the event type.
    */
    ev= NULL;
  }
  else
  {
    /*
      In some previuos versions (see comment in
      Format_description_log_event::Format_description_log_event(char*,...)),
      event types were assigned different id numbers than in the
      present version. In order to replicate from such versions to the
      present version, we must map those event type id's to our event
      type id's.  The mapping is done with the event_type_permutation
      array, which was set up when the Format_description_log_event
      was read.
    */
    if (description_event->event_type_permutation)
      event_type= description_event->event_type_permutation[event_type];

    switch(event_type) {
    case QUERY_EVENT:
      ev  = new Query_log_event(buf, event_len, description_event, QUERY_EVENT);
      break;
    case LOAD_EVENT:
      ev = new Load_log_event(buf, event_len, description_event);
      break;
    case NEW_LOAD_EVENT:
      ev = new Load_log_event(buf, event_len, description_event);
      break;
    case ROTATE_EVENT:
      ev = new Rotate_log_event(buf, event_len, description_event);
      break;
    case CREATE_FILE_EVENT:
      ev = new Create_file_log_event(buf, event_len, description_event);
      break;
    case APPEND_BLOCK_EVENT:
      ev = new Append_block_log_event(buf, event_len, description_event);
      break;
    case DELETE_FILE_EVENT:
      ev = new Delete_file_log_event(buf, event_len, description_event);
      break;
    case EXEC_LOAD_EVENT:
      ev = new Execute_load_log_event(buf, event_len, description_event);
      break;
    case START_EVENT_V3: /* this is sent only by MySQL <=4.x */
      ev = new Start_log_event_v3(buf, description_event);
      break;
    case STOP_EVENT:
      ev = new Stop_log_event(buf, description_event);
      break;
    case XID_EVENT:
      ev = new Xid_log_event(buf, description_event);
      break;
    case FORMAT_DESCRIPTION_EVENT:
      ev = new Format_description_log_event(buf, event_len, description_event);
      break;
    case WRITE_ROWS_EVENT:
      ev = new Write_rows_log_event(buf, event_len, description_event);
      break;
    case UPDATE_ROWS_EVENT:
      ev = new Update_rows_log_event(buf, event_len, description_event);
      break;
    case DELETE_ROWS_EVENT:
      ev = new Delete_rows_log_event(buf, event_len, description_event);
      break;
    case TABLE_MAP_EVENT:
      ev = new Table_map_log_event(buf, event_len, description_event);
      break;
    case BEGIN_LOAD_QUERY_EVENT:
      ev = new Begin_load_query_log_event(buf, event_len, description_event);
      break;
    case EXECUTE_LOAD_QUERY_EVENT:
      ev= new Execute_load_query_log_event(buf, event_len, description_event);
      break;
    case INCIDENT_EVENT:
      ev = new Incident_log_event(buf, event_len, description_event);
      break;
    default:
      ev= NULL;
      break;
    }
  }

  /*
    is_valid() are small event-specific sanity tests which are
    important; for example there are some malloc() in constructors
    (e.g. Query_log_event::Query_log_event(char*...)); when these
    malloc() fail we can't return an error out of the constructor
    (because constructor is "void") ; so instead we leave the pointer we
    wanted to allocate (e.g. 'query') to 0 and we test it in is_valid().
    Same for Format_description_log_event, member 'post_header_len'.
  */
  if (!ev || !ev->is_valid())
  {
    delete ev;
    *error= "Found invalid event in binary log";
    return(0);
  }
  return(ev);
}

inline Log_event::enum_skip_reason
Log_event::continue_group(Relay_log_info *rli)
{
  if (rli->slave_skip_counter == 1)
    return Log_event::EVENT_SKIP_IGNORE;
  return Log_event::do_shall_skip(rli);
}

/**************************************************************************
	Query_log_event methods
**************************************************************************/

/**
  This (which is used only for SHOW BINLOG EVENTS) could be updated to
  print SET @@session_var=. But this is not urgent, as SHOW BINLOG EVENTS is
  only an information, it does not produce suitable queries to replay (for
  example it does not print LOAD DATA INFILE).
  @todo
    show the catalog ??
*/

void Query_log_event::pack_info(Protocol *protocol)
{
  // TODO: show the catalog ??
  char *buf, *pos;
  if (!(buf= (char*) malloc(9 + db_len + q_len)))
    return;
  pos= buf;
  if (!(flags & LOG_EVENT_SUPPRESS_USE_F)
      && db && db_len)
  {
    pos= strcpy(buf, "use `")+5;
    memcpy(pos, db, db_len);
    pos= strcpy(pos+db_len, "`; ")+3;
  }
  if (query && q_len)
  {
    memcpy(pos, query, q_len);
    pos+= q_len;
  }
  protocol->store(buf, pos-buf, &my_charset_bin);
  free(buf);
}


/**
  Query_log_event::write().

  @note
    In this event we have to modify the header to have the correct
    EVENT_LEN_OFFSET as we don't yet know how many status variables we
    will print!
*/

bool Query_log_event::write(IO_CACHE* file)
{
  /**
    @todo if catalog can be of length FN_REFLEN==512, then we are not
    replicating it correctly, since the length is stored in a byte
    /sven
  */
  unsigned char buf[QUERY_HEADER_LEN+
            1+4+           // code of flags2 and flags2
            1+8+           // code of sql_mode and sql_mode
            1+1+FN_REFLEN+ // code of catalog and catalog length and catalog
            1+4+           // code of autoinc and the 2 autoinc variables
            1+6+           // code of charset and charset
            1+1+MAX_TIME_ZONE_NAME_LENGTH+ // code of tz and tz length and tz name
            1+2+           // code of lc_time_names and lc_time_names_number
            1+2            // code of charset_database and charset_database_number
            ], *start, *start_of_status;
  ulong event_length;

  if (!query)
    return 1;                                   // Something wrong with event

  /*
    We want to store the thread id:
    (- as an information for the user when he reads the binlog)
    - if the query uses temporary table: for the slave SQL thread to know to
    which master connection the temp table belongs.
    Now imagine we (write()) are called by the slave SQL thread (we are
    logging a query executed by this thread; the slave runs with
    --log-slave-updates). Then this query will be logged with
    thread_id=the_thread_id_of_the_SQL_thread. Imagine that 2 temp tables of
    the same name were created simultaneously on the master (in the master
    binlog you have
    CREATE TEMPORARY TABLE t; (thread 1)
    CREATE TEMPORARY TABLE t; (thread 2)
    ...)
    then in the slave's binlog there will be
    CREATE TEMPORARY TABLE t; (thread_id_of_the_slave_SQL_thread)
    CREATE TEMPORARY TABLE t; (thread_id_of_the_slave_SQL_thread)
    which is bad (same thread id!).

    To avoid this, we log the thread's thread id EXCEPT for the SQL
    slave thread for which we log the original (master's) thread id.
    Now this moves the bug: what happens if the thread id on the
    master was 10 and when the slave replicates the query, a
    connection number 10 is opened by a normal client on the slave,
    and updates a temp table of the same name? We get a problem
    again. To avoid this, in the handling of temp tables (sql_base.cc)
    we use thread_id AND server_id.  TODO when this is merged into
    4.1: in 4.1, slave_proxy_id has been renamed to pseudo_thread_id
    and is a session variable: that's to make mysqlbinlog work with
    temp tables. We probably need to introduce

    SET PSEUDO_SERVER_ID
    for mysqlbinlog in 4.1. mysqlbinlog would print:
    SET PSEUDO_SERVER_ID=
    SET PSEUDO_THREAD_ID=
    for each query using temp tables.
  */
  int4store(buf + Q_THREAD_ID_OFFSET, slave_proxy_id);
  int4store(buf + Q_EXEC_TIME_OFFSET, exec_time);
  buf[Q_DB_LEN_OFFSET] = (char) db_len;
  int2store(buf + Q_ERR_CODE_OFFSET, error_code);

  /*
    You MUST always write status vars in increasing order of code. This
    guarantees that a slightly older slave will be able to parse those he
    knows.
  */
  start_of_status= start= buf+QUERY_HEADER_LEN;
  if (flags2_inited)
  {
    *start++= Q_FLAGS2_CODE;
    int4store(start, flags2);
    start+= 4;
  }
  if (lc_time_names_number)
  {
    assert(lc_time_names_number <= 0xFFFF);
    *start++= Q_LC_TIME_NAMES_CODE;
    int2store(start, lc_time_names_number);
    start+= 2;
  }
  if (charset_database_number)
  {
    assert(charset_database_number <= 0xFFFF);
    *start++= Q_CHARSET_DATABASE_CODE;
    int2store(start, charset_database_number);
    start+= 2;
  }
  /*
    Here there could be code like
    if (command-line-option-which-says-"log_this_variable" && inited)
    {
    *start++= Q_THIS_VARIABLE_CODE;
    int4store(start, this_variable);
    start+= 4;
    }
  */

  /* Store length of status variables */
  status_vars_len= (uint) (start-start_of_status);
  assert(status_vars_len <= MAX_SIZE_LOG_EVENT_STATUS);
  int2store(buf + Q_STATUS_VARS_LEN_OFFSET, status_vars_len);

  /*
    Calculate length of whole event
    The "1" below is the \0 in the db's length
  */
  event_length= (uint) (start-buf) + get_post_header_size_for_derived() + db_len + 1 + q_len;

  return (write_header(file, event_length) ||
          my_b_safe_write(file, (unsigned char*) buf, QUERY_HEADER_LEN) ||
          write_post_header_for_derived(file) ||
          my_b_safe_write(file, (unsigned char*) start_of_status,
                          (uint) (start-start_of_status)) ||
          my_b_safe_write(file, (db) ? (unsigned char*) db : (unsigned char*)"", db_len + 1) ||
          my_b_safe_write(file, (unsigned char*) query, q_len)) ? 1 : 0;
}

/**
  The simplest constructor that could possibly work.  This is used for
  creating static objects that have a special meaning and are invisible
  to the log.
*/
Query_log_event::Query_log_event()
  :Log_event(), data_buf(0)
{
}


/*
  SYNOPSIS
    Query_log_event::Query_log_event()
      session_arg           - thread handle
      query_arg         - array of char representing the query
      query_length      - size of the  `query_arg' array
      using_trans       - there is a modified transactional table
      suppress_use      - suppress the generation of 'USE' statements
      killed_status_arg - an optional with default to Session::KILLED_NO_VALUE
                          if the value is different from the default, the arg
                          is set to the current session->killed value.
                          A caller might need to masquerade session->killed with
                          Session::NOT_KILLED.
  DESCRIPTION
  Creates an event for binlogging
  The value for local `killed_status' can be supplied by caller.
*/
Query_log_event::Query_log_event(Session* session_arg, const char* query_arg,
                                 ulong query_length, bool using_trans,
                                 bool suppress_use,
                                 Session::killed_state killed_status_arg)
:Log_event(session_arg,
           (session_arg->thread_specific_used ? LOG_EVENT_THREAD_SPECIFIC_F : 0) |
           (suppress_use ? LOG_EVENT_SUPPRESS_USE_F : 0),
           using_trans),
  data_buf(0), query(query_arg), catalog(session_arg->catalog),
  db(session_arg->db), q_len((uint32_t) query_length),
  thread_id(session_arg->thread_id),
  /* save the original thread id; we already know the server id */
  slave_proxy_id(session_arg->variables.pseudo_thread_id),
  flags2_inited(1), sql_mode_inited(1), charset_inited(1),
  sql_mode(0),
  auto_increment_increment(session_arg->variables.auto_increment_increment),
  auto_increment_offset(session_arg->variables.auto_increment_offset),
  lc_time_names_number(session_arg->variables.lc_time_names->number),
   charset_database_number(0)
{
  time_t end_time;

  if (killed_status_arg == Session::KILLED_NO_VALUE)
    killed_status_arg= session_arg->killed;

  error_code=
    (killed_status_arg == Session::NOT_KILLED) ?
    (session_arg->is_error() ? session_arg->main_da.sql_errno() : 0) :
    (session_arg->killed_errno());

  time(&end_time);
  exec_time = (ulong) (end_time  - session_arg->start_time);
  /**
    @todo this means that if we have no catalog, then it is replicated
    as an existing catalog of length zero. is that safe? /sven
  */
  catalog_len = (catalog) ? (uint32_t) strlen(catalog) : 0;
  /* status_vars_len is set just before writing the event */
  db_len = (db) ? (uint32_t) strlen(db) : 0;
  if (session_arg->variables.collation_database != session_arg->db_charset)
    charset_database_number= session_arg->variables.collation_database->number;

  /*
    If we don't use flags2 for anything else than options contained in
    session_arg->options, it would be more efficient to flags2=session_arg->options
    (OPTIONS_WRITTEN_TO_BIN_LOG would be used only at reading time).
    But it's likely that we don't want to use 32 bits for 3 bits; in the future
    we will probably want to reclaim the 29 bits. So we need the &.
  */
  flags2= (uint32_t) (session_arg->options & OPTIONS_WRITTEN_TO_BIN_LOG);
  assert(session_arg->variables.character_set_client->number < 256*256);
  assert(session_arg->variables.collation_connection->number < 256*256);
  assert(session_arg->variables.collation_server->number < 256*256);
  assert(session_arg->variables.character_set_client->mbminlen == 1);
  int2store(charset, session_arg->variables.character_set_client->number);
  int2store(charset+2, session_arg->variables.collation_connection->number);
  int2store(charset+4, session_arg->variables.collation_server->number);
  time_zone_len= 0;
}

static void copy_str_and_move(const char **src,
                              Log_event::Byte **dst,
                              uint32_t len)
{
  memcpy(*dst, *src, len);
  *src= (const char *)*dst;
  (*dst)+= len;
  *(*dst)++= 0;
}


/**
   Macro to check that there is enough space to read from memory.

   @param PTR Pointer to memory
   @param END End of memory
   @param CNT Number of bytes that should be read.
 */
#define CHECK_SPACE(PTR,END,CNT)                      \
  do {                                                \
    assert((PTR) + (CNT) <= (END));                   \
    if ((PTR) + (CNT) > (END)) {                      \
      query= 0;                                       \
      return;                                         \
    }                                                 \
  } while (0)


/**
  This is used by the SQL slave thread to prepare the event before execution.
*/
Query_log_event::Query_log_event(const char* buf, uint32_t event_len,
                                 const Format_description_log_event
                                 *description_event,
                                 Log_event_type event_type)
  :Log_event(buf, description_event), data_buf(0), query(NULL),
   db(NULL), catalog_len(0), status_vars_len(0),
   flags2_inited(0), sql_mode_inited(0), charset_inited(0),
   auto_increment_increment(1), auto_increment_offset(1),
   time_zone_len(0), lc_time_names_number(0), charset_database_number(0)
{
  uint32_t data_len;
  uint32_t tmp;
  uint8_t common_header_len, post_header_len;
  Log_event::Byte *start;
  const Log_event::Byte *end;
  bool catalog_nz= 1;

  common_header_len= description_event->common_header_len;
  post_header_len= description_event->post_header_len[event_type-1];

  /*
    We test if the event's length is sensible, and if so we compute data_len.
    We cannot rely on QUERY_HEADER_LEN here as it would not be format-tolerant.
    We use QUERY_HEADER_MINIMAL_LEN which is the same for 3.23, 4.0 & 5.0.
  */
  if (event_len < (uint)(common_header_len + post_header_len))
    return;
  data_len = event_len - (common_header_len + post_header_len);
  buf+= common_header_len;

  slave_proxy_id= thread_id = uint4korr(buf + Q_THREAD_ID_OFFSET);
  exec_time = uint4korr(buf + Q_EXEC_TIME_OFFSET);
  db_len = (uint)buf[Q_DB_LEN_OFFSET]; // TODO: add a check of all *_len vars
  error_code = uint2korr(buf + Q_ERR_CODE_OFFSET);

  /*
    5.0 format starts here.
    Depending on the format, we may or not have affected/warnings etc
    The remnent post-header to be parsed has length:
  */
  tmp= post_header_len - QUERY_HEADER_MINIMAL_LEN;
  if (tmp)
  {
    status_vars_len= uint2korr(buf + Q_STATUS_VARS_LEN_OFFSET);
    /*
      Check if status variable length is corrupt and will lead to very
      wrong data. We could be even more strict and require data_len to
      be even bigger, but this will suffice to catch most corruption
      errors that can lead to a crash.
    */
    if (status_vars_len > cmin(data_len, (uint32_t)MAX_SIZE_LOG_EVENT_STATUS))
    {
      query= 0;
      return;
    }
    data_len-= status_vars_len;
    tmp-= 2;
  }
  /*
    We have parsed everything we know in the post header for QUERY_EVENT,
    the rest of post header is either comes from older version MySQL or
    dedicated to derived events (e.g. Execute_load_query...)
  */

  /* variable-part: the status vars; only in MySQL 5.0  */

  start= (Log_event::Byte*) (buf+post_header_len);
  end= (const Log_event::Byte*) (start+status_vars_len);
  for (const Log_event::Byte* pos= start; pos < end;)
  {
    switch (*pos++) {
    case Q_FLAGS2_CODE:
      CHECK_SPACE(pos, end, 4);
      flags2_inited= 1;
      flags2= uint4korr(pos);
      pos+= 4;
      break;
    case Q_LC_TIME_NAMES_CODE:
      CHECK_SPACE(pos, end, 2);
      lc_time_names_number= uint2korr(pos);
      pos+= 2;
      break;
    case Q_CHARSET_DATABASE_CODE:
      CHECK_SPACE(pos, end, 2);
      charset_database_number= uint2korr(pos);
      pos+= 2;
      break;
    default:
      /* That's why you must write status vars in growing order of code */
      pos= (const unsigned char*) end;                         // Break loop
    }
  }

  if (!(start= data_buf = (Log_event::Byte*) malloc(catalog_len + 1 +
                                             time_zone_len + 1 +
                                             data_len + 1)))
      return;
  if (catalog_len)                                  // If catalog is given
  {
    /**
      @todo we should clean up and do only copy_str_and_move; it
      works for both cases.  Then we can remove the catalog_nz
      flag. /sven
    */
    if (likely(catalog_nz)) // true except if event comes from 5.0.0|1|2|3.
      copy_str_and_move(&catalog, &start, catalog_len);
    else
    {
      memcpy(start, catalog, catalog_len+1); // copy end 0
      catalog= (const char *)start;
      start+= catalog_len+1;
    }
  }
  if (time_zone_len)
    copy_str_and_move(&time_zone_str, &start, time_zone_len);

  /**
    if time_zone_len or catalog_len are 0, then time_zone and catalog
    are uninitialized at this point.  shouldn't they point to the
    zero-length null-terminated strings we allocated space for in the
    my_alloc call above? /sven
  */

  /* A 2nd variable part; this is common to all versions */
  memcpy(start, end, data_len);          // Copy db and query
  start[data_len]= '\0';              // End query with \0 (For safetly)
  db= (char *)start;
  query= (char *)(start + db_len + 1);
  q_len= data_len - db_len -1;
  return;
}


/*
  Query_log_event::do_apply_event()
*/
int Query_log_event::do_apply_event(Relay_log_info const *rli)
{
  return do_apply_event(rli, query, q_len);
}


/**
  @todo
  Compare the values of "affected rows" around here. Something
  like:
  @code
     if ((uint32_t) affected_in_event != (uint32_t) affected_on_slave)
     {
     sql_print_error("Slave: did not get the expected number of affected \
     rows running query from master - expected %d, got %d (this numbers \
     should have matched modulo 4294967296).", 0, ...);
     session->query_error = 1;
     }
  @endcode
  We may also want an option to tell the slave to ignore "affected"
  mismatch. This mismatch could be implemented with a new ER_ code, and
  to ignore it you would use --slave-skip-errors...
*/
int Query_log_event::do_apply_event(Relay_log_info const *rli,
                                      const char *query_arg, uint32_t q_len_arg)
{
  int expected_error,actual_error= 0;
  Query_id &query_id= Query_id::get_query_id();
  /*
    Colleagues: please never free(session->catalog) in MySQL. This would
    lead to bugs as here session->catalog is a part of an alloced block,
    not an entire alloced block (see
    Query_log_event::do_apply_event()). Same for session->db.  Thank
    you.
  */
  session->catalog= catalog_len ? (char *) catalog : (char *)"";
  session->set_db(db, strlen(db));       /* allocates a copy of 'db' */
  session->variables.auto_increment_increment= auto_increment_increment;
  session->variables.auto_increment_offset=    auto_increment_offset;

  /*
    InnoDB internally stores the master log position it has executed so far,
    i.e. the position just after the COMMIT event.
    When InnoDB will want to store, the positions in rli won't have
    been updated yet, so group_master_log_* will point to old BEGIN
    and event_master_log* will point to the beginning of current COMMIT.
    But log_pos of the COMMIT Query event is what we want, i.e. the pos of the
    END of the current log event (COMMIT). We save it in rli so that InnoDB can
    access it.
  */
  const_cast<Relay_log_info*>(rli)->future_group_master_log_pos= log_pos;

  clear_all_errors(session, const_cast<Relay_log_info*>(rli));
  const_cast<Relay_log_info*>(rli)->clear_tables_to_lock();

  /*
    Note:   We do not need to execute reset_one_shot_variables() if this
            db_ok() test fails.
    Reason: The db stored in binlog events is the same for SET and for
            its companion query.  If the SET is ignored because of
            db_ok(), the companion query will also be ignored, and if
            the companion query is ignored in the db_ok() test of
            ::do_apply_event(), then the companion SET also have so
            we don't need to reset_one_shot_variables().
  */
  if (1)
  {
    session->set_time((time_t)when);
    session->query_length= q_len_arg;
    session->query= (char*)query_arg;
    session->query_id= query_id.next();
    session->variables.pseudo_thread_id= thread_id;		// for temp tables

    if (ignored_error_code((expected_error= error_code)) ||
	!check_expected_error(session,rli,expected_error))
    {
      if (flags2_inited)
        /*
          all bits of session->options which are 1 in OPTIONS_WRITTEN_TO_BIN_LOG
          must take their value from flags2.
        */
        session->options= flags2|(session->options & ~OPTIONS_WRITTEN_TO_BIN_LOG);
      if (time_zone_len)
      {
        String tmp(time_zone_str, time_zone_len, &my_charset_bin);
        if (!(session->variables.time_zone= my_tz_find(session, &tmp)))
        {
          my_error(ER_UNKNOWN_TIME_ZONE, MYF(0), tmp.c_ptr());
          session->variables.time_zone= global_system_variables.time_zone;
          goto compare_errors;
        }
      }
      if (lc_time_names_number)
      {
        if (!(session->variables.lc_time_names=
              my_locale_by_number(lc_time_names_number)))
        {
          my_printf_error(ER_UNKNOWN_ERROR,
                      "Unknown locale: '%d'", MYF(0), lc_time_names_number);
          session->variables.lc_time_names= &my_locale_en_US;
          goto compare_errors;
        }
      }
      else
        session->variables.lc_time_names= &my_locale_en_US;
      if (charset_database_number)
      {
        const CHARSET_INFO *cs;
        if (!(cs= get_charset(charset_database_number, MYF(0))))
        {
          char buf[20];
          int10_to_str((int) charset_database_number, buf, -10);
          my_error(ER_UNKNOWN_COLLATION, MYF(0), buf);
          goto compare_errors;
        }
        session->variables.collation_database= cs;
      }
      else
        session->variables.collation_database= session->db_charset;

      /* Execute the query (note that we bypass dispatch_command()) */
      const char* found_semicolon= NULL;
      mysql_parse(session, session->query, session->query_length, &found_semicolon);
      log_slow_statement(session);
    }
    else
    {
      /*
        The query got a really bad error on the master (thread killed etc),
        which could be inconsistent. Parse it to test the table names: if the
        replicate-*-do|ignore-table rules say "this query must be ignored" then
        we exit gracefully; otherwise we warn about the bad error and tell DBA
        to check/fix it.
      */
      if (mysql_test_parse_for_slave(session, session->query, session->query_length))
        clear_all_errors(session, const_cast<Relay_log_info*>(rli)); /* Can ignore query */
      else
      {
        rli->report(ERROR_LEVEL, expected_error,
                    _("Query partially completed on the master "
                      "(error on master: %d) and was aborted. There is a "
                      "chance that your master is inconsistent at this "
                      "point. If you are sure that your master is ok, run "
                      "this query manually on the slave and then restart the "
                      "slave with SET GLOBAL SQL_SLAVE_SKIP_COUNTER=1; "
                      "START SLAVE; . Query: '%s'"),
                    expected_error, session->query);
        session->is_slave_error= 1;
      }
      goto end;
    }

compare_errors:

     /*
      If we expected a non-zero error code, and we don't get the same error
      code, and none of them should be ignored.
    */
    actual_error= session->is_error() ? session->main_da.sql_errno() : 0;
    if ((expected_error != actual_error) &&
        expected_error &&
        !ignored_error_code(actual_error) &&
        !ignored_error_code(expected_error))
    {
      rli->report(ERROR_LEVEL, 0,
                  _("Query caused differenxt errors on master and slave.\n"
                    "Error on master: '%s' (%d), Error on slave: '%s' (%d).\n"
                    "Default database: '%s'. Query: '%s'"),
                  ER(expected_error),
                  expected_error,
                  actual_error ? session->main_da.message() : _("no error"),
                  actual_error,
                  print_slave_db_safe(db), query_arg);
      session->is_slave_error= 1;
    }
    /*
      If we get the same error code as expected, or they should be ignored.
    */
    else if (expected_error == actual_error ||
 	     ignored_error_code(actual_error))
    {
      clear_all_errors(session, const_cast<Relay_log_info*>(rli));
      session->killed= Session::NOT_KILLED;
    }
    /*
      Other cases: mostly we expected no error and get one.
    */
    else if (session->is_slave_error || session->is_fatal_error)
    {
      rli->report(ERROR_LEVEL, actual_error,
                  _("Error '%s' on query. Default database: '%s'. Query: '%s'"),
                  (actual_error ? session->main_da.message() :
                   _("unexpected success or fatal error")),
                      print_slave_db_safe(session->db), query_arg);
      session->is_slave_error= 1;
    }

    /*
      TODO: compare the values of "affected rows" around here. Something
      like:
      if ((uint32_t) affected_in_event != (uint32_t) affected_on_slave)
      {
      sql_print_error("Slave: did not get the expected number of affected \
      rows running query from master - expected %d, got %d (this numbers \
      should have matched modulo 4294967296).", 0, ...);
      session->is_slave_error = 1;
      }
      We may also want an option to tell the slave to ignore "affected"
      mismatch. This mismatch could be implemented with a new ER_ code, and
      to ignore it you would use --slave-skip-errors...

      To do the comparison we need to know the value of "affected" which the
      above mysql_parse() computed. And we need to know the value of
      "affected" in the master's binlog. Both will be implemented later. The
      important thing is that we now have the format ready to log the values
      of "affected" in the binlog. So we can release 5.0.0 before effectively
      logging "affected" and effectively comparing it.
    */
  } /* End of if (db_ok(... */

end:
  pthread_mutex_lock(&LOCK_thread_count);
  /*
    Probably we have set session->query, session->db, session->catalog to point to places
    in the data_buf of this event. Now the event is going to be deleted
    probably, so data_buf will be freed, so the session->... listed above will be
    pointers to freed memory.
    So we must set them to 0, so that those bad pointers values are not later
    used. Note that "cleanup" queries like automatic DROP TEMPORARY Table
    don't suffer from these assignments to 0 as DROP TEMPORARY
    Table uses the db.table syntax.
  */
  session->catalog= 0;
  session->set_db(NULL, 0);                 /* will free the current database */
  session->query= 0;			// just to be sure
  session->query_length= 0;
  pthread_mutex_unlock(&LOCK_thread_count);
  close_thread_tables(session);
  session->first_successful_insert_id_in_prev_stmt= 0;
  free_root(session->mem_root,MYF(MY_KEEP_PREALLOC));
  return session->is_slave_error;
}

int Query_log_event::do_update_pos(Relay_log_info *rli)
{
  return Log_event::do_update_pos(rli);
}


Log_event::enum_skip_reason
Query_log_event::do_shall_skip(Relay_log_info *rli)
{
  assert(query && q_len > 0);

  if (rli->slave_skip_counter > 0)
  {
    if (strcmp("BEGIN", query) == 0)
    {
      session->options|= OPTION_BEGIN;
      return(Log_event::continue_group(rli));
    }

    if (strcmp("COMMIT", query) == 0 || strcmp("ROLLBACK", query) == 0)
    {
      session->options&= ~OPTION_BEGIN;
      return(Log_event::EVENT_SKIP_COUNT);
    }
  }
  return(Log_event::do_shall_skip(rli));
}


/**************************************************************************
	Start_log_event_v3 methods
**************************************************************************/

Start_log_event_v3::Start_log_event_v3()
  :Log_event(), created(0), binlog_version(BINLOG_VERSION),
   artificial_event(0), dont_set_created(0)
{
  memcpy(server_version, ::server_version, ST_SERVER_VER_LEN);
}

/*
  Start_log_event_v3::pack_info()
*/

void Start_log_event_v3::pack_info(Protocol *protocol)
{
  char buf[12 + ST_SERVER_VER_LEN + 14 + 22], *pos;
  pos= strcpy(buf, "Server ver: ")+12;
  pos= strcpy(pos, server_version)+strlen(server_version);
  pos= strcpy(pos, ", Binlog ver: ")+14;
  pos= int10_to_str(binlog_version, pos, 10);
  protocol->store(buf, (uint) (pos-buf), &my_charset_bin);
}


/*
  Start_log_event_v3::Start_log_event_v3()
*/

Start_log_event_v3::Start_log_event_v3(const char* buf,
                                       const Format_description_log_event
                                       *description_event)
  :Log_event(buf, description_event)
{
  buf+= description_event->common_header_len;
  binlog_version= uint2korr(buf+ST_BINLOG_VER_OFFSET);
  memcpy(server_version, buf+ST_SERVER_VER_OFFSET,
	 ST_SERVER_VER_LEN);
  // prevent overrun if log is corrupted on disk
  server_version[ST_SERVER_VER_LEN-1]= 0;
  created= uint4korr(buf+ST_CREATED_OFFSET);
  /* We use log_pos to mark if this was an artificial event or not */
  artificial_event= (log_pos == 0);
  dont_set_created= 1;
}


/*
  Start_log_event_v3::write()
*/

bool Start_log_event_v3::write(IO_CACHE* file)
{
  char buff[START_V3_HEADER_LEN];
  int2store(buff + ST_BINLOG_VER_OFFSET,binlog_version);
  memcpy(buff + ST_SERVER_VER_OFFSET,server_version,ST_SERVER_VER_LEN);
  if (!dont_set_created)
    created= when= get_time();
  int4store(buff + ST_CREATED_OFFSET,created);
  return (write_header(file, sizeof(buff)) ||
          my_b_safe_write(file, (unsigned char*) buff, sizeof(buff)));
}


/**
  Start_log_event_v3::do_apply_event() .
  The master started

    IMPLEMENTATION
    - To handle the case where the master died without having time to write
    DROP TEMPORARY Table, DO RELEASE_LOCK (prepared statements' deletion is
    TODO), we clean up all temporary tables that we got, if we are sure we
    can (see below).

  @todo
    - Remove all active user locks.
    Guilhem 2003-06: this is true but not urgent: the worst it can cause is
    the use of a bit of memory for a user lock which will not be used
    anymore. If the user lock is later used, the old one will be released. In
    other words, no deadlock problem.
*/

int Start_log_event_v3::do_apply_event(Relay_log_info const *rli)
{
  switch (binlog_version)
  {
  case 3:
  case 4:
    /*
      This can either be 4.x (then a Start_log_event_v3 is only at master
      startup so we are sure the master has restarted and cleared his temp
      tables; the event always has 'created'>0) or 5.0 (then we have to test
      'created').
    */
    if (created)
    {
      close_temporary_tables(session);
      cleanup_load_tmpdir();
    }
    break;

    /*
       Now the older formats; in that case load_tmpdir is cleaned up by the I/O
       thread.
    */
  case 1:
    if (strncmp(rli->relay_log.description_event_for_exec->server_version,
                "3.23.57",7) >= 0 && created)
    {
      /*
        Can distinguish, based on the value of 'created': this event was
        generated at master startup.
      */
      close_temporary_tables(session);
    }
    /*
      Otherwise, can't distinguish a Start_log_event generated at
      master startup and one generated by master FLUSH LOGS, so cannot
      be sure temp tables have to be dropped. So do nothing.
    */
    break;
  default:
    /* this case is impossible */
    return(1);
  }
  return(0);
}

/***************************************************************************
       Format_description_log_event methods
****************************************************************************/

/**
  Format_description_log_event 1st ctor.

    Ctor. Can be used to create the event to write to the binary log (when the
    server starts or when FLUSH LOGS), or to create artificial events to parse
    binlogs from MySQL 3.23 or 4.x.
    When in a client, only the 2nd use is possible.

  @param binlog_version         the binlog version for which we want to build
                                an event. Can be 1 (=MySQL 3.23), 3 (=4.0.x
                                x>=2 and 4.1) or 4 (MySQL 5.0). Note that the
                                old 4.0 (binlog version 2) is not supported;
                                it should not be used for replication with
                                5.0.
*/

Format_description_log_event::
Format_description_log_event(uint8_t binlog_ver, const char*)
  :Start_log_event_v3(), event_type_permutation(0)
{
  binlog_version= binlog_ver;
  switch (binlog_ver) {
  case 4: /* MySQL 5.0 */
    memcpy(server_version, ::server_version, ST_SERVER_VER_LEN);
    common_header_len= LOG_EVENT_HEADER_LEN;
    number_of_event_types= LOG_EVENT_TYPES;
    /* we'll catch malloc() error in is_valid() */
    post_header_len=(uint8_t*) malloc(number_of_event_types*sizeof(uint8_t));
    memset(post_header_len, 0, number_of_event_types*sizeof(uint8_t));
    /*
      This long list of assignments is not beautiful, but I see no way to
      make it nicer, as the right members are #defines, not array members, so
      it's impossible to write a loop.
    */
    if (post_header_len)
    {
      post_header_len[START_EVENT_V3-1]= START_V3_HEADER_LEN;
      post_header_len[QUERY_EVENT-1]= QUERY_HEADER_LEN;
      post_header_len[ROTATE_EVENT-1]= ROTATE_HEADER_LEN;
      post_header_len[LOAD_EVENT-1]= LOAD_HEADER_LEN;
      post_header_len[CREATE_FILE_EVENT-1]= CREATE_FILE_HEADER_LEN;
      post_header_len[APPEND_BLOCK_EVENT-1]= APPEND_BLOCK_HEADER_LEN;
      post_header_len[EXEC_LOAD_EVENT-1]= EXEC_LOAD_HEADER_LEN;
      post_header_len[DELETE_FILE_EVENT-1]= DELETE_FILE_HEADER_LEN;
      post_header_len[NEW_LOAD_EVENT-1]= post_header_len[LOAD_EVENT-1];
      post_header_len[FORMAT_DESCRIPTION_EVENT-1]= FORMAT_DESCRIPTION_HEADER_LEN;
      post_header_len[TABLE_MAP_EVENT-1]=    TABLE_MAP_HEADER_LEN;
      post_header_len[WRITE_ROWS_EVENT-1]=   ROWS_HEADER_LEN;
      post_header_len[UPDATE_ROWS_EVENT-1]=  ROWS_HEADER_LEN;
      post_header_len[DELETE_ROWS_EVENT-1]=  ROWS_HEADER_LEN;
      post_header_len[BEGIN_LOAD_QUERY_EVENT-1]= post_header_len[APPEND_BLOCK_EVENT-1];
      post_header_len[EXECUTE_LOAD_QUERY_EVENT-1]= EXECUTE_LOAD_QUERY_HEADER_LEN;
      post_header_len[INCIDENT_EVENT-1]= INCIDENT_HEADER_LEN;
      post_header_len[HEARTBEAT_LOG_EVENT-1]= 0;
    }
    break;

  default: /* Includes binlog version 2 i.e. 4.0.x x<=1 */
    assert(0);
  }
  calc_server_version_split();
}


/**
  The problem with this constructor is that the fixed header may have a
  length different from this version, but we don't know this length as we
  have not read the Format_description_log_event which says it, yet. This
  length is in the post-header of the event, but we don't know where the
  post-header starts.

  So this type of event HAS to:
  - either have the header's length at the beginning (in the header, at a
  fixed position which will never be changed), not in the post-header. That
  would make the header be "shifted" compared to other events.
  - or have a header of size LOG_EVENT_MINIMAL_HEADER_LEN (19), in all future
  versions, so that we know for sure.

  I (Guilhem) chose the 2nd solution. Rotate has the same constraint (because
  it is sent before Format_description_log_event).
*/

Format_description_log_event::
Format_description_log_event(const char* buf,
                             uint32_t event_len,
                             const
                             Format_description_log_event*
                             description_event)
  :Start_log_event_v3(buf, description_event), event_type_permutation(0)
{
  buf+= LOG_EVENT_MINIMAL_HEADER_LEN;
  if ((common_header_len=buf[ST_COMMON_HEADER_LEN_OFFSET]) < OLD_HEADER_LEN)
    return; /* sanity check */
  number_of_event_types=
    event_len-(LOG_EVENT_MINIMAL_HEADER_LEN+ST_COMMON_HEADER_LEN_OFFSET+1);
  post_header_len= (uint8_t*) malloc(number_of_event_types*
                                     sizeof(*post_header_len));
  /* If alloc fails, we'll detect it in is_valid() */
  if (post_header_len != NULL)
    memcpy(post_header_len, buf+ST_COMMON_HEADER_LEN_OFFSET+1,
           number_of_event_types* sizeof(*post_header_len));
  calc_server_version_split();

  /*
    In some previous versions, the events were given other event type
    id numbers than in the present version. When replicating from such
    a version, we therefore set up an array that maps those id numbers
    to the id numbers of the present server.

    If post_header_len is null, it means malloc failed, and is_valid
    will fail, so there is no need to do anything.

    The trees in which events have wrong id's are:

    mysql-5.1-wl1012.old mysql-5.1-wl2325-5.0-drop6p13-alpha
    mysql-5.1-wl2325-5.0-drop6 mysql-5.1-wl2325-5.0
    mysql-5.1-wl2325-no-dd

    (this was found by grepping for two lines in sequence where the
    first matches "FORMAT_DESCRIPTION_EVENT," and the second matches
    "TABLE_MAP_EVENT," in log_event.h in all trees)

    In these trees, the following server_versions existed since
    TABLE_MAP_EVENT was introduced:

    5.1.1-a_drop5p3   5.1.1-a_drop5p4        5.1.1-alpha
    5.1.2-a_drop5p10  5.1.2-a_drop5p11       5.1.2-a_drop5p12
    5.1.2-a_drop5p13  5.1.2-a_drop5p14       5.1.2-a_drop5p15
    5.1.2-a_drop5p16  5.1.2-a_drop5p16b      5.1.2-a_drop5p16c
    5.1.2-a_drop5p17  5.1.2-a_drop5p4        5.1.2-a_drop5p5
    5.1.2-a_drop5p6   5.1.2-a_drop5p7        5.1.2-a_drop5p8
    5.1.2-a_drop5p9   5.1.3-a_drop5p17       5.1.3-a_drop5p17b
    5.1.3-a_drop5p17c 5.1.4-a_drop5p18       5.1.4-a_drop5p19
    5.1.4-a_drop5p20  5.1.4-a_drop6p0        5.1.4-a_drop6p1
    5.1.4-a_drop6p2   5.1.5-a_drop5p20       5.2.0-a_drop6p3
    5.2.0-a_drop6p4   5.2.0-a_drop6p5        5.2.0-a_drop6p6
    5.2.1-a_drop6p10  5.2.1-a_drop6p11       5.2.1-a_drop6p12
    5.2.1-a_drop6p6   5.2.1-a_drop6p7        5.2.1-a_drop6p8
    5.2.2-a_drop6p13  5.2.2-a_drop6p13-alpha 5.2.2-a_drop6p13b
    5.2.2-a_drop6p13c

    (this was found by grepping for "mysql," in all historical
    versions of configure.in in the trees listed above).

    There are 5.1.1-alpha versions that use the new event id's, so we
    do not test that version string.  So replication from 5.1.1-alpha
    with the other event id's to a new version does not work.
    Moreover, we can safely ignore the part after drop[56].  This
    allows us to simplify the big list above to the following regexes:

    5\.1\.[1-5]-a_drop5.*
    5\.1\.4-a_drop6.*
    5\.2\.[0-2]-a_drop6.*

    This is what we test for in the 'if' below.
  */
  if (post_header_len &&
      server_version[0] == '5' && server_version[1] == '.' &&
      server_version[3] == '.' &&
      strncmp(server_version + 5, "-a_drop", 7) == 0 &&
      ((server_version[2] == '1' &&
        server_version[4] >= '1' && server_version[4] <= '5' &&
        server_version[12] == '5') ||
       (server_version[2] == '1' &&
        server_version[4] == '4' &&
        server_version[12] == '6') ||
       (server_version[2] == '2' &&
        server_version[4] >= '0' && server_version[4] <= '2' &&
        server_version[12] == '6')))
  {
    if (number_of_event_types != 22)
    {
      /* this makes is_valid() return false. */
      free(post_header_len);
      post_header_len= NULL;
      return;
    }
    static const uint8_t perm[23]=
      {
        UNKNOWN_EVENT, START_EVENT_V3, QUERY_EVENT, STOP_EVENT, ROTATE_EVENT,
        LOAD_EVENT, SLAVE_EVENT, CREATE_FILE_EVENT,
        APPEND_BLOCK_EVENT, EXEC_LOAD_EVENT, DELETE_FILE_EVENT,
        NEW_LOAD_EVENT,
        FORMAT_DESCRIPTION_EVENT,
        TABLE_MAP_EVENT,
        XID_EVENT,
        BEGIN_LOAD_QUERY_EVENT,
        EXECUTE_LOAD_QUERY_EVENT,
      };
    event_type_permutation= perm;
    /*
      Since we use (permuted) event id's to index the post_header_len
      array, we need to permute the post_header_len array too.
    */
    uint8_t post_header_len_temp[23];
    for (int i= 1; i < 23; i++)
      post_header_len_temp[perm[i] - 1]= post_header_len[i - 1];
    for (int i= 0; i < 22; i++)
      post_header_len[i] = post_header_len_temp[i];
  }
  return;
}

bool Format_description_log_event::write(IO_CACHE* file)
{
  /*
    We don't call Start_log_event_v3::write() because this would make 2
    my_b_safe_write().
  */
  unsigned char buff[FORMAT_DESCRIPTION_HEADER_LEN];
  int2store(buff + ST_BINLOG_VER_OFFSET,binlog_version);
  memcpy(buff + ST_SERVER_VER_OFFSET,server_version,ST_SERVER_VER_LEN);
  if (!dont_set_created)
    created= when= get_time();
  int4store(buff + ST_CREATED_OFFSET,created);
  buff[ST_COMMON_HEADER_LEN_OFFSET]= LOG_EVENT_HEADER_LEN;
  memcpy(buff+ST_COMMON_HEADER_LEN_OFFSET+1, post_header_len,
         LOG_EVENT_TYPES);
  return (write_header(file, sizeof(buff)) ||
          my_b_safe_write(file, buff, sizeof(buff)));
}


int Format_description_log_event::do_apply_event(Relay_log_info const *rli)
{
  /*
    As a transaction NEVER spans on 2 or more binlogs:
    if we have an active transaction at this point, the master died
    while writing the transaction to the binary log, i.e. while
    flushing the binlog cache to the binlog. XA guarantees that master has
    rolled back. So we roll back.
    Note: this event could be sent by the master to inform us of the
    format of its binlog; in other words maybe it is not at its
    original place when it comes to us; we'll know this by checking
    log_pos ("artificial" events have log_pos == 0).
  */
  if (!artificial_event && created && session->transaction.all.ha_list)
  {
    /* This is not an error (XA is safe), just an information */
    rli->report(INFORMATION_LEVEL, 0,
                _("Rolling back unfinished transaction (no COMMIT "
                  "or ROLLBACK in relay log). A probable cause is that "
                  "the master died while writing the transaction to "
                  "its binary log, thus rolled back too."));
    const_cast<Relay_log_info*>(rli)->cleanup_context(session, 1);
  }
  /*
    If this event comes from ourselves, there is no cleaning task to
    perform, we don't call Start_log_event_v3::do_apply_event()
    (this was just to update the log's description event).
  */
  if (server_id != ::server_id)
  {
    /*
      If the event was not requested by the slave i.e. the master sent
      it while the slave asked for a position >4, the event will make
      rli->group_master_log_pos advance. Say that the slave asked for
      position 1000, and the Format_desc event's end is 96. Then in
      the beginning of replication rli->group_master_log_pos will be
      0, then 96, then jump to first really asked event (which is
      >96). So this is ok.
    */
    return(Start_log_event_v3::do_apply_event(rli));
  }
  return(0);
}

int Format_description_log_event::do_update_pos(Relay_log_info *rli)
{
  /* save the information describing this binlog */
  delete rli->relay_log.description_event_for_exec;
  rli->relay_log.description_event_for_exec= this;

  if (server_id == ::server_id)
  {
    /*
      We only increase the relay log position if we are skipping
      events and do not touch any group_* variables, nor flush the
      relay log info.  If there is a crash, we will have to re-skip
      the events again, but that is a minor issue.

      If we do not skip stepping the group log position (and the
      server id was changed when restarting the server), it might well
      be that we start executing at a position that is invalid, e.g.,
      at a Rows_log_event or a Query_log_event preceeded by a
      Intvar_log_event instead of starting at a Table_map_log_event or
      the Intvar_log_event respectively.
     */
    rli->inc_event_relay_log_pos();
    return 0;
  }
  else
  {
    return Log_event::do_update_pos(rli);
  }
}

Log_event::enum_skip_reason
Format_description_log_event::do_shall_skip(Relay_log_info *)
{
  return Log_event::EVENT_SKIP_NOT;
}


/**
   Splits the event's 'server_version' string into three numeric pieces stored
   into 'server_version_split':
   X.Y.Zabc (X,Y,Z numbers, a not a digit) -> {X,Y,Z}
   X.Yabc -> {X,Y,0}
   Xabc -> {X,0,0}
   'server_version_split' is then used for lookups to find if the server which
   created this event has some known bug.
*/
void Format_description_log_event::calc_server_version_split()
{
  char *p= server_version, *r;
  ulong number;
  for (uint32_t i= 0; i<=2; i++)
  {
    number= strtoul(p, &r, 10);
    server_version_split[i]= (unsigned char)number;
    assert(number < 256); // fit in unsigned char
    p= r;
    assert(!((i == 0) && (*r != '.'))); // should be true in practice
    if (*r == '.')
      p++; // skip the dot
  }
}


  /**************************************************************************
        Load_log_event methods
   General note about Load_log_event: the binlogging of LOAD DATA INFILE is
   going to be changed in 5.0 (or maybe in 5.1; not decided yet).
   However, the 5.0 slave could still have to read such events (from a 4.x
   master), convert them (which just means maybe expand the header, when 5.0
   servers have a UID in events) (remember that whatever is after the header
   will be like in 4.x, as this event's format is not modified in 5.0 as we
   will use new types of events to log the new LOAD DATA INFILE features).
   To be able to read/convert, we just need to not assume that the common
   header is of length LOG_EVENT_HEADER_LEN (we must use the description
   event).
   Note that I (Guilhem) manually tested replication of a big LOAD DATA INFILE
   between 3.23 and 5.0, and between 4.0 and 5.0, and it works fine (and the
   positions displayed in SHOW SLAVE STATUS then are fine too).
  **************************************************************************/

/*
  Load_log_event::pack_info()
*/

uint32_t Load_log_event::get_query_buffer_length()
{
  return
    5 + db_len + 3 +                        // "use DB; "
    18 + fname_len + 2 +                    // "LOAD DATA INFILE 'file''"
    7 +					    // LOCAL
    9 +                                     // " REPLACE or IGNORE "
    13 + table_name_len*2 +                 // "INTO Table `table`"
    21 + sql_ex.field_term_len*4 + 2 +      // " FIELDS TERMINATED BY 'str'"
    23 + sql_ex.enclosed_len*4 + 2 +        // " OPTIONALLY ENCLOSED BY 'str'"
    12 + sql_ex.escaped_len*4 + 2 +         // " ESCAPED BY 'str'"
    21 + sql_ex.line_term_len*4 + 2 +       // " LINES TERMINATED BY 'str'"
    19 + sql_ex.line_start_len*4 + 2 +      // " LINES STARTING BY 'str'"
    15 + 22 +                               // " IGNORE xxx  LINES"
    3 + (num_fields-1)*2 + field_block_len; // " (field1, field2, ...)"
}


void Load_log_event::print_query(bool need_db, char *buf,
                                 char **end, char **fn_start, char **fn_end)
{
  char *pos= buf;

  if (need_db && db && db_len)
  {
    pos= strcpy(pos, "use `")+5;
    memcpy(pos, db, db_len);
    pos= strcpy(pos+db_len, "`; ")+3;
  }

  pos= strcpy(pos, "LOAD DATA ")+10;

  if (fn_start)
    *fn_start= pos;

  if (check_fname_outside_temp_buf())
    pos= strcpy(pos, "LOCAL ")+6;
  pos= strcpy(pos, "INFILE '")+8;
  memcpy(pos, fname, fname_len);
  pos= strcpy(pos+fname_len, "' ")+2;

  if (sql_ex.opt_flags & REPLACE_FLAG)
    pos= strcpy(pos, " REPLACE ")+9;
  else if (sql_ex.opt_flags & IGNORE_FLAG)
    pos= strcpy(pos, " IGNORE ")+8;

  pos= strcpy(pos ,"INTO")+4;

  if (fn_end)
    *fn_end= pos;

  pos= strcpy(pos ," Table `")+8;
  memcpy(pos, table_name, table_name_len);
  pos+= table_name_len;

  /* We have to create all optinal fields as the default is not empty */
  pos= strcpy(pos, "` FIELDS TERMINATED BY ")+23;
  pos= pretty_print_str(pos, sql_ex.field_term, sql_ex.field_term_len);
  if (sql_ex.opt_flags & OPT_ENCLOSED_FLAG)
    pos= strcpy(pos, " OPTIONALLY ")+12;
  pos= strcpy(pos, " ENCLOSED BY ")+13;
  pos= pretty_print_str(pos, sql_ex.enclosed, sql_ex.enclosed_len);

  pos= strcpy(pos, " ESCAPED BY ")+12;
  pos= pretty_print_str(pos, sql_ex.escaped, sql_ex.escaped_len);

  pos= strcpy(pos, " LINES TERMINATED BY ")+21;
  pos= pretty_print_str(pos, sql_ex.line_term, sql_ex.line_term_len);
  if (sql_ex.line_start_len)
  {
    pos= strcpy(pos, " STARTING BY ")+13;
    pos= pretty_print_str(pos, sql_ex.line_start, sql_ex.line_start_len);
  }

  if ((long) skip_lines > 0)
  {
    pos= strcpy(pos, " IGNORE ")+8;
    pos= int64_t10_to_str((int64_t) skip_lines, pos, 10);
    pos= strcpy(pos," LINES ")+7;
  }

  if (num_fields)
  {
    uint32_t i;
    const char *field= fields;
    pos= strcpy(pos, " (")+2;
    for (i = 0; i < num_fields; i++)
    {
      if (i)
      {
        *pos++= ' ';
        *pos++= ',';
      }
      memcpy(pos, field, field_lens[i]);
      pos+=   field_lens[i];
      field+= field_lens[i]  + 1;
    }
    *pos++= ')';
  }

  *end= pos;
}


void Load_log_event::pack_info(Protocol *protocol)
{
  char *buf, *end;

  if (!(buf= (char*) malloc(get_query_buffer_length())))
    return;
  print_query(true, buf, &end, 0, 0);
  protocol->store(buf, end-buf, &my_charset_bin);
  free(buf);
}


/*
  Load_log_event::write_data_header()
*/

bool Load_log_event::write_data_header(IO_CACHE* file)
{
  char buf[LOAD_HEADER_LEN];
  int4store(buf + L_THREAD_ID_OFFSET, slave_proxy_id);
  int4store(buf + L_EXEC_TIME_OFFSET, exec_time);
  int4store(buf + L_SKIP_LINES_OFFSET, skip_lines);
  buf[L_TBL_LEN_OFFSET] = (char)table_name_len;
  buf[L_DB_LEN_OFFSET] = (char)db_len;
  int4store(buf + L_NUM_FIELDS_OFFSET, num_fields);
  return my_b_safe_write(file, (unsigned char*)buf, LOAD_HEADER_LEN) != 0;
}


/*
  Load_log_event::write_data_body()
*/

bool Load_log_event::write_data_body(IO_CACHE* file)
{
  if (sql_ex.write_data(file))
    return 1;
  if (num_fields && fields && field_lens)
  {
    if (my_b_safe_write(file, (unsigned char*)field_lens, num_fields) ||
	my_b_safe_write(file, (unsigned char*)fields, field_block_len))
      return 1;
  }
  return (my_b_safe_write(file, (unsigned char*)table_name, table_name_len + 1) ||
	  my_b_safe_write(file, (unsigned char*)db, db_len + 1) ||
	  my_b_safe_write(file, (unsigned char*)fname, fname_len));
}


/*
  Load_log_event::Load_log_event()
*/

Load_log_event::Load_log_event(Session *session_arg, sql_exchange *ex,
			       const char *db_arg, const char *table_name_arg,
			       List<Item> &fields_arg,
			       enum enum_duplicates handle_dup,
			       bool ignore, bool using_trans)
  :Log_event(session_arg,
             session_arg->thread_specific_used ? LOG_EVENT_THREAD_SPECIFIC_F : 0,
             using_trans),
   thread_id(session_arg->thread_id),
   slave_proxy_id(session_arg->variables.pseudo_thread_id),
   num_fields(0),fields(0),
   field_lens(0),field_block_len(0),
   table_name(table_name_arg ? table_name_arg : ""),
   db(db_arg), fname(ex->file_name), local_fname(false)
{
  time_t end_time;
  time(&end_time);
  exec_time = (ulong) (end_time  - session_arg->start_time);
  /* db can never be a zero pointer in 4.0 */
  db_len = (uint32_t) strlen(db);
  table_name_len = (uint32_t) strlen(table_name);
  fname_len = (fname) ? (uint) strlen(fname) : 0;
  sql_ex.field_term = (char*) ex->field_term->ptr();
  sql_ex.field_term_len = (uint8_t) ex->field_term->length();
  sql_ex.enclosed = (char*) ex->enclosed->ptr();
  sql_ex.enclosed_len = (uint8_t) ex->enclosed->length();
  sql_ex.line_term = (char*) ex->line_term->ptr();
  sql_ex.line_term_len = (uint8_t) ex->line_term->length();
  sql_ex.line_start = (char*) ex->line_start->ptr();
  sql_ex.line_start_len = (uint8_t) ex->line_start->length();
  sql_ex.escaped = (char*) ex->escaped->ptr();
  sql_ex.escaped_len = (uint8_t) ex->escaped->length();
  sql_ex.opt_flags = 0;
  sql_ex.cached_new_format = -1;

  if (ex->dumpfile)
    sql_ex.opt_flags|= DUMPFILE_FLAG;
  if (ex->opt_enclosed)
    sql_ex.opt_flags|= OPT_ENCLOSED_FLAG;

  sql_ex.empty_flags= 0;

  switch (handle_dup) {
  case DUP_REPLACE:
    sql_ex.opt_flags|= REPLACE_FLAG;
    break;
  case DUP_UPDATE:				// Impossible here
  case DUP_ERROR:
    break;
  }
  if (ignore)
    sql_ex.opt_flags|= IGNORE_FLAG;

  if (!ex->field_term->length())
    sql_ex.empty_flags |= FIELD_TERM_EMPTY;
  if (!ex->enclosed->length())
    sql_ex.empty_flags |= ENCLOSED_EMPTY;
  if (!ex->line_term->length())
    sql_ex.empty_flags |= LINE_TERM_EMPTY;
  if (!ex->line_start->length())
    sql_ex.empty_flags |= LINE_START_EMPTY;
  if (!ex->escaped->length())
    sql_ex.empty_flags |= ESCAPED_EMPTY;

  skip_lines = ex->skip_lines;

  List_iterator<Item> li(fields_arg);
  field_lens_buf.length(0);
  fields_buf.length(0);
  Item* item;
  while ((item = li++))
  {
    num_fields++;
    unsigned char len = (unsigned char) strlen(item->name);
    field_block_len += len + 1;
    fields_buf.append(item->name, len + 1);
    field_lens_buf.append((char*)&len, 1);
  }

  field_lens = (const unsigned char*)field_lens_buf.ptr();
  fields = fields_buf.ptr();
}


/**
  @note
    The caller must do buf[event_len] = 0 before he starts using the
    constructed event.
*/
Load_log_event::Load_log_event(const char *buf, uint32_t event_len,
                               const Format_description_log_event *description_event)
  :Log_event(buf, description_event), num_fields(0), fields(0),
   field_lens(0),field_block_len(0),
   table_name(0), db(0), fname(0), local_fname(false)
{
  /*
    I (Guilhem) manually tested replication of LOAD DATA INFILE for 3.23->5.0,
    4.0->5.0 and 5.0->5.0 and it works.
  */
  if (event_len)
    copy_log_event(buf, event_len,
                   ((buf[EVENT_TYPE_OFFSET] == LOAD_EVENT) ?
                    LOAD_HEADER_LEN +
                    description_event->common_header_len :
                    LOAD_HEADER_LEN + LOG_EVENT_HEADER_LEN),
                   description_event);
  /* otherwise it's a derived class, will call copy_log_event() itself */
  return;
}


/*
  Load_log_event::copy_log_event()
*/

int Load_log_event::copy_log_event(const char *buf, ulong event_len,
                                   int body_offset,
                                   const Format_description_log_event *description_event)
{
  uint32_t data_len;
  char* buf_end = (char*)buf + event_len;
  /* this is the beginning of the post-header */
  const char* data_head = buf + description_event->common_header_len;
  slave_proxy_id= thread_id= uint4korr(data_head + L_THREAD_ID_OFFSET);
  exec_time = uint4korr(data_head + L_EXEC_TIME_OFFSET);
  skip_lines = uint4korr(data_head + L_SKIP_LINES_OFFSET);
  table_name_len = (uint)data_head[L_TBL_LEN_OFFSET];
  db_len = (uint)data_head[L_DB_LEN_OFFSET];
  num_fields = uint4korr(data_head + L_NUM_FIELDS_OFFSET);

  if ((int) event_len < body_offset)
    return(1);
  /*
    Sql_ex.init() on success returns the pointer to the first byte after
    the sql_ex structure, which is the start of field lengths array.
  */
  if (!(field_lens= (unsigned char*)sql_ex.init((char*)buf + body_offset,
                                        buf_end,
                                        buf[EVENT_TYPE_OFFSET] != LOAD_EVENT)))
    return(1);

  data_len = event_len - body_offset;
  if (num_fields > data_len) // simple sanity check against corruption
    return(1);
  for (uint32_t i = 0; i < num_fields; i++)
    field_block_len += (uint)field_lens[i] + 1;

  fields = (char*)field_lens + num_fields;
  table_name  = fields + field_block_len;
  db = table_name + table_name_len + 1;
  fname = db + db_len + 1;
  fname_len = strlen(fname);
  // null termination is accomplished by the caller doing buf[event_len]=0

  return(0);
}


/**
  Load_log_event::set_fields()

  @note
    This function can not use the member variable
    for the database, since LOAD DATA INFILE on the slave
    can be for a different database than the current one.
    This is the reason for the affected_db argument to this method.
*/

void Load_log_event::set_fields(const char* affected_db,
				List<Item> &field_list,
                                Name_resolution_context *context)
{
  uint32_t i;
  const char* field = fields;
  for (i= 0; i < num_fields; i++)
  {
    field_list.push_back(new Item_field(context,
                                        affected_db, table_name, field));
    field+= field_lens[i]  + 1;
  }
}


/**
  Does the data loading job when executing a LOAD DATA on the slave.

  @param net
  @param rli
  @param use_rli_only_for_errors     If set to 1, rli is provided to
                                     Load_log_event::exec_event only for this
                                     function to have RPL_LOG_NAME and
                                     rli->last_slave_error, both being used by
                                     error reports. rli's position advancing
                                     is skipped (done by the caller which is
                                     Execute_load_log_event::exec_event).
                                     If set to 0, rli is provided for full use,
                                     i.e. for error reports and position
                                     advancing.

  @todo
    fix this; this can be done by testing rules in
    Create_file_log_event::exec_event() and then discarding Append_block and
    al.
  @todo
    this is a bug - this needs to be moved to the I/O thread

  @retval
    0           Success
  @retval
    1           Failure
*/

int Load_log_event::do_apply_event(NET* net, Relay_log_info const *rli,
                                   bool use_rli_only_for_errors)
{
  Query_id &query_id= Query_id::get_query_id();
  session->set_db(db, strlen(db));
  assert(session->query == 0);
  session->query_length= 0;                         // Should not be needed
  session->is_slave_error= 0;
  clear_all_errors(session, const_cast<Relay_log_info*>(rli));

  /* see Query_log_event::do_apply_event() and BUG#13360 */
  assert(!rli->m_table_map.count());
  /*
    Usually lex_start() is called by mysql_parse(), but we need it here
    as the present method does not call mysql_parse().
  */
  lex_start(session);
  mysql_reset_session_for_next_command(session);

  if (!use_rli_only_for_errors)
  {
    /*
      Saved for InnoDB, see comment in
      Query_log_event::do_apply_event()
    */
    const_cast<Relay_log_info*>(rli)->future_group_master_log_pos= log_pos;
  }

   /*
    We test replicate_*_db rules. Note that we have already prepared
    the file to load, even if we are going to ignore and delete it
    now. So it is possible that we did a lot of disk writes for
    nothing. In other words, a big LOAD DATA INFILE on the master will
    still consume a lot of space on the slave (space in the relay log
    + space of temp files: twice the space of the file to load...)
    even if it will finally be ignored.  TODO: fix this; this can be
    done by testing rules in Create_file_log_event::do_apply_event()
    and then discarding Append_block and al. Another way is do the
    filtering in the I/O thread (more efficient: no disk writes at
    all).


    Note:   We do not need to execute reset_one_shot_variables() if this
            db_ok() test fails.
    Reason: The db stored in binlog events is the same for SET and for
            its companion query.  If the SET is ignored because of
            db_ok(), the companion query will also be ignored, and if
            the companion query is ignored in the db_ok() test of
            ::do_apply_event(), then the companion SET also have so
            we don't need to reset_one_shot_variables().
  */
  if (1)
  {
    session->set_time((time_t)when);
    session->query_id = query_id.next();
    /*
      Initing session->row_count is not necessary in theory as this variable has no
      influence in the case of the slave SQL thread (it is used to generate a
      "data truncated" warning but which is absorbed and never gets to the
      error log); still we init it to avoid a Valgrind message.
    */
    drizzle_reset_errors(session, 0);

    TableList tables;
    memset(&tables, 0, sizeof(tables));
    tables.db= session->strmake(session->db, session->db_length);
    tables.alias = tables.table_name = (char*) table_name;
    tables.lock_type = TL_WRITE;
    tables.updating= 1;

    // the table will be opened in mysql_load
    {
      char llbuff[22];
      char *end;
      enum enum_duplicates handle_dup;
      bool ignore= 0;
      char *load_data_query;

      /*
        Forge LOAD DATA INFILE query which will be used in SHOW PROCESS LIST
        and written to slave's binlog if binlogging is on.
      */
      if (!(load_data_query= (char *)session->alloc(get_query_buffer_length() + 1)))
      {
        /*
          This will set session->fatal_error in case of OOM. So we surely will notice
          that something is wrong.
        */
        goto error;
      }

      print_query(false, load_data_query, &end, (char **)&session->lex->fname_start,
                  (char **)&session->lex->fname_end);
      *end= 0;
      session->query_length= end - load_data_query;
      session->query= load_data_query;

      if (sql_ex.opt_flags & REPLACE_FLAG)
      {
        handle_dup= DUP_REPLACE;
      }
      else if (sql_ex.opt_flags & IGNORE_FLAG)
      {
        ignore= 1;
        handle_dup= DUP_ERROR;
      }
      else
      {
        /*
          When replication is running fine, if it was DUP_ERROR on the
          master then we could choose IGNORE here, because if DUP_ERROR
          suceeded on master, and data is identical on the master and slave,
          then there should be no uniqueness errors on slave, so IGNORE is
          the same as DUP_ERROR. But in the unlikely case of uniqueness errors
          (because the data on the master and slave happen to be different
          (user error or bug), we want LOAD DATA to print an error message on
          the slave to discover the problem.

          If reading from net (a 3.23 master), mysql_load() will change this
          to IGNORE.
        */
        handle_dup= DUP_ERROR;
      }
      /*
        We need to set session->lex->sql_command and session->lex->duplicates
        since InnoDB tests these variables to decide if this is a LOAD
        DATA ... REPLACE INTO ... statement even though mysql_parse()
        is not called.  This is not needed in 5.0 since there the LOAD
        DATA ... statement is replicated using mysql_parse(), which
        sets the session->lex fields correctly.
      */
      session->lex->sql_command= SQLCOM_LOAD;
      session->lex->duplicates= handle_dup;

      sql_exchange ex((char*)fname, sql_ex.opt_flags & DUMPFILE_FLAG);
      String field_term(sql_ex.field_term,sql_ex.field_term_len,&my_charset_utf8_general_ci);
      String enclosed(sql_ex.enclosed,sql_ex.enclosed_len,&my_charset_utf8_general_ci);
      String line_term(sql_ex.line_term,sql_ex.line_term_len,&my_charset_utf8_general_ci);
      String line_start(sql_ex.line_start,sql_ex.line_start_len,&my_charset_utf8_general_ci);
      String escaped(sql_ex.escaped,sql_ex.escaped_len, &my_charset_utf8_general_ci);
      ex.field_term= &field_term;
      ex.enclosed= &enclosed;
      ex.line_term= &line_term;
      ex.line_start= &line_start;
      ex.escaped= &escaped;

      ex.opt_enclosed = (sql_ex.opt_flags & OPT_ENCLOSED_FLAG);
      if (sql_ex.empty_flags & FIELD_TERM_EMPTY)
        ex.field_term->length(0);

      ex.skip_lines = skip_lines;
      List<Item> field_list;
      session->lex->select_lex.context.resolve_in_table_list_only(&tables);
      set_fields(tables.db, field_list, &session->lex->select_lex.context);
      session->variables.pseudo_thread_id= thread_id;
      if (net)
      {
        // mysql_load will use session->net to read the file
        session->net.vio = net->vio;
        /*
          Make sure the client does not get confused about the packet sequence
        */
        session->net.pkt_nr = net->pkt_nr;
      }
      /*
        It is safe to use tmp_list twice because we are not going to
        update it inside mysql_load().
      */
      List<Item> tmp_list;
      if (mysql_load(session, &ex, &tables, field_list, tmp_list, tmp_list,
                     handle_dup, ignore, net != 0))
        session->is_slave_error= 1;
      if (session->cuted_fields)
      {
        /* log_pos is the position of the LOAD event in the master log */
        sql_print_warning(_("Slave: load data infile on table '%s' at "
                            "log position %s in log '%s' produced %ld "
                            "warning(s). Default database: '%s'"),
                          (char*) table_name,
                          llstr(log_pos,llbuff), RPL_LOG_NAME,
                          (ulong) session->cuted_fields,
                          print_slave_db_safe(session->db));
      }
      if (net)
        net->pkt_nr= session->net.pkt_nr;
    }
  }
  else
  {
    /*
      We will just ask the master to send us /dev/null if we do not
      want to load the data.
      TODO: this a bug - needs to be done in I/O thread
    */
    if (net)
      skip_load_data_infile(net);
  }

error:
  session->net.vio = 0;
  const char *remember_db= session->db;
  pthread_mutex_lock(&LOCK_thread_count);
  session->catalog= 0;
  session->set_db(NULL, 0);                   /* will free the current database */
  session->query= 0;
  session->query_length= 0;
  pthread_mutex_unlock(&LOCK_thread_count);
  close_thread_tables(session);

  if (session->is_slave_error)
  {
    /* this err/sql_errno code is copy-paste from net_send_error() */
    const char *err;
    int sql_errno;
    if (session->is_error())
    {
      err= session->main_da.message();
      sql_errno= session->main_da.sql_errno();
    }
    else
    {
      sql_errno=ER_UNKNOWN_ERROR;
      err=ER(sql_errno);
    }
    rli->report(ERROR_LEVEL, sql_errno,
                _("Error '%s' running LOAD DATA INFILE on table '%s'. "
                  "Default database: '%s'"),
                err, (char*)table_name, print_slave_db_safe(remember_db));
    free_root(session->mem_root,MYF(MY_KEEP_PREALLOC));
    return 1;
  }
  free_root(session->mem_root,MYF(MY_KEEP_PREALLOC));

  if (session->is_fatal_error)
  {
    char buf[256];
    snprintf(buf, sizeof(buf),
             _("Running LOAD DATA INFILE on table '%-.64s'."
               " Default database: '%-.64s'"),
             (char*)table_name,
             print_slave_db_safe(remember_db));

    rli->report(ERROR_LEVEL, ER_SLAVE_FATAL_ERROR,
                ER(ER_SLAVE_FATAL_ERROR), buf);
    return 1;
  }

  return ( use_rli_only_for_errors ? 0 : Log_event::do_apply_event(rli) );
}


/**************************************************************************
  Rotate_log_event methods
**************************************************************************/

/*
  Rotate_log_event::pack_info()
*/

void Rotate_log_event::pack_info(Protocol *protocol)
{
  char buf1[256], buf[22];
  String tmp(buf1, sizeof(buf1), &my_charset_utf8_general_ci);
  tmp.length(0);
  tmp.append(new_log_ident.c_str(), ident_len);
  tmp.append(STRING_WITH_LEN(";pos="));
  tmp.append(llstr(pos,buf));
  protocol->store(tmp.ptr(), tmp.length(), &my_charset_bin);
}


/*
  Rotate_log_event::Rotate_log_event() (2 constructors)
*/


Rotate_log_event::Rotate_log_event(const char* new_log_ident_arg,
                                   uint32_t ident_len_arg, uint64_t pos_arg,
                                   uint32_t flags_arg)
  :Log_event(), pos(pos_arg),
   ident_len(ident_len_arg
               ? ident_len_arg
               : strlen(new_log_ident_arg)),
   flags(flags_arg)
{
  new_log_ident.assign(new_log_ident_arg, ident_len);
  return;
}


Rotate_log_event::Rotate_log_event(const char* buf, uint32_t event_len,
                                   const Format_description_log_event* description_event)
  :Log_event(buf, description_event), flags(DUP_NAME)
{
  // The caller will ensure that event_len is what we have at EVENT_LEN_OFFSET
  uint8_t header_size= description_event->common_header_len;
  uint8_t post_header_len= description_event->post_header_len[ROTATE_EVENT-1];
  uint32_t ident_offset;
  if (event_len < header_size)
    return;
  buf += header_size;
  pos = post_header_len ? uint8korr(buf + R_POS_OFFSET) : 4;
  ident_len = (uint)(event_len -
                     (header_size+post_header_len));
  ident_offset = post_header_len;
  set_if_smaller(ident_len,FN_REFLEN-1);
  new_log_ident.assign(buf + ident_offset, ident_len);
  return;
}


/*
  Rotate_log_event::write()
*/

bool Rotate_log_event::write(IO_CACHE* file)
{
  char buf[ROTATE_HEADER_LEN];
  int8store(buf + R_POS_OFFSET, pos);
  return (write_header(file, ROTATE_HEADER_LEN + ident_len) ||
          my_b_safe_write(file, (unsigned char*)buf, ROTATE_HEADER_LEN) ||
          my_b_safe_write(file, (const unsigned char*)new_log_ident.c_str(),
                          (uint) ident_len));
}


/*
  Got a rotate log event from the master.

  This is mainly used so that we can later figure out the logname and
  position for the master.

  We can't rotate the slave's BINlog as this will cause infinitive rotations
  in a A -> B -> A setup.
  The NOTES below is a wrong comment which will disappear when 4.1 is merged.

  @retval
    0	ok
*/
int Rotate_log_event::do_update_pos(Relay_log_info *rli)
{
  pthread_mutex_lock(&rli->data_lock);
  rli->event_relay_log_pos= my_b_tell(rli->cur_log);
  /*
    If we are in a transaction or in a group: the only normal case is
    when the I/O thread was copying a big transaction, then it was
    stopped and restarted: we have this in the relay log:

    BEGIN
    ...
    ROTATE (a fake one)
    ...
    COMMIT or ROLLBACK

    In that case, we don't want to touch the coordinates which
    correspond to the beginning of the transaction.  Starting from
    5.0.0, there also are some rotates from the slave itself, in the
    relay log, which shall not change the group positions.
  */
  if ((server_id != ::server_id || rli->replicate_same_server_id) &&
      !rli->is_in_group())
  {
    rli->group_master_log_name.assign(new_log_ident);
    rli->notify_group_master_log_name_update();
    rli->group_master_log_pos= pos;
    rli->group_relay_log_name.assign(rli->event_relay_log_name);
    rli->notify_group_relay_log_name_update();
    rli->group_relay_log_pos= rli->event_relay_log_pos;
    /*
      Reset session->options and sql_mode etc, because this could be the signal of
      a master's downgrade from 5.0 to 4.0.
      However, no need to reset description_event_for_exec: indeed, if the next
      master is 5.0 (even 5.0.1) we will soon get a Format_desc; if the next
      master is 4.0 then the events are in the slave's format (conversion).
    */
    set_slave_thread_options(session);
    session->variables.auto_increment_increment=
      session->variables.auto_increment_offset= 1;
  }
  pthread_mutex_unlock(&rli->data_lock);
  pthread_cond_broadcast(&rli->data_cond);
  flush_relay_log_info(rli);

  return(0);
}


Log_event::enum_skip_reason
Rotate_log_event::do_shall_skip(Relay_log_info *rli)
{
  enum_skip_reason reason= Log_event::do_shall_skip(rli);

  switch (reason) {
  case Log_event::EVENT_SKIP_NOT:
  case Log_event::EVENT_SKIP_COUNT:
    return Log_event::EVENT_SKIP_NOT;

  case Log_event::EVENT_SKIP_IGNORE:
    return Log_event::EVENT_SKIP_IGNORE;
  }
  assert(0);
  return Log_event::EVENT_SKIP_NOT;             // To keep compiler happy
}


/**************************************************************************
  Xid_log_event methods
**************************************************************************/

void Xid_log_event::pack_info(Protocol *protocol)
{
  char buf[128], *pos;
  pos= strcpy(buf, "COMMIT /* xid=")+14;
  pos= int64_t10_to_str(xid, pos, 10);
  pos= strcpy(pos, " */")+3;
  protocol->store(buf, (uint) (pos-buf), &my_charset_bin);
}

/**
  @note
  It's ok not to use int8store here,
  as long as XID::set(uint64_t) and
  XID::get_my_xid doesn't do it either.
  We don't care about actual values of xids as long as
  identical numbers compare identically
*/

Xid_log_event::
Xid_log_event(const char* buf,
              const Format_description_log_event *description_event)
  :Log_event(buf, description_event)
{
  buf+= description_event->common_header_len;
  memcpy(&xid, buf, sizeof(xid));
}


bool Xid_log_event::write(IO_CACHE* file)
{
  return write_header(file, sizeof(xid)) ||
         my_b_safe_write(file, (unsigned char*) &xid, sizeof(xid));
}


int Xid_log_event::do_apply_event(const Relay_log_info *)
{
  return end_trans(session, COMMIT);
}

Log_event::enum_skip_reason
Xid_log_event::do_shall_skip(Relay_log_info *rli)
{
  if (rli->slave_skip_counter > 0) {
    session->options&= ~OPTION_BEGIN;
    return(Log_event::EVENT_SKIP_COUNT);
  }
  return(Log_event::do_shall_skip(rli));
}


/**************************************************************************
  Slave_log_event methods
**************************************************************************/

void Slave_log_event::pack_info(Protocol *protocol)
{
  ostringstream stream;
  stream << "host=" << master_host << ",port=" << master_port;
  stream << ",log=" << master_log << ",pos=" << master_pos;

  protocol->store(stream.str().c_str(), stream.str().length(),
                  &my_charset_bin);
}


/**
  @todo
  re-write this better without holding both locks at the same time
*/
Slave_log_event::Slave_log_event(Session* session_arg,
				 Relay_log_info* rli)
  :Log_event(session_arg, 0, 0) , mem_pool(0), master_host(0)
{
  if (!rli->inited)				// QQ When can this happen ?
    return;

  Master_info* mi = rli->mi;
  // TODO: re-write this better without holding both locks at the same time
  pthread_mutex_lock(&mi->data_lock);
  pthread_mutex_lock(&rli->data_lock);
  // on OOM, just do not initialize the structure and print the error
  if ((mem_pool = (char*)malloc(get_data_size() + 1)))
  {
    master_host.assign(mi->getHostname());
    master_log.assign(rli->group_master_log_name);
    master_port = mi->getPort();
    master_pos = rli->group_master_log_pos;
  }
  else
    sql_print_error(_("Out of memory while recording slave event"));
  pthread_mutex_unlock(&rli->data_lock);
  pthread_mutex_unlock(&mi->data_lock);
  return;
}


Slave_log_event::~Slave_log_event()
{
  free(mem_pool);
}


int Slave_log_event::get_data_size()
{
  return master_host.length() + master_log.length() + 1 + SL_MASTER_HOST_OFFSET;
}


bool Slave_log_event::write(IO_CACHE* file)
{
  ulong event_length= get_data_size();
  int8store(mem_pool + SL_MASTER_POS_OFFSET, master_pos);
  int2store(mem_pool + SL_MASTER_PORT_OFFSET, master_port);
  // log and host are already there

  return (write_header(file, event_length) ||
          my_b_safe_write(file, (unsigned char*) mem_pool, event_length));
}


void Slave_log_event::init_from_mem_pool()
{
  master_pos = uint8korr(mem_pool + SL_MASTER_POS_OFFSET);
  master_port = uint2korr(mem_pool + SL_MASTER_PORT_OFFSET);
#ifdef FIXME
  /* Assign these correctly */
  master_host.assign(mem_pool + SL_MASTER_HOST_OFFSET);
  master_log.assign();
#endif
}


int Slave_log_event::do_apply_event(const Relay_log_info *)
{
  if (drizzle_bin_log.is_open())
    drizzle_bin_log.write(this);
  return 0;
}


/**************************************************************************
	Stop_log_event methods
**************************************************************************/

/*
  The master stopped.  We used to clean up all temporary tables but
  this is useless as, as the master has shut down properly, it has
  written all DROP TEMPORARY Table (prepared statements' deletion is
  TODO only when we binlog prep stmts).  We used to clean up
  slave_load_tmpdir, but this is useless as it has been cleared at the
  end of LOAD DATA INFILE.  So we have nothing to do here.  The place
  were we must do this cleaning is in
  Start_log_event_v3::do_apply_event(), not here. Because if we come
  here, the master was sane.
*/
int Stop_log_event::do_update_pos(Relay_log_info *rli)
{
  /*
    We do not want to update master_log pos because we get a rotate event
    before stop, so by now group_master_log_name is set to the next log.
    If we updated it, we will have incorrect master coordinates and this
    could give false triggers in MASTER_POS_WAIT() that we have reached
    the target position when in fact we have not.
  */
  if (session->options & OPTION_BEGIN)
    rli->inc_event_relay_log_pos();
  else
  {
    rli->inc_group_relay_log_pos(0);
    flush_relay_log_info(rli);
  }
  return 0;
}


/**************************************************************************
	Create_file_log_event methods
**************************************************************************/

/*
  Create_file_log_event ctor
*/

Create_file_log_event::
Create_file_log_event(Session* session_arg, sql_exchange* ex,
		      const char* db_arg, const char* table_name_arg,
		      List<Item>& fields_arg, enum enum_duplicates handle_dup,
                      bool ignore,
		      unsigned char* block_arg, uint32_t block_len_arg, bool using_trans)
  :Load_log_event(session_arg,ex,db_arg,table_name_arg,fields_arg,handle_dup, ignore,
		  using_trans),
   fake_base(0), block(block_arg), event_buf(0), block_len(block_len_arg),
   file_id(session_arg->file_id = drizzle_bin_log.next_file_id())
{
  sql_ex.force_new_format();
  return;
}


/*
  Create_file_log_event::write_data_body()
*/

bool Create_file_log_event::write_data_body(IO_CACHE* file)
{
  bool res;
  if ((res= Load_log_event::write_data_body(file)) || fake_base)
    return res;
  return (my_b_safe_write(file, (unsigned char*) "", 1) ||
          my_b_safe_write(file, (unsigned char*) block, block_len));
}


/*
  Create_file_log_event::write_data_header()
*/

bool Create_file_log_event::write_data_header(IO_CACHE* file)
{
  bool res;
  unsigned char buf[CREATE_FILE_HEADER_LEN];
  if ((res= Load_log_event::write_data_header(file)) || fake_base)
    return res;
  int4store(buf + CF_FILE_ID_OFFSET, file_id);
  return my_b_safe_write(file, buf, CREATE_FILE_HEADER_LEN) != 0;
}


/*
  Create_file_log_event::write_base()
*/

bool Create_file_log_event::write_base(IO_CACHE* file)
{
  bool res;
  fake_base= 1;                                 // pretend we are Load event
  res= write(file);
  fake_base= 0;
  return res;
}

/*
  Create_file_log_event ctor
*/

Create_file_log_event::Create_file_log_event(const char* buf, uint32_t len,
                                             const Format_description_log_event* description_event)
  :Load_log_event(buf,0,description_event),fake_base(0),block(0),inited_from_old(0)
{
  uint32_t block_offset;
  uint32_t header_len= description_event->common_header_len;
  uint8_t load_header_len= description_event->post_header_len[LOAD_EVENT-1];
  uint8_t create_file_header_len= description_event->post_header_len[CREATE_FILE_EVENT-1];
  if (!(event_buf= (const char*)malloc(len)) ||
      memcpy((char *)event_buf, buf, len) ||
      copy_log_event(event_buf,len,
                     ((buf[EVENT_TYPE_OFFSET] == LOAD_EVENT) ?
                      load_header_len + header_len :
                      (fake_base ? (header_len+load_header_len) :
                       (header_len+load_header_len) +
                       create_file_header_len)),
                     description_event))
    return;
  if (description_event->binlog_version!=1)
  {
    file_id= uint4korr(buf +
                       header_len +
		       load_header_len + CF_FILE_ID_OFFSET);
    /*
      Note that it's ok to use get_data_size() below, because it is computed
      with values we have already read from this event (because we called
      copy_log_event()); we are not using slave's format info to decode
      master's format, we are really using master's format info.
      Anyway, both formats should be identical (except the common_header_len)
      as these Load events are not changed between 4.0 and 5.0 (as logging of
      LOAD DATA INFILE does not use Load_log_event in 5.0).

      The + 1 is for \0 terminating fname
    */
    block_offset= (description_event->common_header_len +
                   Load_log_event::get_data_size() +
                   create_file_header_len + 1);
    if (len < block_offset)
      return;
    block = (unsigned char*)buf + block_offset;
    block_len = len - block_offset;
  }
  else
  {
    sql_ex.force_new_format();
    inited_from_old = 1;
  }
  return;
}


/*
  Create_file_log_event::pack_info()
*/

void Create_file_log_event::pack_info(Protocol *protocol)
{
  char buf[NAME_LEN*2 + 30 + 21*2], *pos;
  pos= strcpy(buf, "db=")+3;
  memcpy(pos, db, db_len);
  pos= strcpy(pos + db_len, ";table=")+7;
  memcpy(pos, table_name, table_name_len);
  pos= strcpy(pos + table_name_len, ";file_id=")+9;
  pos= int10_to_str((long) file_id, pos, 10);
  pos= strcpy(pos, ";block_len=")+11;
  pos= int10_to_str((long) block_len, pos, 10);
  protocol->store(buf, (uint) (pos-buf), &my_charset_bin);
}


/*
  Create_file_log_event::do_apply_event()
*/

int Create_file_log_event::do_apply_event(Relay_log_info const *rli)
{
  char proc_info[17+FN_REFLEN+10], *fname_buf;
  char *ext;
  int fd = -1;
  IO_CACHE file;
  int error = 1;

  memset(&file, 0, sizeof(file));
  fname_buf= strcpy(proc_info, "Making temp file ")+17;
  ext= slave_load_file_stem(fname_buf, file_id, server_id, ".info");
  session->set_proc_info(proc_info);
  my_delete(fname_buf, MYF(0)); // old copy may exist already
  if ((fd= my_create(fname_buf, CREATE_MODE,
		     O_WRONLY | O_EXCL,
		     MYF(MY_WME))) < 0 ||
      init_io_cache(&file, fd, IO_SIZE, WRITE_CACHE, (my_off_t)0, 0,
		    MYF(MY_WME|MY_NABP)))
  {
    rli->report(ERROR_LEVEL, my_errno,
                _("Error in Create_file event: could not open file '%s'"),
                fname_buf);
    goto err;
  }

  // a trick to avoid allocating another buffer
  fname= fname_buf;
  fname_len= (uint) ((strcpy(ext, ".data") + 5) - fname);
  if (write_base(&file))
  {
    strcpy(ext, ".info"); // to have it right in the error message
    rli->report(ERROR_LEVEL, my_errno,
                _("Error in Create_file event: could not write to file '%s'"),
                fname_buf);
    goto err;
  }
  end_io_cache(&file);
  my_close(fd, MYF(0));

  // fname_buf now already has .data, not .info, because we did our trick
  my_delete(fname_buf, MYF(0)); // old copy may exist already
  if ((fd= my_create(fname_buf, CREATE_MODE,
                     O_WRONLY | O_EXCL,
                     MYF(MY_WME))) < 0)
  {
    rli->report(ERROR_LEVEL, my_errno,
                _("Error in Create_file event: could not open file '%s'"),
                fname_buf);
    goto err;
  }
  if (my_write(fd, (unsigned char*) block, block_len, MYF(MY_WME+MY_NABP)))
  {
    rli->report(ERROR_LEVEL, my_errno,
                _("Error in Create_file event: write to '%s' failed"),
                fname_buf);
    goto err;
  }
  error=0;					// Everything is ok

err:
  if (error)
    end_io_cache(&file);
  if (fd >= 0)
    my_close(fd, MYF(0));
  session->set_proc_info(0);
  return error == 0;
}


/**************************************************************************
	Append_block_log_event methods
**************************************************************************/

/*
  Append_block_log_event ctor
*/

Append_block_log_event::Append_block_log_event(Session *session_arg,
                                               const char *db_arg,
					       unsigned char *block_arg,
					       uint32_t block_len_arg,
					       bool using_trans)
  :Log_event(session_arg,0, using_trans), block(block_arg),
   block_len(block_len_arg), file_id(session_arg->file_id), db(db_arg)
{
}


/*
  Append_block_log_event ctor
*/

Append_block_log_event::Append_block_log_event(const char* buf, uint32_t len,
                                               const Format_description_log_event* description_event)
  :Log_event(buf, description_event),block(0)
{
  uint8_t common_header_len= description_event->common_header_len;
  uint8_t append_block_header_len=
    description_event->post_header_len[APPEND_BLOCK_EVENT-1];
  uint32_t total_header_len= common_header_len+append_block_header_len;
  if (len < total_header_len)
    return;
  file_id= uint4korr(buf + common_header_len + AB_FILE_ID_OFFSET);
  block= (unsigned char*)buf + total_header_len;
  block_len= len - total_header_len;
  return;
}


/*
  Append_block_log_event::write()
*/

bool Append_block_log_event::write(IO_CACHE* file)
{
  unsigned char buf[APPEND_BLOCK_HEADER_LEN];
  int4store(buf + AB_FILE_ID_OFFSET, file_id);
  return (write_header(file, APPEND_BLOCK_HEADER_LEN + block_len) ||
          my_b_safe_write(file, buf, APPEND_BLOCK_HEADER_LEN) ||
	  my_b_safe_write(file, (unsigned char*) block, block_len));
}


/*
  Append_block_log_event::pack_info()
*/

void Append_block_log_event::pack_info(Protocol *protocol)
{
  char buf[256];
  uint32_t length;
  length= (uint) sprintf(buf, ";file_id=%u;block_len=%u", file_id,
			     block_len);
  protocol->store(buf, length, &my_charset_bin);
}


/*
  Append_block_log_event::get_create_or_append()
*/

int Append_block_log_event::get_create_or_append() const
{
  return 0; /* append to the file, fail if not exists */
}

/*
  Append_block_log_event::do_apply_event()
*/

int Append_block_log_event::do_apply_event(Relay_log_info const *rli)
{
  char proc_info[17+FN_REFLEN+10], *fname= proc_info+17;
  int fd;
  int error = 1;

  fname= strcpy(proc_info, "Making temp file ")+17;
  slave_load_file_stem(fname, file_id, server_id, ".data");
  session->set_proc_info(proc_info);
  if (get_create_or_append())
  {
    my_delete(fname, MYF(0)); // old copy may exist already
    if ((fd= my_create(fname, CREATE_MODE,
		       O_WRONLY | O_EXCL,
		       MYF(MY_WME))) < 0)
    {
      rli->report(ERROR_LEVEL, my_errno,
                  _("Error in %s event: could not create file '%s'"),
                  get_type_str(), fname);
      goto err;
    }
  }
  else if ((fd = my_open(fname, O_WRONLY | O_APPEND,
                         MYF(MY_WME))) < 0)
  {
    rli->report(ERROR_LEVEL, my_errno,
                _("Error in %s event: could not open file '%s'"),
                get_type_str(), fname);
    goto err;
  }
  if (my_write(fd, (unsigned char*) block, block_len, MYF(MY_WME+MY_NABP)))
  {
    rli->report(ERROR_LEVEL, my_errno,
                _("Error in %s event: write to '%s' failed"),
                get_type_str(), fname);
    goto err;
  }
  error=0;

err:
  if (fd >= 0)
    my_close(fd, MYF(0));
  session->set_proc_info(0);
  return(error);
}


/**************************************************************************
	Delete_file_log_event methods
**************************************************************************/

/*
  Delete_file_log_event ctor
*/

Delete_file_log_event::Delete_file_log_event(Session *session_arg, const char* db_arg,
					     bool using_trans)
  :Log_event(session_arg, 0, using_trans), file_id(session_arg->file_id), db(db_arg)
{
}

/*
  Delete_file_log_event ctor
*/

Delete_file_log_event::Delete_file_log_event(const char* buf, uint32_t len,
                                             const Format_description_log_event* description_event)
  :Log_event(buf, description_event),file_id(0)
{
  uint8_t common_header_len= description_event->common_header_len;
  uint8_t delete_file_header_len= description_event->post_header_len[DELETE_FILE_EVENT-1];
  if (len < (uint)(common_header_len + delete_file_header_len))
    return;
  file_id= uint4korr(buf + common_header_len + DF_FILE_ID_OFFSET);
}


/*
  Delete_file_log_event::write()
*/

bool Delete_file_log_event::write(IO_CACHE* file)
{
 unsigned char buf[DELETE_FILE_HEADER_LEN];
 int4store(buf + DF_FILE_ID_OFFSET, file_id);
 return (write_header(file, sizeof(buf)) ||
         my_b_safe_write(file, buf, sizeof(buf)));
}


/*
  Delete_file_log_event::pack_info()
*/

void Delete_file_log_event::pack_info(Protocol *protocol)
{
  char buf[64];
  uint32_t length;
  length= (uint) sprintf(buf, ";file_id=%u", (uint) file_id);
  protocol->store(buf, (int32_t) length, &my_charset_bin);
}

/*
  Delete_file_log_event::do_apply_event()
*/

int Delete_file_log_event::do_apply_event(const Relay_log_info *)
{
  char fname[FN_REFLEN+10];
  char *ext= slave_load_file_stem(fname, file_id, server_id, ".data");
  (void) my_delete(fname, MYF(MY_WME));
  strcpy(ext, ".info");
  (void) my_delete(fname, MYF(MY_WME));
  return 0;
}


/**************************************************************************
	Execute_load_log_event methods
**************************************************************************/

/*
  Execute_load_log_event ctor
*/

Execute_load_log_event::Execute_load_log_event(Session *session_arg,
                                               const char* db_arg,
					       bool using_trans)
  :Log_event(session_arg, 0, using_trans), file_id(session_arg->file_id), db(db_arg)
{
}


/*
  Execute_load_log_event ctor
*/

Execute_load_log_event::Execute_load_log_event(const char* buf, uint32_t len,
                                               const Format_description_log_event* description_event)
  :Log_event(buf, description_event), file_id(0)
{
  uint8_t common_header_len= description_event->common_header_len;
  uint8_t exec_load_header_len= description_event->post_header_len[EXEC_LOAD_EVENT-1];
  if (len < (uint)(common_header_len+exec_load_header_len))
    return;
  file_id= uint4korr(buf + common_header_len + EL_FILE_ID_OFFSET);
}


/*
  Execute_load_log_event::write()
*/

bool Execute_load_log_event::write(IO_CACHE* file)
{
  unsigned char buf[EXEC_LOAD_HEADER_LEN];
  int4store(buf + EL_FILE_ID_OFFSET, file_id);
  return (write_header(file, sizeof(buf)) ||
          my_b_safe_write(file, buf, sizeof(buf)));
}


/*
  Execute_load_log_event::pack_info()
*/

void Execute_load_log_event::pack_info(Protocol *protocol)
{
  char buf[64];
  uint32_t length;
  length= (uint) sprintf(buf, ";file_id=%u", (uint) file_id);
  protocol->store(buf, (int32_t) length, &my_charset_bin);
}


/*
  Execute_load_log_event::do_apply_event()
*/

int Execute_load_log_event::do_apply_event(Relay_log_info const *rli)
{
  char fname[FN_REFLEN+10];
  char *ext;
  int fd;
  int error= 1;
  IO_CACHE file;
  Load_log_event *lev= 0;

  ext= slave_load_file_stem(fname, file_id, server_id, ".info");
  if ((fd = my_open(fname, O_RDONLY,
                    MYF(MY_WME))) < 0 ||
      init_io_cache(&file, fd, IO_SIZE, READ_CACHE, (my_off_t)0, 0,
		    MYF(MY_WME|MY_NABP)))
  {
    rli->report(ERROR_LEVEL, my_errno,
                _("Error in Exec_load event: could not open file '%s'"),
                fname);
    goto err;
  }
  if (!(lev = (Load_log_event*)Log_event::read_log_event(&file,
                                                         (pthread_mutex_t*)0,
                                                         rli->relay_log.description_event_for_exec)) ||
      lev->get_type_code() != NEW_LOAD_EVENT)
  {
    rli->report(ERROR_LEVEL, 0,
                _("Error in Exec_load event: "
                  "file '%s' appears corrupted"),
                fname);
    goto err;
  }

  lev->session = session;
  /*
    lev->do_apply_event should use rli only for errors i.e. should
    not advance rli's position.

    lev->do_apply_event is the place where the table is loaded (it
    calls mysql_load()).
  */

  const_cast<Relay_log_info*>(rli)->future_group_master_log_pos= log_pos;
  if (lev->do_apply_event(0,rli,1))
  {
    /*
      We want to indicate the name of the file that could not be loaded
      (SQL_LOADxxx).
      But as we are here we are sure the error is in rli->last_slave_error and
      rli->last_slave_errno (example of error: duplicate entry for key), so we
      don't want to overwrite it with the filename.
      What we want instead is add the filename to the current error message.
    */
    char *tmp= strdup(rli->last_error().message);
    if (tmp)
    {
      rli->report(ERROR_LEVEL, rli->last_error().number,
                  _("%s. Failed executing load from '%s'"),
                  tmp, fname);
      free(tmp);
    }
    goto err;
  }
  /*
    We have an open file descriptor to the .info file; we need to close it
    or Windows will refuse to delete the file in my_delete().
  */
  if (fd >= 0)
  {
    my_close(fd, MYF(0));
    end_io_cache(&file);
    fd= -1;
  }
  (void) my_delete(fname, MYF(MY_WME));
  memcpy(ext, ".data", 6);
  (void) my_delete(fname, MYF(MY_WME));
  error = 0;

err:
  delete lev;
  if (fd >= 0)
  {
    my_close(fd, MYF(0));
    end_io_cache(&file);
  }
  return error;
}


/**************************************************************************
	Begin_load_query_log_event methods
**************************************************************************/

Begin_load_query_log_event::
Begin_load_query_log_event(Session* session_arg, const char* db_arg, unsigned char* block_arg,
                           uint32_t block_len_arg, bool using_trans)
  :Append_block_log_event(session_arg, db_arg, block_arg, block_len_arg,
                          using_trans)
{
   file_id= session_arg->file_id= drizzle_bin_log.next_file_id();
}


Begin_load_query_log_event::
Begin_load_query_log_event(const char* buf, uint32_t len,
                           const Format_description_log_event* desc_event)
  :Append_block_log_event(buf, len, desc_event)
{
}


int Begin_load_query_log_event::get_create_or_append() const
{
  return 1; /* create the file */
}


Log_event::enum_skip_reason
Begin_load_query_log_event::do_shall_skip(Relay_log_info *rli)
{
  /*
    If the slave skip counter is 1, then we should not start executing
    on the next event.
  */
  return continue_group(rli);
}


/**************************************************************************
	Execute_load_query_log_event methods
**************************************************************************/


Execute_load_query_log_event::
Execute_load_query_log_event(Session *session_arg, const char* query_arg,
                             ulong query_length_arg, uint32_t fn_pos_start_arg,
                             uint32_t fn_pos_end_arg,
                             enum_load_dup_handling dup_handling_arg,
                             bool using_trans, bool suppress_use,
                             Session::killed_state killed_err_arg):
  Query_log_event(session_arg, query_arg, query_length_arg, using_trans,
                  suppress_use, killed_err_arg),
  file_id(session_arg->file_id), fn_pos_start(fn_pos_start_arg),
  fn_pos_end(fn_pos_end_arg), dup_handling(dup_handling_arg)
{
}


Execute_load_query_log_event::
Execute_load_query_log_event(const char* buf, uint32_t event_len,
                             const Format_description_log_event* desc_event):
  Query_log_event(buf, event_len, desc_event, EXECUTE_LOAD_QUERY_EVENT),
  file_id(0), fn_pos_start(0), fn_pos_end(0)
{
  if (!Query_log_event::is_valid())
    return;

  buf+= desc_event->common_header_len;

  fn_pos_start= uint4korr(buf + ELQ_FN_POS_START_OFFSET);
  fn_pos_end= uint4korr(buf + ELQ_FN_POS_END_OFFSET);
  dup_handling= (enum_load_dup_handling)(*(buf + ELQ_DUP_HANDLING_OFFSET));

  if (fn_pos_start > q_len || fn_pos_end > q_len ||
      dup_handling > LOAD_DUP_REPLACE)
    return;

  file_id= uint4korr(buf + ELQ_FILE_ID_OFFSET);
}


ulong Execute_load_query_log_event::get_post_header_size_for_derived()
{
  return EXECUTE_LOAD_QUERY_EXTRA_HEADER_LEN;
}


bool
Execute_load_query_log_event::write_post_header_for_derived(IO_CACHE* file)
{
  unsigned char buf[EXECUTE_LOAD_QUERY_EXTRA_HEADER_LEN];
  int4store(buf, file_id);
  int4store(buf + 4, fn_pos_start);
  int4store(buf + 4 + 4, fn_pos_end);
  *(buf + 4 + 4 + 4)= (unsigned char) dup_handling;
  return my_b_safe_write(file, buf, EXECUTE_LOAD_QUERY_EXTRA_HEADER_LEN);
}


void Execute_load_query_log_event::pack_info(Protocol *protocol)
{
  char *buf, *pos;
  if (!(buf= (char*) malloc(9 + db_len + q_len + 10 + 21)))
    return;
  pos= buf;
  if (db && db_len)
  {
    pos= strcpy(buf, "use `")+5;
    memcpy(pos, db, db_len);
    pos= strcpy(pos+db_len, "`; ")+3;
  }
  if (query && q_len)
  {
    memcpy(pos, query, q_len);
    pos+= q_len;
  }
  pos= strcpy(pos, " ;file_id=")+10;
  pos= int10_to_str((long) file_id, pos, 10);
  protocol->store(buf, pos-buf, &my_charset_bin);
  free(buf);
}


int
Execute_load_query_log_event::do_apply_event(Relay_log_info const *rli)
{
  char *p;
  char *buf;
  char *fname;
  char *fname_end;
  int error;

  buf= (char*) malloc(q_len + 1 - (fn_pos_end - fn_pos_start) +
                      (FN_REFLEN + 10) + 10 + 8 + 5);

  /* Replace filename and LOCAL keyword in query before executing it */
  if (buf == NULL)
  {
    rli->report(ERROR_LEVEL, ER_SLAVE_FATAL_ERROR,
                ER(ER_SLAVE_FATAL_ERROR),
                _("Not enough memory"));
    return 1;
  }

  p= buf;
  memcpy(p, query, fn_pos_start);
  p+= fn_pos_start;
  fname= (p= strncpy(p, STRING_WITH_LEN(" INFILE \'")) + 9);
  p= slave_load_file_stem(p, file_id, server_id, ".data");
  fname_end= p= strchr(p, '\0');                      // Safer than p=p+5
  *(p++)='\'';
  switch (dup_handling) {
  case LOAD_DUP_IGNORE:
    p= strncpy(p, STRING_WITH_LEN(" IGNORE")) + 7;
    break;
  case LOAD_DUP_REPLACE:
    p= strncpy(p, STRING_WITH_LEN(" REPLACE")) + 8;
    break;
  default:
    /* Ordinary load data */
    break;
  }
  size_t end_len = q_len-fn_pos_end;
  p= strncpy(p, STRING_WITH_LEN(" INTO")) + 5;
  p= strncpy(p, query+fn_pos_end, end_len);
  p+= end_len;

  error= Query_log_event::do_apply_event(rli, buf, p-buf);

  /* Forging file name for deletion in same buffer */
  *fname_end= 0;

  /*
    If there was an error the slave is going to stop, leave the
    file so that we can re-execute this event at START SLAVE.
  */
  if (!error)
    (void) my_delete(fname, MYF(MY_WME));

  free(buf);
  return error;
}


/**************************************************************************
	sql_ex_info methods
**************************************************************************/

/*
  sql_ex_info::write_data()
*/

bool sql_ex_info::write_data(IO_CACHE* file)
{
  if (new_format())
  {
    return (write_str(file, field_term, (uint) field_term_len) ||
	    write_str(file, enclosed,   (uint) enclosed_len) ||
	    write_str(file, line_term,  (uint) line_term_len) ||
	    write_str(file, line_start, (uint) line_start_len) ||
	    write_str(file, escaped,    (uint) escaped_len) ||
	    my_b_safe_write(file,(unsigned char*) &opt_flags,1));
  }
  else
    assert(0);
  return true;
}


/*
  sql_ex_info::init()
*/

const char *sql_ex_info::init(const char *buf, const char *buf_end,
                              bool use_new_format)
{
  cached_new_format = use_new_format;
  if (use_new_format)
  {
    empty_flags=0;
    /*
      The code below assumes that buf will not disappear from
      under our feet during the lifetime of the event. This assumption
      holds true in the slave thread if the log is in new format, but is not
      the case when we have old format because we will be reusing net buffer
      to read the actual file before we write out the Create_file event.
    */
    if (read_str(&buf, buf_end, &field_term, &field_term_len) ||
        read_str(&buf, buf_end, &enclosed,   &enclosed_len) ||
        read_str(&buf, buf_end, &line_term,  &line_term_len) ||
        read_str(&buf, buf_end, &line_start, &line_start_len) ||
        read_str(&buf, buf_end, &escaped,    &escaped_len))
      return 0;
    opt_flags = *buf++;
  }
  else
  {
    field_term_len= enclosed_len= line_term_len= line_start_len= escaped_len=1;
    field_term = buf++;			// Use first byte in string
    enclosed=	 buf++;
    line_term=   buf++;
    line_start=  buf++;
    escaped=     buf++;
    opt_flags =  *buf++;
    empty_flags= *buf++;
    if (empty_flags & FIELD_TERM_EMPTY)
      field_term_len=0;
    if (empty_flags & ENCLOSED_EMPTY)
      enclosed_len=0;
    if (empty_flags & LINE_TERM_EMPTY)
      line_term_len=0;
    if (empty_flags & LINE_START_EMPTY)
      line_start_len=0;
    if (empty_flags & ESCAPED_EMPTY)
      escaped_len=0;
  }
  return buf;
}


/**************************************************************************
	Rows_log_event member functions
**************************************************************************/

Rows_log_event::Rows_log_event(Session *session_arg, Table *tbl_arg, ulong tid,
                               MY_BITMAP const *cols, bool is_transactional)
  : Log_event(session_arg, 0, is_transactional),
    m_row_count(0),
    m_table(tbl_arg),
    m_table_id(tid),
    m_width(tbl_arg ? tbl_arg->s->fields : 1),
    m_rows_buf(0), m_rows_cur(0), m_rows_end(0), m_flags(0)
    , m_curr_row(NULL), m_curr_row_end(NULL), m_key(NULL)
{
  /*
    We allow a special form of dummy event when the table, and cols
    are null and the table id is UINT32_MAX.  This is a temporary
    solution, to be able to terminate a started statement in the
    binary log: the extraneous events will be removed in the future.
   */
  assert((tbl_arg && tbl_arg->s && tid != UINT32_MAX) || (!tbl_arg && !cols && tid == UINT32_MAX));

  if (session_arg->options & OPTION_NO_FOREIGN_KEY_CHECKS)
      set_flags(NO_FOREIGN_KEY_CHECKS_F);
  if (session_arg->options & OPTION_RELAXED_UNIQUE_CHECKS)
      set_flags(RELAXED_UNIQUE_CHECKS_F);
  /* if bitmap_init fails, caught in is_valid() */
  if (likely(!bitmap_init(&m_cols,
                          m_width <= sizeof(m_bitbuf)*8 ? m_bitbuf : NULL,
                          m_width,
                          false)))
  {
    /* Cols can be zero if this is a dummy binrows event */
    if (likely(cols != NULL))
    {
      memcpy(m_cols.bitmap, cols->bitmap, no_bytes_in_map(cols));
      create_last_word_mask(&m_cols);
    }
  }
  else
  {
    // Needed because bitmap_init() does not set it to null on failure
    m_cols.bitmap= 0;
  }
}


Rows_log_event::Rows_log_event(const char *buf, uint32_t event_len,
                               Log_event_type event_type,
                               const Format_description_log_event
                               *description_event)
  : Log_event(buf, description_event),
    m_row_count(0),
    m_table(NULL),
    m_table_id(0), m_rows_buf(0), m_rows_cur(0), m_rows_end(0)
    , m_curr_row(NULL), m_curr_row_end(NULL), m_key(NULL)
{
  uint8_t const common_header_len= description_event->common_header_len;
  uint8_t const post_header_len= description_event->post_header_len[event_type-1];

  const char *post_start= buf + common_header_len;
  post_start+= RW_MAPID_OFFSET;
  if (post_header_len == 6)
  {
    /* Master is of an intermediate source tree before 5.1.4. Id is 4 bytes */
    m_table_id= uint4korr(post_start);
    post_start+= 4;
  }
  else
  {
    m_table_id= (ulong) uint6korr(post_start);
    post_start+= RW_FLAGS_OFFSET;
  }

  m_flags= uint2korr(post_start);

  unsigned char const *const var_start=
    (const unsigned char *)buf + common_header_len + post_header_len;
  unsigned char const *const ptr_width= var_start;
  unsigned char *ptr_after_width= (unsigned char*) ptr_width;
  m_width = net_field_length(&ptr_after_width);
  /* if bitmap_init fails, catched in is_valid() */
  if (likely(!bitmap_init(&m_cols,
                          m_width <= sizeof(m_bitbuf)*8 ? m_bitbuf : NULL,
                          m_width,
                          false)))
  {
    memcpy(m_cols.bitmap, ptr_after_width, (m_width + 7) / 8);
    create_last_word_mask(&m_cols);
    ptr_after_width+= (m_width + 7) / 8;
  }
  else
  {
    // Needed because bitmap_init() does not set it to null on failure
    m_cols.bitmap= NULL;
    return;
  }

  m_cols_ai.bitmap= m_cols.bitmap; /* See explanation in is_valid() */

  if (event_type == UPDATE_ROWS_EVENT)
  {
    /* if bitmap_init fails, caught in is_valid() */
    if (likely(!bitmap_init(&m_cols_ai,
                            m_width <= sizeof(m_bitbuf_ai)*8 ? m_bitbuf_ai : NULL,
                            m_width,
                            false)))
    {
      memcpy(m_cols_ai.bitmap, ptr_after_width, (m_width + 7) / 8);
      create_last_word_mask(&m_cols_ai);
      ptr_after_width+= (m_width + 7) / 8;
    }
    else
    {
      // Needed because bitmap_init() does not set it to null on failure
      m_cols_ai.bitmap= 0;
      return;
    }
  }

  const unsigned char* const ptr_rows_data= (const unsigned char*) ptr_after_width;

  size_t const data_size= event_len - (ptr_rows_data - (const unsigned char *) buf);

  m_rows_buf= (unsigned char*) malloc(data_size);
  if (likely((bool)m_rows_buf))
  {
    m_curr_row= m_rows_buf;
    m_rows_end= m_rows_buf + data_size;
    m_rows_cur= m_rows_end;
    memcpy(m_rows_buf, ptr_rows_data, data_size);
  }
  else
    m_cols.bitmap= 0; // to not free it

  return;
}

Rows_log_event::~Rows_log_event()
{
  if (m_cols.bitmap == m_bitbuf) // no malloc happened
    m_cols.bitmap= 0; // so no free in bitmap_free
  bitmap_free(&m_cols); // To pair with bitmap_init().
  free((unsigned char*)m_rows_buf);
}

int Rows_log_event::get_data_size()
{
  int const type_code= get_type_code();

  unsigned char buf[sizeof(m_width)+1];
  unsigned char *end= net_store_length(buf, (m_width + 7) / 8);

  int data_size= ROWS_HEADER_LEN;
  data_size+= no_bytes_in_map(&m_cols);
  data_size+= end - buf;

  if (type_code == UPDATE_ROWS_EVENT)
    data_size+= no_bytes_in_map(&m_cols_ai);

  data_size+= (m_rows_cur - m_rows_buf);
  return data_size;
}


int Rows_log_event::do_add_row_data(unsigned char *row_data, size_t length)
{
  /*
    When the table has a primary key, we would probably want, by default, to
    log only the primary key value instead of the entire "before image". This
    would save binlog space. TODO
  */

  /*
    If length is zero, there is nothing to write, so we just
    return. Note that this is not an optimization, since calling
    realloc() with size 0 means free().
   */
  if (length == 0)
  {
    m_row_count++;
    return(0);
  }

  assert(m_rows_buf <= m_rows_cur);
  assert(!m_rows_buf || (m_rows_end && m_rows_buf <= m_rows_end));
  assert(m_rows_cur <= m_rows_end);

  /* The cast will always work since m_rows_cur <= m_rows_end */
  if (static_cast<size_t>(m_rows_end - m_rows_cur) <= length)
  {
    size_t const block_size= 1024;
    const size_t cur_size= m_rows_cur - m_rows_buf;
    const size_t new_alloc=
        block_size * ((cur_size + length + block_size - 1) / block_size);

    unsigned char* new_buf= (unsigned char*)realloc(m_rows_buf, new_alloc);
    if (unlikely(!new_buf))
      return(HA_ERR_OUT_OF_MEM);

    /* If the memory moved, we need to move the pointers */
    if (new_buf != m_rows_buf)
    {
      m_rows_buf= new_buf;
      m_rows_cur= m_rows_buf + cur_size;
    }

    /*
       The end pointer should always be changed to point to the end of
       the allocated memory.
    */
    m_rows_end= m_rows_buf + new_alloc;
  }

  assert(m_rows_cur + length <= m_rows_end);
  memcpy(m_rows_cur, row_data, length);
  m_rows_cur+= length;
  m_row_count++;
  return(0);
}

int Rows_log_event::do_apply_event(Relay_log_info const *rli)
{
  int error= 0;
  /*
    If m_table_id == UINT32_MAX, then we have a dummy event that does not
    contain any data.  In that case, we just remove all tables in the
    tables_to_lock list, close the thread tables, and return with
    success.
   */
  if (m_table_id == UINT32_MAX)
  {
    /*
       This one is supposed to be set: just an extra check so that
       nothing strange has happened.
     */
    assert(get_flags(STMT_END_F));

    const_cast<Relay_log_info*>(rli)->clear_tables_to_lock();
    close_thread_tables(session);
    session->clear_error();
    return(0);
  }

  /*
    'session' has been set by exec_relay_log_event(), just before calling
    do_apply_event(). We still check here to prevent future coding
    errors.
  */
  assert(rli->sql_session == session);

  /*
    If there is no locks taken, this is the first binrow event seen
    after the table map events.  We should then lock all the tables
    used in the transaction and proceed with execution of the actual
    event.
  */
  if (!session->lock)
  {
    bool need_reopen= 1; /* To execute the first lap of the loop below */

    /*
      lock_tables() reads the contents of session->lex, so they must be
      initialized. Contrary to in
      Table_map_log_event::do_apply_event() we don't call
      mysql_init_query() as that may reset the binlog format.
    */
    lex_start(session);

    /*
      There are a few flags that are replicated with each row event.
      Make sure to set/clear them before executing the main body of
      the event.
    */
    if (get_flags(NO_FOREIGN_KEY_CHECKS_F))
        session->options|= OPTION_NO_FOREIGN_KEY_CHECKS;
    else
        session->options&= ~OPTION_NO_FOREIGN_KEY_CHECKS;

    if (get_flags(RELAXED_UNIQUE_CHECKS_F))
        session->options|= OPTION_RELAXED_UNIQUE_CHECKS;
    else
        session->options&= ~OPTION_RELAXED_UNIQUE_CHECKS;
    /* A small test to verify that objects have consistent types */
    assert(sizeof(session->options) == sizeof(OPTION_RELAXED_UNIQUE_CHECKS));


    while ((error= lock_tables(session, rli->tables_to_lock,
                               rli->tables_to_lock_count, &need_reopen)))
    {
      if (!need_reopen)
      {
        if (session->is_slave_error || session->is_fatal_error)
        {
          /*
            Error reporting borrowed from Query_log_event with many excessive
            simplifications (we don't honour --slave-skip-errors)
          */
          uint32_t actual_error= session->main_da.sql_errno();
          rli->report(ERROR_LEVEL, actual_error,
                      _("Error '%s' in %s event: when locking tables"),
                      (actual_error
                       ? session->main_da.message()
                       : _("unexpected success or fatal error")),
                      get_type_str());
          session->is_fatal_error= 1;
        }
        else
        {
          rli->report(ERROR_LEVEL, error,
                      _("Error in %s event: when locking tables"),
                      get_type_str());
        }
        const_cast<Relay_log_info*>(rli)->clear_tables_to_lock();
        return(error);
      }

      TableList *tables= rli->tables_to_lock;
      close_tables_for_reopen(session, &tables);

      uint32_t tables_count= rli->tables_to_lock_count;
      if ((error= open_tables(session, &tables, &tables_count, 0)))
      {
        if (session->is_slave_error || session->is_fatal_error)
        {
          /*
            Error reporting borrowed from Query_log_event with many excessive
            simplifications (we don't honour --slave-skip-errors)
          */
          uint32_t actual_error= session->main_da.sql_errno();
          rli->report(ERROR_LEVEL, actual_error,
                      _("Error '%s' on reopening tables"),
                      (actual_error
                       ? session->main_da.message()
                       : _("unexpected success or fatal error")));
          session->is_slave_error= 1;
        }
        const_cast<Relay_log_info*>(rli)->clear_tables_to_lock();
        return(error);
      }
    }

    /*
      When the open and locking succeeded, we check all tables to
      ensure that they still have the correct type.

      We can use a down cast here since we know that every table added
      to the tables_to_lock is a RPL_TableList.
    */

    {
      RPL_TableList *ptr= rli->tables_to_lock;
      for ( ; ptr ; ptr= static_cast<RPL_TableList*>(ptr->next_global))
      {
        if (ptr->m_tabledef.compatible_with(rli, ptr->table))
        {
          mysql_unlock_tables(session, session->lock);
          session->lock= 0;
          session->is_slave_error= 1;
          const_cast<Relay_log_info*>(rli)->clear_tables_to_lock();
          return(ERR_BAD_TABLE_DEF);
        }
      }
    }

    /*
      ... and then we add all the tables to the table map and remove
      them from tables to lock.

      We also invalidate the query cache for all the tables, since
      they will now be changed.

      TODO [/Matz]: Maybe the query cache should not be invalidated
      here? It might be that a table is not changed, even though it
      was locked for the statement.  We do know that each
      Rows_log_event contain at least one row, so after processing one
      Rows_log_event, we can invalidate the query cache for the
      associated table.
     */
    for (TableList *ptr= rli->tables_to_lock ; ptr ; ptr= ptr->next_global)
    {
      const_cast<Relay_log_info*>(rli)->m_table_map.set_table(ptr->table_id, ptr->table);
    }
  }

  Table*
    table=
    m_table= const_cast<Relay_log_info*>(rli)->m_table_map.get_table(m_table_id);

  if (table)
  {
    /*
      table == NULL means that this table should not be replicated
      (this was set up by Table_map_log_event::do_apply_event()
      which tested replicate-* rules).
    */

    /*
      It's not needed to set_time() but
      1) it continues the property that "Time" in SHOW PROCESSLIST shows how
      much slave is behind
      2) it will be needed when we allow replication from a table with no
      TIMESTAMP column to a table with one.
      So we call set_time(), like in SBR. Presently it changes nothing.
    */
    session->set_time((time_t)when);
    /*
      There are a few flags that are replicated with each row event.
      Make sure to set/clear them before executing the main body of
      the event.
    */
    if (get_flags(NO_FOREIGN_KEY_CHECKS_F))
        session->options|= OPTION_NO_FOREIGN_KEY_CHECKS;
    else
        session->options&= ~OPTION_NO_FOREIGN_KEY_CHECKS;

    if (get_flags(RELAXED_UNIQUE_CHECKS_F))
        session->options|= OPTION_RELAXED_UNIQUE_CHECKS;
    else
        session->options&= ~OPTION_RELAXED_UNIQUE_CHECKS;

    if (slave_allow_batching)
      session->options|= OPTION_ALLOW_BATCH;
    else
      session->options&= ~OPTION_ALLOW_BATCH;

    /* A small test to verify that objects have consistent types */
    assert(sizeof(session->options) == sizeof(OPTION_RELAXED_UNIQUE_CHECKS));

    /*
      Now we are in a statement and will stay in a statement until we
      see a STMT_END_F.

      We set this flag here, before actually applying any rows, in
      case the SQL thread is stopped and we need to detect that we're
      inside a statement and halting abruptly might cause problems
      when restarting.
     */
    const_cast<Relay_log_info*>(rli)->set_flag(Relay_log_info::IN_STMT);

     if ( m_width == table->s->fields && bitmap_is_set_all(&m_cols))
      set_flags(COMPLETE_ROWS_F);

    /*
      Set tables write and read sets.

      Read_set contains all slave columns (in case we are going to fetch
      a complete record from slave)

      Write_set equals the m_cols bitmap sent from master but it can be
      longer if slave has extra columns.
     */

    bitmap_set_all(table->read_set);
    bitmap_set_all(table->write_set);
    if (!get_flags(COMPLETE_ROWS_F))
      bitmap_intersect(table->write_set,&m_cols);

    this->slave_exec_mode= slave_exec_mode_options; // fix the mode

    // Do event specific preparations
    error= do_before_row_operations(rli);

    // row processing loop

    while (error == 0 && m_curr_row < m_rows_end)
    {
      /* in_use can have been set to NULL in close_tables_for_reopen */
      Session* old_session= table->in_use;
      if (!table->in_use)
        table->in_use= session;

      error= do_exec_row(rli);

      table->in_use = old_session;
      switch (error)
      {
      case 0:
	break;
      /*
        The following list of "idempotent" errors
        means that an error from the list might happen
        because of idempotent (more than once)
        applying of a binlog file.
        Notice, that binlog has a  ddl operation its
        second applying may cause

        case HA_ERR_TABLE_DEF_CHANGED:
        case HA_ERR_CANNOT_ADD_FOREIGN:

        which are not included into to the list.
      */
      case HA_ERR_RECORD_CHANGED:
      case HA_ERR_RECORD_DELETED:
      case HA_ERR_KEY_NOT_FOUND:
      case HA_ERR_END_OF_FILE:
      case HA_ERR_FOUND_DUPP_KEY:
      case HA_ERR_FOUND_DUPP_UNIQUE:
      case HA_ERR_FOREIGN_DUPLICATE_KEY:
      case HA_ERR_NO_REFERENCED_ROW:
      case HA_ERR_ROW_IS_REFERENCED:
        if (bit_is_set(slave_exec_mode, SLAVE_EXEC_MODE_IDEMPOTENT) == 1)
        {
          if (global_system_variables.log_warnings)
            slave_rows_error_report(WARNING_LEVEL, error, rli, session, table,
                                    get_type_str(),
                                    RPL_LOG_NAME, (ulong) log_pos);
          error= 0;
        }
        break;

      default:
	session->is_slave_error= 1;
	break;
      }

      /*
       If m_curr_row_end  was not set during event execution (e.g., because
       of errors) we can't proceed to the next row. If the error is transient
       (i.e., error==0 at this point) we must call unpack_current_row() to set
       m_curr_row_end.
      */
      if (!m_curr_row_end && !error)
        unpack_current_row(rli, &m_cols);

      // at this moment m_curr_row_end should be set
      assert(error || m_curr_row_end != NULL);
      assert(error || m_curr_row < m_curr_row_end);
      assert(error || m_curr_row_end <= m_rows_end);

      m_curr_row= m_curr_row_end;

    } // row processing loop

    error= do_after_row_operations(rli, error);
    if (!cache_stmt)
    {
      session->options|= OPTION_KEEP_LOG;
    }
  } // if (table)

  /*
    We need to delay this clear until here bacause unpack_current_row() uses
    master-side table definitions stored in rli.
  */
  if (rli->tables_to_lock && get_flags(STMT_END_F))
    const_cast<Relay_log_info*>(rli)->clear_tables_to_lock();
  /* reset OPTION_ALLOW_BATCH as not affect later events */
  session->options&= ~OPTION_ALLOW_BATCH;

  if (error)
  {                     /* error has occured during the transaction */
    slave_rows_error_report(ERROR_LEVEL, error, rli, session, table,
                            get_type_str(), RPL_LOG_NAME, (ulong) log_pos);
  }
  if (error)
  {
    /*
      If one day we honour --skip-slave-errors in row-based replication, and
      the error should be skipped, then we would clear mappings, rollback,
      close tables, but the slave SQL thread would not stop and then may
      assume the mapping is still available, the tables are still open...
      So then we should clear mappings/rollback/close here only if this is a
      STMT_END_F.
      For now we code, knowing that error is not skippable and so slave SQL
      thread is certainly going to stop.
      rollback at the caller along with sbr.
    */
    const_cast<Relay_log_info*>(rli)->cleanup_context(session, error);
    session->is_slave_error= 1;
    return(error);
  }

  /*
    This code would ideally be placed in do_update_pos() instead, but
    since we have no access to table there, we do the setting of
    last_event_start_time here instead.
  */
  if (table && (table->s->primary_key == MAX_KEY) &&
      !cache_stmt && get_flags(STMT_END_F) == RLE_NO_FLAGS)
  {
    /*
      ------------ Temporary fix until WL#2975 is implemented ---------

      This event is not the last one (no STMT_END_F). If we stop now
      (in case of terminate_slave_thread()), how will we restart? We
      have to restart from Table_map_log_event, but as this table is
      not transactional, the rows already inserted will still be
      present, and idempotency is not guaranteed (no PK) so we risk
      that repeating leads to double insert. So we desperately try to
      continue, hope we'll eventually leave this buggy situation (by
      executing the final Rows_log_event). If we are in a hopeless
      wait (reached end of last relay log and nothing gets appended
      there), we timeout after one minute, and notify DBA about the
      problem.  When WL#2975 is implemented, just remove the member
      Relay_log_info::last_event_start_time and all its occurrences.
    */
    const_cast<Relay_log_info*>(rli)->last_event_start_time= my_time(0);
  }

  return(0);
}

Log_event::enum_skip_reason
Rows_log_event::do_shall_skip(Relay_log_info *rli)
{
  /*
    If the slave skip counter is 1 and this event does not end a
    statement, then we should not start executing on the next event.
    Otherwise, we defer the decision to the normal skipping logic.
  */
  if (rli->slave_skip_counter == 1 && !get_flags(STMT_END_F))
    return Log_event::EVENT_SKIP_IGNORE;
  else
    return Log_event::do_shall_skip(rli);
}

int
Rows_log_event::do_update_pos(Relay_log_info *rli)
{
  int error= 0;

  if (get_flags(STMT_END_F))
  {
    /*
      If this event is not in a transaction, the call below will, if some
      transactional storage engines are involved, commit the statement into
      them and flush the pending event to binlog.
      If this event is in a transaction, the call will do nothing, but a
      Xid_log_event will come next which will, if some transactional engines
      are involved, commit the transaction and flush the pending event to the
      binlog.
    */
    error= ha_autocommit_or_rollback(session, 0);

    /*
      Now what if this is not a transactional engine? we still need to
      flush the pending event to the binlog; we did it with
      session->binlog_flush_pending_rows_event(). Note that we imitate
      what is done for real queries: a call to
      ha_autocommit_or_rollback() (sometimes only if involves a
      transactional engine), and a call to be sure to have the pending
      event flushed.
    */

    rli->cleanup_context(session, 0);
    if (error == 0)
    {
      /*
        Indicate that a statement is finished.
        Step the group log position if we are not in a transaction,
        otherwise increase the event log position.
       */
      rli->stmt_done(log_pos, when);

      /*
        Clear any errors pushed in session->net.last_err* if for example "no key
        found" (as this is allowed). This is a safety measure; apparently
        those errors (e.g. when executing a Delete_rows_log_event of a
        non-existing row, like in rpl_row_mystery22.test,
        session->net.last_error = "Can't find record in 't1'" and last_errno=1032)
        do not become visible. We still prefer to wipe them out.
      */
      session->clear_error();
    }
    else
      rli->report(ERROR_LEVEL, error,
                  _("Error in %s event: commit of row events failed, "
                    "table `%s`.`%s`"),
                  get_type_str(), m_table->s->db.str,
                  m_table->s->table_name.str);
  }
  else
  {
    rli->inc_event_relay_log_pos();
  }

  return(error);
}

bool Rows_log_event::write_data_header(IO_CACHE *file)
{
  unsigned char buf[ROWS_HEADER_LEN];	// No need to init the buffer
  assert(m_table_id != UINT32_MAX);
  int6store(buf + RW_MAPID_OFFSET, (uint64_t)m_table_id);
  int2store(buf + RW_FLAGS_OFFSET, m_flags);
  return (my_b_safe_write(file, buf, ROWS_HEADER_LEN));
}

bool Rows_log_event::write_data_body(IO_CACHE*file)
{
  /*
     Note that this should be the number of *bits*, not the number of
     bytes.
  */
  unsigned char sbuf[sizeof(m_width)];
  my_ptrdiff_t const data_size= m_rows_cur - m_rows_buf;
  bool res= false;
  unsigned char *const sbuf_end= net_store_length(sbuf, (size_t) m_width);
  assert(static_cast<size_t>(sbuf_end - sbuf) <= sizeof(sbuf));

  res= res || my_b_safe_write(file, sbuf, (size_t) (sbuf_end - sbuf));

  res= res || my_b_safe_write(file, (unsigned char*) m_cols.bitmap,
                              no_bytes_in_map(&m_cols));
  /*
    TODO[refactor write]: Remove the "down cast" here (and elsewhere).
   */
  if (get_type_code() == UPDATE_ROWS_EVENT)
  {
    res= res || my_b_safe_write(file, (unsigned char*) m_cols_ai.bitmap,
                                no_bytes_in_map(&m_cols_ai));
  }
  res= res || my_b_safe_write(file, m_rows_buf, (size_t) data_size);

  return res;

}


void Rows_log_event::pack_info(Protocol *protocol)
{
  char buf[256];
  char const *const flagstr=
    get_flags(STMT_END_F) ? " flags: STMT_END_F" : "";
  size_t bytes= snprintf(buf, sizeof(buf),
                         "table_id: %lu%s", m_table_id, flagstr);
  protocol->store(buf, bytes, &my_charset_bin);
}


/**************************************************************************
	Table_map_log_event member functions and support functions
**************************************************************************/

/**
  @page How replication of field metadata works.

  When a table map is created, the master first calls
  Table_map_log_event::save_field_metadata() which calculates how many
  values will be in the field metadata. Only those fields that require the
  extra data are added. The method also loops through all of the fields in
  the table calling the method Field::save_field_metadata() which returns the
  values for the field that will be saved in the metadata and replicated to
  the slave. Once all fields have been processed, the table map is written to
  the binlog adding the size of the field metadata and the field metadata to
  the end of the body of the table map.

  When a table map is read on the slave, the field metadata is read from the
  table map and passed to the table_def class constructor which saves the
  field metadata from the table map into an array based on the type of the
  field. Field metadata values not present (those fields that do not use extra
  data) in the table map are initialized as zero (0). The array size is the
  same as the columns for the table on the slave.

  Additionally, values saved for field metadata on the master are saved as a
  string of bytes (unsigned char) in the binlog. A field may require 1 or more bytes
  to store the information. In cases where values require multiple bytes
  (e.g. values > 255), the endian-safe methods are used to properly encode
  the values on the master and decode them on the slave. When the field
  metadata values are captured on the slave, they are stored in an array of
  type uint16_t. This allows the least number of casts to prevent casting bugs
  when the field metadata is used in comparisons of field attributes. When
  the field metadata is used for calculating addresses in pointer math, the
  type used is uint32_t.
*/

/**
  Save the field metadata based on the real_type of the field.
  The metadata saved depends on the type of the field. Some fields
  store a single byte for pack_length() while others store two bytes
  for field_length (max length).

  @retval  0  Ok.

  @todo
  We may want to consider changing the encoding of the information.
  Currently, the code attempts to minimize the number of bytes written to
  the tablemap. There are at least two other alternatives; 1) using
  net_store_length() to store the data allowing it to choose the number of
  bytes that are appropriate thereby making the code much easier to
  maintain (only 1 place to change the encoding), or 2) use a fixed number
  of bytes for each field. The problem with option 1 is that net_store_length()
  will use one byte if the value < 251, but 3 bytes if it is > 250. Thus,
  for fields like CHAR which can be no larger than 255 characters, the method
  will use 3 bytes when the value is > 250. Further, every value that is
  encoded using 2 parts (e.g., pack_length, field_length) will be numerically
  > 250 therefore will use 3 bytes for eah value. The problem with option 2
  is less wasteful for space but does waste 1 byte for every field that does
  not encode 2 parts.
*/
int Table_map_log_event::save_field_metadata()
{
  int index= 0;
  for (unsigned int i= 0 ; i < m_table->s->fields ; i++)
    index+= m_table->s->field[i]->save_field_metadata(&m_field_metadata[index]);
  return(index);
}

/*
  Constructor used to build an event for writing to the binary log.
  Mats says tbl->s lives longer than this event so it's ok to copy pointers
  (tbl->s->db etc) and not pointer content.
 */
Table_map_log_event::Table_map_log_event(Session *session, Table *tbl,
                                         ulong tid, bool, uint16_t flags)
  : Log_event(session, 0, true),
    m_table(tbl),
    m_dbnam(tbl->s->db.str),
    m_dblen(m_dbnam ? tbl->s->db.length : 0),
    m_tblnam(tbl->s->table_name.str),
    m_tbllen(tbl->s->table_name.length),
    m_colcnt(tbl->s->fields),
    m_memory(NULL),
    m_table_id(tid),
    m_flags(flags),
    m_data_size(0),
    m_field_metadata(0),
    m_field_metadata_size(0),
    m_null_bits(0),
    m_meta_memory(NULL)
{
  assert(m_table_id != UINT32_MAX);
  /*
    In TABLE_SHARE, "db" and "table_name" are 0-terminated (see this comment in
    table.cc / alloc_table_share():
      Use the fact the key is db/0/table_name/0
    As we rely on this let's assert it.
  */
  assert((tbl->s->db.str == 0) ||
              (tbl->s->db.str[tbl->s->db.length] == 0));
  assert(tbl->s->table_name.str[tbl->s->table_name.length] == 0);


  m_data_size=  TABLE_MAP_HEADER_LEN;
  m_data_size+= m_dblen + 2;	// Include length and terminating \0
  m_data_size+= m_tbllen + 2;	// Include length and terminating \0
  m_data_size+= 1 + m_colcnt;	// COLCNT and column types

  /* If malloc fails, caught in is_valid() */
  if ((m_memory= (unsigned char*) malloc(m_colcnt)))
  {
    m_coltype= reinterpret_cast<unsigned char*>(m_memory);
    for (unsigned int i= 0 ; i < m_table->s->fields ; ++i)
      m_coltype[i]= m_table->field[i]->type();
  }

  /*
    Calculate a bitmap for the results of maybe_null() for all columns.
    The bitmap is used to determine when there is a column from the master
    that is not on the slave and is null and thus not in the row data during
    replication.
  */
  uint32_t num_null_bytes= (m_table->s->fields + 7) / 8;
  m_data_size+= num_null_bytes;
  m_meta_memory= (unsigned char *)my_multi_malloc(MYF(MY_WME),
                                 &m_null_bits, num_null_bytes,
                                 &m_field_metadata, (m_colcnt * 2),
                                 NULL);

  memset(m_field_metadata, 0, (m_colcnt * 2));

  /*
    Create an array for the field metadata and store it.
  */
  m_field_metadata_size= save_field_metadata();
  assert(m_field_metadata_size <= (m_colcnt * 2));

  /*
    Now set the size of the data to the size of the field metadata array
    plus one or two bytes for number of elements in the field metadata array.
  */
  if (m_field_metadata_size > 255)
    m_data_size+= m_field_metadata_size + 2;
  else
    m_data_size+= m_field_metadata_size + 1;

  memset(m_null_bits, 0, num_null_bytes);
  for (unsigned int i= 0 ; i < m_table->s->fields ; ++i)
    if (m_table->field[i]->maybe_null())
      m_null_bits[(i / 8)]+= 1 << (i % 8);

}


/*
  Constructor used by slave to read the event from the binary log.
 */
Table_map_log_event::Table_map_log_event(const char *buf, uint32_t event_len,
                                         const Format_description_log_event
                                         *description_event)

  : Log_event(buf, description_event),
    m_table(NULL),
    m_dbnam(NULL), m_dblen(0), m_tblnam(NULL), m_tbllen(0),
    m_colcnt(0), m_coltype(0),
    m_memory(NULL), m_table_id(ULONG_MAX), m_flags(0),
    m_data_size(0), m_field_metadata(0), m_field_metadata_size(0),
    m_null_bits(0), m_meta_memory(NULL)
{
  unsigned int bytes_read= 0;

  uint8_t common_header_len= description_event->common_header_len;
  uint8_t post_header_len= description_event->post_header_len[TABLE_MAP_EVENT-1];

  /* Read the post-header */
  const char *post_start= buf + common_header_len;

  post_start+= TM_MAPID_OFFSET;
  if (post_header_len == 6)
  {
    /* Master is of an intermediate source tree before 5.1.4. Id is 4 bytes */
    m_table_id= uint4korr(post_start);
    post_start+= 4;
  }
  else
  {
    assert(post_header_len == TABLE_MAP_HEADER_LEN);
    m_table_id= (ulong) uint6korr(post_start);
    post_start+= TM_FLAGS_OFFSET;
  }

  assert(m_table_id != UINT32_MAX);

  m_flags= uint2korr(post_start);

  /* Read the variable part of the event */
  const char *const vpart= buf + common_header_len + post_header_len;

  /* Extract the length of the various parts from the buffer */
  unsigned char const *const ptr_dblen= (unsigned char const*)vpart + 0;
  m_dblen= *(unsigned char*) ptr_dblen;

  /* Length of database name + counter + terminating null */
  unsigned char const *const ptr_tbllen= ptr_dblen + m_dblen + 2;
  m_tbllen= *(unsigned char*) ptr_tbllen;

  /* Length of table name + counter + terminating null */
  unsigned char const *const ptr_colcnt= ptr_tbllen + m_tbllen + 2;
  unsigned char *ptr_after_colcnt= (unsigned char*) ptr_colcnt;
  m_colcnt= net_field_length(&ptr_after_colcnt);

  /* Allocate mem for all fields in one go. If fails, caught in is_valid() */
  m_memory= (unsigned char*) my_multi_malloc(MYF(MY_WME),
                                     &m_dbnam, (uint) m_dblen + 1,
                                     &m_tblnam, (uint) m_tbllen + 1,
                                     &m_coltype, (uint) m_colcnt,
                                     NULL);

  if (m_memory)
  {
    /* Copy the different parts into their memory */
    strncpy(const_cast<char*>(m_dbnam), (const char*)ptr_dblen  + 1, m_dblen + 1);
    strncpy(const_cast<char*>(m_tblnam), (const char*)ptr_tbllen + 1, m_tbllen + 1);
    memcpy(m_coltype, ptr_after_colcnt, m_colcnt);

    ptr_after_colcnt= ptr_after_colcnt + m_colcnt;
    bytes_read= ptr_after_colcnt - (unsigned char *)buf;
    if (bytes_read < event_len)
    {
      m_field_metadata_size= net_field_length(&ptr_after_colcnt);
      assert(m_field_metadata_size <= (m_colcnt * 2));
      uint32_t num_null_bytes= (m_colcnt + 7) / 8;
      m_meta_memory= (unsigned char *)my_multi_malloc(MYF(MY_WME),
                                     &m_null_bits, num_null_bytes,
                                     &m_field_metadata, m_field_metadata_size,
                                     NULL);
      memcpy(m_field_metadata, ptr_after_colcnt, m_field_metadata_size);
      ptr_after_colcnt= (unsigned char*)ptr_after_colcnt + m_field_metadata_size;
      memcpy(m_null_bits, ptr_after_colcnt, num_null_bytes);
    }
  }

  return;
}

Table_map_log_event::~Table_map_log_event()
{
  free(m_meta_memory);
  free(m_memory);
}

/*
  Return value is an error code, one of:

      -1     Failure to open table   [from open_tables()]
       0     Success
       1     No room for more tables [from set_table()]
       2     Out of memory           [from set_table()]
       3     Wrong table definition
       4     Daisy-chaining RBR with SBR not possible
 */

int Table_map_log_event::do_apply_event(Relay_log_info const *rli)
{
  RPL_TableList *table_list;
  char *db_mem, *tname_mem;
  Query_id &query_id= Query_id::get_query_id();
  void *memory;
  assert(rli->sql_session == session);

  /* Step the query id to mark what columns that are actually used. */
  session->query_id= query_id.next();

  if (!(memory= my_multi_malloc(MYF(MY_WME),
                                &table_list, (uint) sizeof(RPL_TableList),
                                &db_mem, (uint) NAME_LEN + 1,
                                &tname_mem, (uint) NAME_LEN + 1,
                                NULL)))
    return(HA_ERR_OUT_OF_MEM);

  memset(table_list, 0, sizeof(*table_list));
  table_list->db = db_mem;
  table_list->alias= table_list->table_name = tname_mem;
  table_list->lock_type= TL_WRITE;
  table_list->next_global= table_list->next_local= 0;
  table_list->table_id= m_table_id;
  table_list->updating= 1;
  strcpy(table_list->db, m_dbnam);
  strcpy(table_list->table_name, m_tblnam);

  int error= 0;

  {
    /*
      open_tables() reads the contents of session->lex, so they must be
      initialized, so we should call lex_start(); to be even safer, we
      call mysql_init_query() which does a more complete set of inits.
    */
    lex_start(session);
    mysql_reset_session_for_next_command(session);

    /*
      Open the table if it is not already open and add the table to
      table map.  Note that for any table that should not be
      replicated, a filter is needed.

      The creation of a new TableList is used to up-cast the
      table_list consisting of RPL_TableList items. This will work
      since the only case where the argument to open_tables() is
      changed, is when session->lex->query_tables == table_list, i.e.,
      when the statement requires prelocking. Since this is not
      executed when a statement is executed, this case will not occur.
      As a precaution, an assertion is added to ensure that the bad
      case is not a fact.

      Either way, the memory in the list is *never* released
      internally in the open_tables() function, hence we take a copy
      of the pointer to make sure that it's not lost.
    */
    uint32_t count;
    assert(session->lex->query_tables != table_list);
    TableList *tmp_table_list= table_list;
    if ((error= open_tables(session, &tmp_table_list, &count, 0)))
    {
      if (session->is_slave_error || session->is_fatal_error)
      {
        /*
          Error reporting borrowed from Query_log_event with many excessive
          simplifications (we don't honour --slave-skip-errors)
        */
        uint32_t actual_error= session->main_da.sql_errno();
        rli->report(ERROR_LEVEL, actual_error,
                    _("Error '%s' on opening table `%s`.`%s`"),
                    (actual_error
                     ? session->main_da.message()
                     : _("unexpected success or fatal error")),
                    table_list->db, table_list->table_name);
        session->is_slave_error= 1;
      }
      goto err;
    }

    m_table= table_list->table;

    /*
      This will fail later otherwise, the 'in_use' field should be
      set to the current thread.
    */
    assert(m_table->in_use);

    /*
      Use placement new to construct the table_def instance in the
      memory allocated for it inside table_list.

      The memory allocated by the table_def structure (i.e., not the
      memory allocated *for* the table_def structure) is released
      inside Relay_log_info::clear_tables_to_lock() by calling the
      table_def destructor explicitly.
    */
    new (&table_list->m_tabledef) table_def(m_coltype, m_colcnt,
         m_field_metadata, m_field_metadata_size, m_null_bits);
    table_list->m_tabledef_valid= true;

    /*
      We record in the slave's information that the table should be
      locked by linking the table into the list of tables to lock.
    */
    table_list->next_global= table_list->next_local= rli->tables_to_lock;
    const_cast<Relay_log_info*>(rli)->tables_to_lock= table_list;
    const_cast<Relay_log_info*>(rli)->tables_to_lock_count++;
    /* 'memory' is freed in clear_tables_to_lock */
  }

  return(error);

err:
  free(memory);
  return(error);
}

Log_event::enum_skip_reason
Table_map_log_event::do_shall_skip(Relay_log_info *rli)
{
  /*
    If the slave skip counter is 1, then we should not start executing
    on the next event.
  */
  return continue_group(rli);
}

int Table_map_log_event::do_update_pos(Relay_log_info *rli)
{
  rli->inc_event_relay_log_pos();
  return 0;
}


bool Table_map_log_event::write_data_header(IO_CACHE *file)
{
  assert(m_table_id != UINT32_MAX);
  unsigned char buf[TABLE_MAP_HEADER_LEN];
  int6store(buf + TM_MAPID_OFFSET, (uint64_t)m_table_id);
  int2store(buf + TM_FLAGS_OFFSET, m_flags);
  return (my_b_safe_write(file, buf, TABLE_MAP_HEADER_LEN));
}

bool Table_map_log_event::write_data_body(IO_CACHE *file)
{
  assert(m_dbnam != NULL);
  assert(m_tblnam != NULL);
  /* We use only one byte per length for storage in event: */
  assert(m_dblen < 128);
  assert(m_tbllen < 128);

  unsigned char const dbuf[]= { (unsigned char) m_dblen };
  unsigned char const tbuf[]= { (unsigned char) m_tbllen };

  unsigned char cbuf[sizeof(m_colcnt)];
  unsigned char *const cbuf_end= net_store_length(cbuf, (size_t) m_colcnt);
  assert(static_cast<size_t>(cbuf_end - cbuf) <= sizeof(cbuf));

  /*
    Store the size of the field metadata.
  */
  unsigned char mbuf[sizeof(m_field_metadata_size)];
  unsigned char *const mbuf_end= net_store_length(mbuf, m_field_metadata_size);

  return (my_b_safe_write(file, dbuf,      sizeof(dbuf)) ||
          my_b_safe_write(file, (const unsigned char*)m_dbnam,   m_dblen+1) ||
          my_b_safe_write(file, tbuf,      sizeof(tbuf)) ||
          my_b_safe_write(file, (const unsigned char*)m_tblnam,  m_tbllen+1) ||
          my_b_safe_write(file, cbuf, (size_t) (cbuf_end - cbuf)) ||
          my_b_safe_write(file, m_coltype, m_colcnt) ||
          my_b_safe_write(file, mbuf, (size_t) (mbuf_end - mbuf)) ||
          my_b_safe_write(file, m_field_metadata, m_field_metadata_size),
          my_b_safe_write(file, m_null_bits, (m_colcnt + 7) / 8));
 }


/*
  Print some useful information for the SHOW BINARY LOG information
  field.
 */

void Table_map_log_event::pack_info(Protocol *protocol)
{
    char buf[256];
    size_t bytes= snprintf(buf, sizeof(buf),
                           "table_id: %lu (%s.%s)",
                           m_table_id, m_dbnam, m_tblnam);
    protocol->store(buf, bytes, &my_charset_bin);
}


/**************************************************************************
	Write_rows_log_event member functions
**************************************************************************/

/*
  Constructor used to build an event for writing to the binary log.
 */
Write_rows_log_event::Write_rows_log_event(Session *session_arg, Table *tbl_arg,
                                           ulong tid_arg,
                                           bool is_transactional)
  : Rows_log_event(session_arg, tbl_arg, tid_arg, tbl_arg->write_set, is_transactional)
{
}

/*
  Constructor used by slave to read the event from the binary log.
 */
Write_rows_log_event::Write_rows_log_event(const char *buf, uint32_t event_len,
                                           const Format_description_log_event
                                           *description_event)
: Rows_log_event(buf, event_len, WRITE_ROWS_EVENT, description_event)
{
}

int
Write_rows_log_event::do_before_row_operations(const Slave_reporting_capability *const)
{
  int error= 0;

  /**
     todo: to introduce a property for the event (handler?) which forces
     applying the event in the replace (idempotent) fashion.
  */
  if (bit_is_set(slave_exec_mode, SLAVE_EXEC_MODE_IDEMPOTENT) == 1)
  {
    /*
      We are using REPLACE semantics and not INSERT IGNORE semantics
      when writing rows, that is: new rows replace old rows.  We need to
      inform the storage engine that it should use this behaviour.
    */

    /* Tell the storage engine that we are using REPLACE semantics. */
    session->lex->duplicates= DUP_REPLACE;

    /*
      Pretend we're executing a REPLACE command: this is needed for
      InnoDB since it is not (properly) checking the
      lex->duplicates flag.
    */
    session->lex->sql_command= SQLCOM_REPLACE;
    /*
       Do not raise the error flag in case of hitting to an unique attribute
    */
    m_table->file->extra(HA_EXTRA_IGNORE_DUP_KEY);
  }

  m_table->file->ha_start_bulk_insert(0);
  /*
    We need TIMESTAMP_NO_AUTO_SET otherwise ha_write_row() will not use fill
    any TIMESTAMP column with data from the row but instead will use
    the event's current time.
    As we replicate from TIMESTAMP to TIMESTAMP and slave has no extra
    columns, we know that all TIMESTAMP columns on slave will receive explicit
    data from the row, so TIMESTAMP_NO_AUTO_SET is ok.
    When we allow a table without TIMESTAMP to be replicated to a table having
    more columns including a TIMESTAMP column, or when we allow a TIMESTAMP
    column to be replicated into a BIGINT column and the slave's table has a
    TIMESTAMP column, then the slave's TIMESTAMP column will take its value
    from set_time() which we called earlier (consistent with SBR). And then in
    some cases we won't want TIMESTAMP_NO_AUTO_SET (will require some code to
    analyze if explicit data is provided for slave's TIMESTAMP columns).
  */
  m_table->timestamp_field_type= TIMESTAMP_NO_AUTO_SET;

  return error;
}

int
Write_rows_log_event::do_after_row_operations(const Slave_reporting_capability *const,
                                              int error)
{
  int local_error= 0;
  if (bit_is_set(slave_exec_mode, SLAVE_EXEC_MODE_IDEMPOTENT) == 1)
  {
    m_table->file->extra(HA_EXTRA_NO_IGNORE_DUP_KEY);
    m_table->file->extra(HA_EXTRA_WRITE_CANNOT_REPLACE);
    /*
      resetting the extra with
      table->file->extra(HA_EXTRA_NO_IGNORE_NO_KEY);
      fires bug#27077
      explanation: file->reset() performs this duty
      ultimately. Still todo: fix
    */
  }
  if ((local_error= m_table->file->ha_end_bulk_insert()))
  {
    m_table->file->print_error(local_error, MYF(0));
  }
  return error? error : local_error;
}


/*
  Check if there are more UNIQUE keys after the given key.
*/
static int
last_uniq_key(Table *table, uint32_t keyno)
{
  while (++keyno < table->s->keys)
    if (table->key_info[keyno].flags & HA_NOSAME)
      return 0;
  return 1;
}

/**
   Check if an error is a duplicate key error.

   This function is used to check if an error code is one of the
   duplicate key error, i.e., and error code for which it is sensible
   to do a <code>get_dup_key()</code> to retrieve the duplicate key.

   @param errcode The error code to check.

   @return <code>true</code> if the error code is such that
   <code>get_dup_key()</code> will return true, <code>false</code>
   otherwise.
 */
bool
is_duplicate_key_error(int errcode)
{
  switch (errcode)
  {
  case HA_ERR_FOUND_DUPP_KEY:
  case HA_ERR_FOUND_DUPP_UNIQUE:
    return true;
  }
  return false;
}

/**
  Write the current row into event's table.

  The row is located in the row buffer, pointed by @c m_curr_row member.
  Number of columns of the row is stored in @c m_width member (it can be
  different from the number of columns in the table to which we insert).
  Bitmap @c m_cols indicates which columns are present in the row. It is assumed
  that event's table is already open and pointed by @c m_table.

  If the same record already exists in the table it can be either overwritten
  or an error is reported depending on the value of @c overwrite flag
  (error reporting not yet implemented). Note that the matching record can be
  different from the row we insert if we use primary keys to identify records in
  the table.

  The row to be inserted can contain values only for selected columns. The
  missing columns are filled with default values using @c prepare_record()
  function. If a matching record is found in the table and @c overwritte is
  true, the missing columns are taken from it.

  @param  rli   Relay log info (needed for row unpacking).
  @param  overwrite
                Shall we overwrite if the row already exists or signal
                error (currently ignored).

  @returns Error code on failure, 0 on success.

  This method, if successful, sets @c m_curr_row_end pointer to point at the
  next row in the rows buffer. This is done when unpacking the row to be
  inserted.

  @note If a matching record is found, it is either updated using
  @c ha_update_row() or first deleted and then new record written.
*/

int
Rows_log_event::write_row(const Relay_log_info *const rli,
                          const bool overwrite)
{
  assert(m_table != NULL && session != NULL);

  Table *table= m_table;  // pointer to event's table
  int error;
  int keynum;
  basic_string<unsigned char> key;

  /* fill table->record[0] with default values */

  /*
     We only check if the columns have default values for non-NDB
     engines, for NDB we ignore the check since updates are sent as
     writes, causing errors when trying to prepare the record.

     TODO[ndb]: Elimiate this hard-coded dependency on NDB. Ideally,
     the engine should be able to set a flag that it want the default
     values filled in and one flag to handle the case that the default
     values should be checked. Maybe these two flags can be combined.
  */
  if ((error= prepare_record(table, &m_cols, m_width, true)))
    return(error);

  /* unpack row into table->record[0] */
  error= unpack_current_row(rli, &m_cols);

  // Temporary fix to find out why it fails [/Matz]
  memcpy(m_table->write_set->bitmap, m_cols.bitmap, (m_table->write_set->n_bits + 7) / 8);

  /*
    Try to write record. If a corresponding record already exists in the table,
    we try to change it using ha_update_row() if possible. Otherwise we delete
    it and repeat the whole process again.

    TODO: Add safety measures against infinite looping.
   */

  while ((error= table->file->ha_write_row(table->record[0])))
  {
    if (error == HA_ERR_LOCK_DEADLOCK ||
        error == HA_ERR_LOCK_WAIT_TIMEOUT ||
        (keynum= table->file->get_dup_key(error)) < 0 ||
        !overwrite)
    {
      /*
        Deadlock, waiting for lock or just an error from the handler
        such as HA_ERR_FOUND_DUPP_KEY when overwrite is false.
        Retrieval of the duplicate key number may fail
        - either because the error was not "duplicate key" error
        - or because the information which key is not available
      */
      table->file->print_error(error, MYF(0));
      return(error);
    }
    /*
       We need to retrieve the old row into record[1] to be able to
       either update or delete the offending record.  We either:

       - use rnd_pos() with a row-id (available as dupp_row) to the
         offending row, if that is possible (MyISAM and Blackhole), or else

       - use index_read_idx() with the key that is duplicated, to
         retrieve the offending row.
     */
    if (table->file->ha_table_flags() & HA_DUPLICATE_POS)
    {
      if (table->file->inited && (error= table->file->ha_index_end()))
        return(error);
      if ((error= table->file->ha_rnd_init(false)))
        return(error);

      error= table->file->rnd_pos(table->record[1], table->file->dup_ref);
      table->file->ha_rnd_end();
      if (error)
      {
        table->file->print_error(error, MYF(0));
        return(error);
      }
    }
    else
    {
      if (table->file->extra(HA_EXTRA_FLUSH_CACHE))
      {
        return(my_errno);
      }

      key.reserve(table->s->max_unique_length);

      key_copy(key, table->record[0], table->key_info + keynum, 0);
      error= table->file->index_read_idx_map(table->record[1], keynum,
                                             key.data(),
                                             HA_WHOLE_KEY,
                                             HA_READ_KEY_EXACT);
      if (error)
      {
        table->file->print_error(error, MYF(0));
        return(error);
      }
    }

    /*
       Now, record[1] should contain the offending row.  That
       will enable us to update it or, alternatively, delete it (so
       that we can insert the new row afterwards).
     */

    /*
      If row is incomplete we will use the record found to fill
      missing columns.
    */
    if (!get_flags(COMPLETE_ROWS_F))
    {
      restore_record(table,record[1]);
      error= unpack_current_row(rli, &m_cols);
    }

    /*
       REPLACE is defined as either INSERT or DELETE + INSERT.  If
       possible, we can replace it with an UPDATE, but that will not
       work on InnoDB if FOREIGN KEY checks are necessary.

       I (Matz) am not sure of the reason for the last_uniq_key()
       check as, but I'm guessing that it's something along the
       following lines.

       Suppose that we got the duplicate key to be a key that is not
       the last unique key for the table and we perform an update:
       then there might be another key for which the unique check will
       fail, so we're better off just deleting the row and inserting
       the correct row.
     */
    if (last_uniq_key(table, keynum) &&
        !table->file->referenced_by_foreign_key())
    {
      error=table->file->ha_update_row(table->record[1],
                                       table->record[0]);
      switch (error) {

      case HA_ERR_RECORD_IS_THE_SAME:
        error= 0;

      case 0:
        break;

      default:
        table->file->print_error(error, MYF(0));
      }

      return(error);
    }
    else
    {
      if ((error= table->file->ha_delete_row(table->record[1])))
      {
        table->file->print_error(error, MYF(0));
        return(error);
      }
      /* Will retry ha_write_row() with the offending row removed. */
    }
  }

  return(error);
}


int Rows_log_event::unpack_current_row(const Relay_log_info *const rli,
                                         MY_BITMAP const *cols)
{
  assert(m_table);
  ASSERT_OR_RETURN_ERROR(m_curr_row < m_rows_end, HA_ERR_CORRUPT_EVENT);
  int const result= ::unpack_row(rli, m_table, m_width, m_curr_row, cols,
                                 &m_curr_row_end, &m_master_reclength);
  if (m_curr_row_end > m_rows_end)
    my_error(ER_SLAVE_CORRUPT_EVENT, MYF(0));
  ASSERT_OR_RETURN_ERROR(m_curr_row_end <= m_rows_end, HA_ERR_CORRUPT_EVENT);
  return result;
}


int
Write_rows_log_event::do_exec_row(const Relay_log_info *const rli)
{
  assert(m_table != NULL);
  int error=
    write_row(rli,        /* if 1 then overwrite */
              bit_is_set(slave_exec_mode, SLAVE_EXEC_MODE_IDEMPOTENT) == 1);

  if (error && !session->is_error())
  {
    assert(0);
    my_error(ER_UNKNOWN_ERROR, MYF(0));
  }

  return error;
}


/**************************************************************************
	Delete_rows_log_event member functions
**************************************************************************/

/*
  Compares table->record[0] and table->record[1]

  Returns TRUE if different.
*/
static bool record_compare(Table *table)
{
  /*
    Need to set the X bit and the filler bits in both records since
    there are engines that do not set it correctly.

    In addition, since MyISAM checks that one hasn't tampered with the
    record, it is necessary to restore the old bytes into the record
    after doing the comparison.

    TODO[record format ndb]: Remove it once NDB returns correct
    records. Check that the other engines also return correct records.
   */
  bool result= false;
  unsigned char saved_x[2], saved_filler[2];

  if (table->s->null_bytes > 0)
  {
    for (int i = 0 ; i < 2 ; ++i)
    {
      saved_x[i]= table->record[i][0];
      saved_filler[i]= table->record[i][table->s->null_bytes - 1];
      table->record[i][0]|= 1U;
      table->record[i][table->s->null_bytes - 1]|=
        256U - (1U << table->s->last_null_bit_pos);
    }
  }

  if (table->s->blob_fields + table->s->varchar_fields == 0)
  {
    result= cmp_record(table,record[1]);
    goto record_compare_exit;
  }

  /* Compare null bits */
  if (memcmp(table->null_flags,
	     table->null_flags+table->s->rec_buff_length,
	     table->s->null_bytes))
  {
    result= true;				// Diff in NULL value
    goto record_compare_exit;
  }

  /* Compare updated fields */
  for (Field **ptr=table->field ; *ptr ; ptr++)
  {
    if ((*ptr)->cmp_binary_offset(table->s->rec_buff_length))
    {
      result= true;
      goto record_compare_exit;
    }
  }

record_compare_exit:
  /*
    Restore the saved bytes.

    TODO[record format ndb]: Remove this code once NDB returns the
    correct record format.
  */
  if (table->s->null_bytes > 0)
  {
    for (int i = 0 ; i < 2 ; ++i)
    {
      table->record[i][0]= saved_x[i];
      table->record[i][table->s->null_bytes - 1]= saved_filler[i];
    }
  }

  return result;
}

/**
  Locate the current row in event's table.

  The current row is pointed by @c m_curr_row. Member @c m_width tells how many
  columns are there in the row (this can be differnet from the number of columns
  in the table). It is assumed that event's table is already open and pointed
  by @c m_table.

  If a corresponding record is found in the table it is stored in
  @c m_table->record[0]. Note that when record is located based on a primary
  key, it is possible that the record found differs from the row being located.

  If no key is specified or table does not have keys, a table scan is used to
  find the row. In that case the row should be complete and contain values for
  all columns. However, it can still be shorter than the table, i.e. the table
  can contain extra columns not present in the row. It is also possible that
  the table has fewer columns than the row being located.

  @returns Error code on failure, 0 on success.

  @post In case of success @c m_table->record[0] contains the record found.
  Also, the internal "cursor" of the table is positioned at the record found.

  @note If the engine allows random access of the records, a combination of
  @c position() and @c rnd_pos() will be used.
 */

int Rows_log_event::find_row(const Relay_log_info *rli)
{
  assert(m_table && m_table->in_use != NULL);

  Table *table= m_table;
  int error;

  /* unpack row - missing fields get default values */
  prepare_record(table, &m_cols, m_width, false/* don't check errors */);
  error= unpack_current_row(rli, &m_cols);

  // Temporary fix to find out why it fails [/Matz]
  memcpy(m_table->read_set->bitmap, m_cols.bitmap, (m_table->read_set->n_bits + 7) / 8);

  if ((table->file->ha_table_flags() & HA_PRIMARY_KEY_REQUIRED_FOR_POSITION) &&
      table->s->primary_key < MAX_KEY)
  {
    /*
      Use a more efficient method to fetch the record given by
      table->record[0] if the engine allows it.  We first compute a
      row reference using the position() member function (it will be
      stored in table->file->ref) and the use rnd_pos() to position
      the "cursor" (i.e., record[0] in this case) at the correct row.

      TODO: Add a check that the correct record has been fetched by
      comparing with the original record. Take into account that the
      record on the master and slave can be of different
      length. Something along these lines should work:

      ADD>>>  store_record(table,record[1]);
              int error= table->file->rnd_pos(table->record[0], table->file->ref);
      ADD>>>  assert(memcmp(table->record[1], table->record[0],
                                 table->s->reclength) == 0);
    */

    int error= table->file->rnd_pos_by_record(table->record[0]);
    table->file->ha_rnd_end();
    if (error)
    {
      table->file->print_error(error, MYF(0));
    }
    return(error);
  }

  // We can't use position() - try other methods.

  /*
    Save copy of the record in table->record[1]. It might be needed
    later if linear search is used to find exact match.
   */
  store_record(table,record[1]);

  if (table->s->keys > 0)
  {
    /* We have a key: search the table using the index */
    if (!table->file->inited && (error= table->file->ha_index_init(0, false)))
    {
      table->file->print_error(error, MYF(0));
      goto err;
    }

    /* Fill key data for the row */

    assert(m_key);
    key_copy(m_key, table->record[0], table->key_info, 0);

    /*
      We need to set the null bytes to ensure that the filler bit are
      all set when returning.  There are storage engines that just set
      the necessary bits on the bytes and don't set the filler bits
      correctly.
    */
    my_ptrdiff_t const pos=
      table->s->null_bytes > 0 ? table->s->null_bytes - 1 : 0;
    table->record[0][pos]= 0xFF;

    if ((error= table->file->index_read_map(table->record[0], m_key,
                                            HA_WHOLE_KEY,
                                            HA_READ_KEY_EXACT)))
    {
      table->file->print_error(error, MYF(0));
      table->file->ha_index_end();
      goto err;
    }

    /*
      Below is a minor "optimization".  If the key (i.e., key number
      0) has the HA_NOSAME flag set, we know that we have found the
      correct record (since there can be no duplicates); otherwise, we
      have to compare the record with the one found to see if it is
      the correct one.

      CAVEAT! This behaviour is essential for the replication of,
      e.g., the mysql.proc table since the correct record *shall* be
      found using the primary key *only*.  There shall be no
      comparison of non-PK columns to decide if the correct record is
      found.  I can see no scenario where it would be incorrect to
      chose the row to change only using a PK or an UNNI.
    */
    if (table->key_info->flags & HA_NOSAME)
    {
      table->file->ha_index_end();
      goto ok;
    }

    /*
      In case key is not unique, we still have to iterate over records found
      and find the one which is identical to the row given. A copy of the
      record we are looking for is stored in record[1].
     */
    while (record_compare(table))
    {
      /*
        We need to set the null bytes to ensure that the filler bit
        are all set when returning.  There are storage engines that
        just set the necessary bits on the bytes and don't set the
        filler bits correctly.

        TODO[record format ndb]: Remove this code once NDB returns the
        correct record format.
      */
      if (table->s->null_bytes > 0)
      {
        table->record[0][table->s->null_bytes - 1]|=
          256U - (1U << table->s->last_null_bit_pos);
      }

      if ((error= table->file->index_next(table->record[0])))
      {
        table->file->print_error(error, MYF(0));
        table->file->ha_index_end();
        goto err;
      }
    }

    /*
      Have to restart the scan to be able to fetch the next row.
    */
    table->file->ha_index_end();
  }
  else
  {
    int restart_count= 0; // Number of times scanning has restarted from top

    /* We don't have a key: search the table using rnd_next() */
    if ((error= table->file->ha_rnd_init(1)))
    {
      table->file->print_error(error, MYF(0));
      goto err;
    }

    /* Continue until we find the right record or have made a full loop */
    do
    {
      error= table->file->rnd_next(table->record[0]);

      switch (error) {

      case 0:
      case HA_ERR_RECORD_DELETED:
        break;

      case HA_ERR_END_OF_FILE:
        if (++restart_count < 2)
          table->file->ha_rnd_init(1);
        break;

      default:
        table->file->print_error(error, MYF(0));
        table->file->ha_rnd_end();
        goto err;
      }
    }
    while (restart_count < 2 && record_compare(table));

    /*
      Note: above record_compare will take into accout all record fields
      which might be incorrect in case a partial row was given in the event
     */
    table->file->ha_rnd_end();

    assert(error == HA_ERR_END_OF_FILE || error == HA_ERR_RECORD_DELETED || error == 0);
    goto err;
  }
ok:
  table->default_column_bitmaps();
  return(0);
err:
  table->default_column_bitmaps();
  return(error);
}


/*
  Constructor used to build an event for writing to the binary log.
 */

Delete_rows_log_event::Delete_rows_log_event(Session *session_arg, Table *tbl_arg,
                                             ulong tid,
                                             bool is_transactional)
  : Rows_log_event(session_arg, tbl_arg, tid, tbl_arg->read_set, is_transactional)
{
}

/*
  Constructor used by slave to read the event from the binary log.
 */
Delete_rows_log_event::Delete_rows_log_event(const char *buf, uint32_t event_len,
                                             const Format_description_log_event
                                             *description_event)
  : Rows_log_event(buf, event_len, DELETE_ROWS_EVENT, description_event)
{
}


int
Delete_rows_log_event::do_before_row_operations(const Slave_reporting_capability *const)
{
  if ((m_table->file->ha_table_flags() & HA_PRIMARY_KEY_REQUIRED_FOR_POSITION) &&
      m_table->s->primary_key < MAX_KEY)
  {
    /*
      We don't need to allocate any memory for m_key since it is not used.
    */
    return 0;
  }

  if (m_table->s->keys > 0)
  {
    // Allocate buffer for key searches
    m_key= (unsigned char*)malloc(m_table->key_info->key_length);
    if (!m_key)
      return HA_ERR_OUT_OF_MEM;
  }

  return 0;
}

int
Delete_rows_log_event::do_after_row_operations(const Slave_reporting_capability *const,
                                               int error)
{
  /*error= ToDo:find out what this should really be, this triggers close_scan in nbd, returning error?*/
  m_table->file->ha_index_or_rnd_end();
  free(m_key);
  m_key= NULL;

  return error;
}

int Delete_rows_log_event::do_exec_row(const Relay_log_info *const rli)
{
  int error;
  assert(m_table != NULL);

  if (!(error= find_row(rli)))
  {
    /*
      Delete the record found, located in record[0]
    */
    error= m_table->file->ha_delete_row(m_table->record[0]);
  }
  return error;
}


/**************************************************************************
	Update_rows_log_event member functions
**************************************************************************/

/*
  Constructor used to build an event for writing to the binary log.
 */
Update_rows_log_event::Update_rows_log_event(Session *session_arg, Table *tbl_arg,
                                             ulong tid,
                                             bool is_transactional)
: Rows_log_event(session_arg, tbl_arg, tid, tbl_arg->read_set, is_transactional)
{
  init(tbl_arg->write_set);
}

void Update_rows_log_event::init(MY_BITMAP const *cols)
{
  /* if bitmap_init fails, caught in is_valid() */
  if (likely(!bitmap_init(&m_cols_ai,
                          m_width <= sizeof(m_bitbuf_ai)*8 ? m_bitbuf_ai : NULL,
                          m_width,
                          false)))
  {
    /* Cols can be zero if this is a dummy binrows event */
    if (likely(cols != NULL))
    {
      memcpy(m_cols_ai.bitmap, cols->bitmap, no_bytes_in_map(cols));
      create_last_word_mask(&m_cols_ai);
    }
  }
}


Update_rows_log_event::~Update_rows_log_event()
{
  if (m_cols_ai.bitmap == m_bitbuf_ai) // no malloc happened
    m_cols_ai.bitmap= 0; // so no free in bitmap_free
  bitmap_free(&m_cols_ai); // To pair with bitmap_init().
}


/*
  Constructor used by slave to read the event from the binary log.
 */
Update_rows_log_event::Update_rows_log_event(const char *buf, uint32_t event_len,
                                             const
                                             Format_description_log_event
                                             *description_event)
  : Rows_log_event(buf, event_len, UPDATE_ROWS_EVENT, description_event)
{
}


int
Update_rows_log_event::do_before_row_operations(const Slave_reporting_capability *const)
{
  if (m_table->s->keys > 0)
  {
    // Allocate buffer for key searches
    m_key= (unsigned char*)malloc(m_table->key_info->key_length);
    if (!m_key)
      return HA_ERR_OUT_OF_MEM;
  }

  m_table->timestamp_field_type= TIMESTAMP_NO_AUTO_SET;

  return 0;
}

int
Update_rows_log_event::do_after_row_operations(const Slave_reporting_capability *const,
                                               int error)
{
  /*error= ToDo:find out what this should really be, this triggers close_scan in nbd, returning error?*/
  m_table->file->ha_index_or_rnd_end();
  free(m_key); // Free for multi_malloc
  m_key= NULL;

  return error;
}

int
Update_rows_log_event::do_exec_row(const Relay_log_info *const rli)
{
  assert(m_table != NULL);

  int error= find_row(rli);
  if (error)
  {
    /*
      We need to read the second image in the event of error to be
      able to skip to the next pair of updates
    */
    m_curr_row= m_curr_row_end;
    unpack_current_row(rli, &m_cols_ai);
    return error;
  }

  /*
    This is the situation after locating BI:

    ===|=== before image ====|=== after image ===|===
       ^                     ^
       m_curr_row            m_curr_row_end

    BI found in the table is stored in record[0]. We copy it to record[1]
    and unpack AI to record[0].
   */

  store_record(m_table,record[1]);

  m_curr_row= m_curr_row_end;
  error= unpack_current_row(rli, &m_cols_ai); // this also updates m_curr_row_end

  /*
    Now we have the right row to update.  The old row (the one we're
    looking for) is in record[1] and the new row is in record[0].
  */

  // Temporary fix to find out why it fails [/Matz]
  memcpy(m_table->read_set->bitmap, m_cols.bitmap, (m_table->read_set->n_bits + 7) / 8);
  memcpy(m_table->write_set->bitmap, m_cols_ai.bitmap, (m_table->write_set->n_bits + 7) / 8);

  error= m_table->file->ha_update_row(m_table->record[1], m_table->record[0]);
  if (error == HA_ERR_RECORD_IS_THE_SAME)
    error= 0;

  return error;
}


Incident_log_event::Incident_log_event(const char *buf, uint32_t event_len,
                                       const Format_description_log_event *descr_event)
  : Log_event(buf, descr_event)
{
  uint8_t const common_header_len=
    descr_event->common_header_len;
  uint8_t const post_header_len=
    descr_event->post_header_len[INCIDENT_EVENT-1];

  m_incident= static_cast<Incident>(uint2korr(buf + common_header_len));
  char const *ptr= buf + common_header_len + post_header_len;
  char const *const str_end= buf + event_len;
  uint8_t len= 0;                   // Assignment to keep compiler happy
  const char *str= NULL;          // Assignment to keep compiler happy
  read_str(&ptr, str_end, &str, &len);
  m_message.str= const_cast<char*>(str);
  m_message.length= len;
  return;
}


Incident_log_event::~Incident_log_event()
{
}


const char *
Incident_log_event::description() const
{
  static const char *const description[]= {
    "NOTHING",                                  // Not used
    "LOST_EVENTS"
  };

  assert(0 <= m_incident);
  assert((size_t) m_incident <= sizeof(description)/sizeof(*description));

  return description[m_incident];
}


void Incident_log_event::pack_info(Protocol *protocol)
{
  char buf[256];
  size_t bytes;
  if (m_message.length > 0)
    bytes= snprintf(buf, sizeof(buf), "#%d (%s)",
                    m_incident, description());
  else
    bytes= snprintf(buf, sizeof(buf), "#%d (%s): %s",
                    m_incident, description(), m_message.str);
  protocol->store(buf, bytes, &my_charset_bin);
}


int
Incident_log_event::do_apply_event(Relay_log_info const *rli)
{
  rli->report(ERROR_LEVEL, ER_SLAVE_INCIDENT,
              ER(ER_SLAVE_INCIDENT),
              description(),
              m_message.length > 0 ? m_message.str : "<none>");
  return(1);
}


bool
Incident_log_event::write_data_header(IO_CACHE *file)
{
  unsigned char buf[sizeof(int16_t)];
  int2store(buf, (int16_t) m_incident);
  return(my_b_safe_write(file, buf, sizeof(buf)));
}

bool
Incident_log_event::write_data_body(IO_CACHE *file)
{
  return(write_str(file, m_message.str, m_message.length));
}

Heartbeat_log_event::Heartbeat_log_event(const char* buf, uint32_t event_len,
                    const Format_description_log_event* description_event)
  :Log_event(buf, description_event)
{
  uint8_t header_size= description_event->common_header_len;
  ident_len = event_len - header_size;
  set_if_smaller(ident_len,FN_REFLEN-1);
  log_ident= buf + header_size;
}
