/* Copyright (C) 2000-2006 MySQL AB & Sasha

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

#include <drizzled/server_includes.h>

#include <drizzled/replication/mi.h>
#include <drizzled/replication/replication.h>
#include <drizzled/log_event.h>
#include <libdrizzle/libdrizzle.h>
#include <mysys/hash.h>
#include <drizzled/error.h>
#include <drizzled/gettext.h>
#include <drizzled/data_home.h>
#include <drizzled/unireg.h>
#include <drizzled/item/return_int.h>
#include <drizzled/item/empty_string.h>

int max_binlog_dump_events = 0; // unlimited


/*
  Adjust the position pointer in the binary log file for all running slaves

  SYNOPSIS
    adjust_linfo_offsets()
    purge_offset	Number of bytes removed from start of log index file

  NOTES
    - This is called when doing a PURGE when we delete lines from the
      index log file

  REQUIREMENTS
    - Before calling this function, we have to ensure that no threads are
      using any binary log file before purge_offset.a

  TODO
    - Inform the slave threads that they should sync the position
      in the binary log file with flush_relay_log_info.
      Now they sync is done for next read.
*/

void adjust_linfo_offsets(my_off_t purge_offset)
{
  Session *tmp;

  pthread_mutex_lock(&LOCK_thread_count);
  I_List_iterator<Session> it(threads);

  while ((tmp=it++))
  {
    LOG_INFO* linfo;
    if ((linfo = tmp->current_linfo))
    {
      pthread_mutex_lock(&linfo->lock);
      /*
	Index file offset can be less that purge offset only if
	we just started reading the index file. In that case
	we have nothing to adjust
      */
      if (linfo->index_file_offset < purge_offset)
	linfo->fatal = (linfo->index_file_offset != 0);
      else
	linfo->index_file_offset -= purge_offset;
      pthread_mutex_unlock(&linfo->lock);
    }
  }
  pthread_mutex_unlock(&LOCK_thread_count);
}


bool log_in_use(const char* log_name)
{
  int log_name_len = strlen(log_name) + 1;
  Session *tmp;
  bool result = 0;

  pthread_mutex_lock(&LOCK_thread_count);
  I_List_iterator<Session> it(threads);

  while ((tmp=it++))
  {
    LOG_INFO* linfo;
    if ((linfo = tmp->current_linfo))
    {
      pthread_mutex_lock(&linfo->lock);
      result = !memcmp(log_name, linfo->log_file_name, log_name_len);
      pthread_mutex_unlock(&linfo->lock);
      if (result)
	break;
    }
  }

  pthread_mutex_unlock(&LOCK_thread_count);
  return result;
}

bool purge_error_message(Session* session, int res)
{
  uint32_t errmsg= 0;

  switch (res)  {
  case 0: break;
  case LOG_INFO_EOF:	errmsg= ER_UNKNOWN_TARGET_BINLOG; break;
  case LOG_INFO_IO:	errmsg= ER_IO_ERR_LOG_INDEX_READ; break;
  case LOG_INFO_INVALID:errmsg= ER_BINLOG_PURGE_PROHIBITED; break;
  case LOG_INFO_SEEK:	errmsg= ER_FSEEK_FAIL; break;
  case LOG_INFO_MEM:	errmsg= ER_OUT_OF_RESOURCES; break;
  case LOG_INFO_FATAL:	errmsg= ER_BINLOG_PURGE_FATAL_ERR; break;
  case LOG_INFO_IN_USE: errmsg= ER_LOG_IN_USE; break;
  case LOG_INFO_EMFILE: errmsg= ER_BINLOG_PURGE_EMFILE; break;
  default:		errmsg= ER_LOG_PURGE_UNKNOWN_ERR; break;
  }

  if (errmsg)
  {
    my_message(errmsg, ER(errmsg), MYF(0));
    return true;
  }
  my_ok(session);
  return false;
}


int test_for_non_eof_log_read_errors(int error, const char **errmsg)
{
  if (error == LOG_READ_EOF)
    return 0;
  my_errno= ER_MASTER_FATAL_ERROR_READING_BINLOG;
  switch (error) {
  case LOG_READ_BOGUS:
    *errmsg = "bogus data in log event";
    break;
  case LOG_READ_TOO_LARGE:
    *errmsg = "log event entry exceeded max_allowed_packet; \
Increase max_allowed_packet on master";
    break;
  case LOG_READ_IO:
    *errmsg = "I/O error reading log event";
    break;
  case LOG_READ_MEM:
    *errmsg = "memory allocation failed reading log event";
    break;
  case LOG_READ_TRUNC:
    *errmsg = "binlog truncated in the middle of event";
    break;
  default:
    *errmsg = "unknown error reading log event on the master";
    break;
  }
  return error;
}


int start_slave(Session* session , Master_info* mi,  bool net_report)
{
  int slave_errno= 0;
  int thread_mask;

  lock_slave_threads(mi);  // this allows us to cleanly read slave_running
  // Get a mask of _stopped_ threads
  init_thread_mask(&thread_mask,mi,1 /* inverse */);
  /*
    Below we will start all stopped threads.  But if the user wants to
    start only one thread, do as if the other thread was running (as we
    don't wan't to touch the other thread), so set the bit to 0 for the
    other thread
  */
  if (session->lex->slave_session_opt)
    thread_mask&= session->lex->slave_session_opt;
  if (thread_mask) //some threads are stopped, start them
  {
    if (mi->init_master_info(master_info_file, relay_log_info_file, thread_mask))
      slave_errno=ER_MASTER_INFO;
    else if (server_id_supplied && *mi->getHostname())
    {
      /*
        If we will start SQL thread we will care about UNTIL options If
        not and they are specified we will ignore them and warn user
        about this fact.
      */
      if (thread_mask & SLAVE_SQL)
      {
        pthread_mutex_lock(&mi->rli.data_lock);

        if (session->lex->mi.pos)
        {
          mi->rli.until_condition= Relay_log_info::UNTIL_MASTER_POS;
          mi->rli.until_log_pos= session->lex->mi.pos;
          /*
             We don't check session->lex->mi.log_file_name for NULL here
             since it is checked in sql_yacc.yy
          */
          strncpy(mi->rli.until_log_name, session->lex->mi.log_file_name,
                  sizeof(mi->rli.until_log_name)-1);
        }
        else if (session->lex->mi.relay_log_pos)
        {
          mi->rli.until_condition= Relay_log_info::UNTIL_RELAY_POS;
          mi->rli.until_log_pos= session->lex->mi.relay_log_pos;
          strncpy(mi->rli.until_log_name, session->lex->mi.relay_log_name,
                  sizeof(mi->rli.until_log_name)-1);
        }
        else
          mi->rli.clear_until_condition();

        if (mi->rli.until_condition != Relay_log_info::UNTIL_NONE)
        {
          /* Preparing members for effective until condition checking */
          const char *p= fn_ext(mi->rli.until_log_name);
          char *p_end;
          if (*p)
          {
            //p points to '.'
            mi->rli.until_log_name_extension= strtoul(++p,&p_end, 10);
            /*
              p_end points to the first invalid character. If it equals
              to p, no digits were found, error. If it contains '\0' it
              means  conversion went ok.
            */
            if (p_end==p || *p_end)
              slave_errno=ER_BAD_SLAVE_UNTIL_COND;
          }
          else
            slave_errno=ER_BAD_SLAVE_UNTIL_COND;

          /* mark the cached result of the UNTIL comparison as "undefined" */
          mi->rli.until_log_names_cmp_result=
            Relay_log_info::UNTIL_LOG_NAMES_CMP_UNKNOWN;

          /* Issuing warning then started without --skip-slave-start */
          if (!opt_skip_slave_start)
            push_warning(session, DRIZZLE_ERROR::WARN_LEVEL_NOTE,
                         ER_MISSING_SKIP_SLAVE,
                         ER(ER_MISSING_SKIP_SLAVE));
        }

        pthread_mutex_unlock(&mi->rli.data_lock);
      }
      else if (session->lex->mi.pos || session->lex->mi.relay_log_pos)
        push_warning(session, DRIZZLE_ERROR::WARN_LEVEL_NOTE, ER_UNTIL_COND_IGNORED,
                     ER(ER_UNTIL_COND_IGNORED));

      if (!slave_errno)
        slave_errno = start_slave_threads(0 /*no mutex */,
					1 /* wait for start */,
					mi,
					master_info_file,relay_log_info_file,
					thread_mask);
    }
    else
      slave_errno = ER_BAD_SLAVE;
  }
  else
  {
    /* no error if all threads are already started, only a warning */
    push_warning(session, DRIZZLE_ERROR::WARN_LEVEL_NOTE, ER_SLAVE_WAS_RUNNING,
                 ER(ER_SLAVE_WAS_RUNNING));
  }

  unlock_slave_threads(mi);

  if (slave_errno)
  {
    if (net_report)
      my_message(slave_errno, ER(slave_errno), MYF(0));
    return(1);
  }
  else if (net_report)
    my_ok(session);

  return(0);
}


int stop_slave(Session* session, Master_info* mi, bool net_report )
{
  int slave_errno;
  if (!session)
    session = current_session;

  session->set_proc_info("Killing slave");
  int thread_mask;
  lock_slave_threads(mi);
  // Get a mask of _running_ threads
  init_thread_mask(&thread_mask,mi,0 /* not inverse*/);
  /*
    Below we will stop all running threads.
    But if the user wants to stop only one thread, do as if the other thread
    was stopped (as we don't wan't to touch the other thread), so set the
    bit to 0 for the other thread
  */
  if (session->lex->slave_session_opt)
    thread_mask &= session->lex->slave_session_opt;

  if (thread_mask)
  {
    slave_errno= terminate_slave_threads(mi,thread_mask,
                                         1 /*skip lock */);
  }
  else
  {
    //no error if both threads are already stopped, only a warning
    slave_errno= 0;
    push_warning(session, DRIZZLE_ERROR::WARN_LEVEL_NOTE, ER_SLAVE_WAS_NOT_RUNNING,
                 ER(ER_SLAVE_WAS_NOT_RUNNING));
  }
  unlock_slave_threads(mi);
  session->set_proc_info(0);

  if (slave_errno)
  {
    if (net_report)
      my_message(slave_errno, ER(slave_errno), MYF(0));
    return(1);
  }
  else if (net_report)
    my_ok(session);

  return(0);
}


/*
  Remove all relay logs and start replication from the start

  SYNOPSIS
    reset_slave()
    session			Thread handler
    mi			Master info for the slave

  RETURN
    0	ok
    1	error
*/


int reset_slave(Session *session, Master_info* mi)
{
  struct stat stat_area;
  char fname[FN_REFLEN];
  int thread_mask= 0, error= 0;
  uint32_t sql_errno=0;
  const char* errmsg=0;

  lock_slave_threads(mi);
  init_thread_mask(&thread_mask,mi,0 /* not inverse */);
  if (thread_mask) // We refuse if any slave thread is running
  {
    sql_errno= ER_SLAVE_MUST_STOP;
    error=1;
    goto err;
  }

  // delete relay logs, clear relay log coordinates
  if ((error= purge_relay_logs(&mi->rli, session,
			       1 /* just reset */,
			       &errmsg)))
    goto err;

  /* Clear master's log coordinates */
  mi->reset();
  /*
     Reset errors (the idea is that we forget about the
     old master).
  */
  mi->rli.clear_error();
  mi->rli.clear_until_condition();

  // close master_info_file, relay_log_info_file, set mi->inited=rli->inited=0
  mi->end_master_info();
  // and delete these two files
  fn_format(fname, master_info_file, drizzle_data_home, "", 4+32);
  if (!stat(fname, &stat_area) && my_delete(fname, MYF(MY_WME)))
  {
    error=1;
    goto err;
  }
  // delete relay_log_info_file
  fn_format(fname, relay_log_info_file, drizzle_data_home, "", 4+32);
  if (!stat(fname, &stat_area) && my_delete(fname, MYF(MY_WME)))
  {
    error=1;
    goto err;
  }

err:
  unlock_slave_threads(mi);
  if (error)
    my_error(sql_errno, MYF(0), errmsg);
  return(error);
}

/*

  Kill all Binlog_dump threads which previously talked to the same slave
  ("same" means with the same server id). Indeed, if the slave stops, if the
  Binlog_dump thread is waiting (pthread_cond_wait) for binlog update, then it
  will keep existing until a query is written to the binlog. If the master is
  idle, then this could last long, and if the slave reconnects, we could have 2
  Binlog_dump threads in SHOW PROCESSLIST, until a query is written to the
  binlog. To avoid this, when the slave reconnects and sends COM_BINLOG_DUMP,
  the master kills any existing thread with the slave's server id (if this id is
  not zero; it will be true for real slaves, but false for mysqlbinlog when it
  sends COM_BINLOG_DUMP to get a remote binlog dump).

  SYNOPSIS
    kill_zombie_dump_threads()
    slave_server_id     the slave's server id

*/


void kill_zombie_dump_threads(uint32_t)
{
}


bool change_master(Session* session, Master_info* mi)
{
  int thread_mask;
  const char* errmsg= 0;
  bool need_relay_log_purge= 1;

  lock_slave_threads(mi);
  init_thread_mask(&thread_mask,mi,0 /*not inverse*/);
  if (thread_mask) // We refuse if any slave thread is running
  {
    my_message(ER_SLAVE_MUST_STOP, ER(ER_SLAVE_MUST_STOP), MYF(0));
    unlock_slave_threads(mi);
    return(true);
  }

  session->set_proc_info("Changing master");
  LEX_MASTER_INFO* lex_mi= &session->lex->mi;
  // TODO: see if needs re-write
  if (mi->init_master_info(master_info_file, relay_log_info_file, thread_mask))
  {
    my_message(ER_MASTER_INFO, ER(ER_MASTER_INFO), MYF(0));
    unlock_slave_threads(mi);
    return(true);
  }

  /*
    Data lock not needed since we have already stopped the running threads,
    and we have the hold on the run locks which will keep all threads that
    could possibly modify the data structures from running
  */

  /*
    If the user specified host or port without binlog or position,
    reset binlog's name to FIRST and position to 4.
  */

  if ((lex_mi->host || lex_mi->port) && !lex_mi->log_file_name && !lex_mi->pos)
    mi->reset();

  if (lex_mi->log_file_name)
    mi->setLogName(lex_mi->log_file_name);
  if (lex_mi->pos)
  {
    mi->setLogPosition(lex_mi->pos);
  }

  if (lex_mi->host)
    mi->setHost(lex_mi->host, lex_mi->port);
  if (lex_mi->user)
    mi->setUsername(lex_mi->user);
  if (lex_mi->password)
    mi->setPassword(lex_mi->password);
  if (lex_mi->connect_retry)
    mi->connect_retry = lex_mi->connect_retry;
  if (lex_mi->heartbeat_opt != LEX_MASTER_INFO::LEX_MI_UNCHANGED)
    mi->heartbeat_period = lex_mi->heartbeat_period;
  else
    mi->heartbeat_period= (float) cmin((double)SLAVE_MAX_HEARTBEAT_PERIOD,
                                      (slave_net_timeout/2.0));
  mi->received_heartbeats= 0L; // counter lives until master is CHANGEd

  if (lex_mi->relay_log_name)
  {
    need_relay_log_purge= 0;
    mi->rli.event_relay_log_name.assign(lex_mi->relay_log_name);
  }

  if (lex_mi->relay_log_pos)
  {
    need_relay_log_purge= 0;
    mi->rli.group_relay_log_pos= mi->rli.event_relay_log_pos= lex_mi->relay_log_pos;
  }

  /*
    If user did specify neither host nor port nor any log name nor any log
    pos, i.e. he specified only user/password/master_connect_retry, he probably
    wants replication to resume from where it had left, i.e. from the
    coordinates of the **SQL** thread (imagine the case where the I/O is ahead
    of the SQL; restarting from the coordinates of the I/O would lose some
    events which is probably unwanted when you are just doing minor changes
    like changing master_connect_retry).
    A side-effect is that if only the I/O thread was started, this thread may
    restart from ''/4 after the CHANGE MASTER. That's a minor problem (it is a
    much more unlikely situation than the one we are fixing here).
    Note: coordinates of the SQL thread must be read here, before the
    'if (need_relay_log_purge)' block which resets them.
  */
  if (!lex_mi->host && !lex_mi->port &&
      !lex_mi->log_file_name && !lex_mi->pos &&
      need_relay_log_purge)
   {
     /*
       Sometimes mi->rli.master_log_pos == 0 (it happens when the SQL thread is
       not initialized), so we use a cmax().
       What happens to mi->rli.master_log_pos during the initialization stages
       of replication is not 100% clear, so we guard against problems using
       cmax().
      */
     mi->setLogPosition(((BIN_LOG_HEADER_SIZE > mi->rli.group_master_log_pos)
                         ? BIN_LOG_HEADER_SIZE
                         : mi->rli.group_master_log_pos));
     mi->setLogName(mi->rli.group_master_log_name.c_str());
  }
  /*
    Relay log's IO_CACHE may not be inited, if rli->inited==0 (server was never
    a slave before).
  */
  if (mi->flush())
  {
    my_error(ER_RELAY_LOG_INIT, MYF(0), "Failed to flush master info file");
    unlock_slave_threads(mi);
    return(true);
  }
  if (need_relay_log_purge)
  {
    relay_log_purge= 1;
    session->set_proc_info("Purging old relay logs");
    if (purge_relay_logs(&mi->rli, session,
			 0 /* not only reset, but also reinit */,
			 &errmsg))
    {
      my_error(ER_RELAY_LOG_FAIL, MYF(0), errmsg);
      unlock_slave_threads(mi);
      return(true);
    }
  }
  else
  {
    const char* msg;
    relay_log_purge= 0;
    /* Relay log is already initialized */
    if (init_relay_log_pos(&mi->rli,
			   mi->rli.group_relay_log_name.c_str(),
			   mi->rli.group_relay_log_pos,
			   0 /*no data lock*/,
			   &msg, 0))
    {
      my_error(ER_RELAY_LOG_INIT, MYF(0), msg);
      unlock_slave_threads(mi);
      return(true);
    }
  }
  /*
    Coordinates in rli were spoilt by the 'if (need_relay_log_purge)' block,
    so restore them to good values. If we left them to ''/0, that would work;
    but that would fail in the case of 2 successive CHANGE MASTER (without a
    START SLAVE in between): because first one would set the coords in mi to
    the good values of those in rli, the set those in rli to ''/0, then
    second CHANGE MASTER would set the coords in mi to those of rli, i.e. to
    ''/0: we have lost all copies of the original good coordinates.
    That's why we always save good coords in rli.
  */
  mi->rli.group_master_log_pos= mi->getLogPosition();
  mi->rli.group_master_log_name.assign(mi->getLogName());

  if (mi->rli.group_master_log_name.size() == 0) // uninitialized case
    mi->rli.group_master_log_pos= 0;

  pthread_mutex_lock(&mi->rli.data_lock);
  mi->rli.abort_pos_wait++; /* for MASTER_POS_WAIT() to abort */
  /* Clear the errors, for a clean start */
  mi->rli.clear_error();
  mi->rli.clear_until_condition();
  /*
    If we don't write new coordinates to disk now, then old will remain in
    relay-log.info until START SLAVE is issued; but if mysqld is shutdown
    before START SLAVE, then old will remain in relay-log.info, and will be the
    in-memory value at restart (thus causing errors, as the old relay log does
    not exist anymore).
  */
  flush_relay_log_info(&mi->rli);
  pthread_cond_broadcast(&mi->data_cond);
  pthread_mutex_unlock(&mi->rli.data_lock);

  unlock_slave_threads(mi);
  session->set_proc_info(0);
  my_ok(session);
  return(false);
}

int cmp_master_pos(const char* log_file_name1, uint64_t log_pos1,
		   const char* log_file_name2, uint64_t log_pos2)
{
  int res;
  uint32_t log_file_name1_len=  strlen(log_file_name1);
  uint32_t log_file_name2_len=  strlen(log_file_name2);

  //  We assume that both log names match up to '.'
  if (log_file_name1_len == log_file_name2_len)
  {
    if ((res= strcmp(log_file_name1, log_file_name2)))
      return res;
    return (log_pos1 < log_pos2) ? -1 : (log_pos1 == log_pos2) ? 0 : 1;
  }
  return ((log_file_name1_len < log_file_name2_len) ? -1 : 1);
}

/*
  Replication System Variables
*/

class sys_var_slave_skip_counter :public sys_var
{
public:
  sys_var_slave_skip_counter(sys_var_chain *chain, const char *name_arg)
    :sys_var(name_arg)
  { chain_sys_var(chain); }
  bool check(Session *session, set_var *var);
  bool update(Session *session, set_var *var);
  bool check_type(enum_var_type type) { return type != OPT_GLOBAL; }
  /*
    We can't retrieve the value of this, so we don't have to define
    type() or value_ptr()
  */
};

class sys_var_sync_binlog_period :public sys_var_long_ptr
{
public:
  sys_var_sync_binlog_period(sys_var_chain *chain, const char *name_arg,
                             uint64_t *value_ptr)
    :sys_var_long_ptr(chain, name_arg, value_ptr) {}
  bool update(Session *session, set_var *var);
};

static void fix_slave_net_timeout(Session *session,
                                  enum_var_type type __attribute__((unused)))
{
  pthread_mutex_lock(&LOCK_active_mi);
  if (active_mi && slave_net_timeout < active_mi->heartbeat_period)
    push_warning_printf(session, DRIZZLE_ERROR::WARN_LEVEL_WARN,
                        ER_SLAVE_HEARTBEAT_VALUE_OUT_OF_RANGE,
                        "The currect value for master_heartbeat_period"
                        " exceeds the new value of `slave_net_timeout' sec."
                        " A sensible value for the period should be"
                        " less than the timeout.");
  pthread_mutex_unlock(&LOCK_active_mi);
  return;
}

static sys_var_chain vars = { NULL, NULL };

static sys_var_bool_ptr	sys_relay_log_purge(&vars, "relay_log_purge",
					    &relay_log_purge);
static sys_var_long_ptr	sys_slave_net_timeout(&vars, "slave_net_timeout",
					      &slave_net_timeout,
                                              fix_slave_net_timeout);
static sys_var_long_ptr	sys_slave_trans_retries(&vars, "slave_transaction_retries", &slave_trans_retries);
static sys_var_sync_binlog_period sys_sync_binlog_period(&vars, "sync_binlog", &sync_binlog_period);
static sys_var_slave_skip_counter sys_slave_skip_counter(&vars, "sql_slave_skip_counter");

static int show_slave_skip_errors(Session *session, SHOW_VAR *var, char *buff);


static int show_slave_skip_errors(Session *session __attribute__((unused)),
                                  SHOW_VAR *var, char *buff)
{
  var->type=SHOW_CHAR;
  var->value= buff;
  if (!use_slave_mask || bitmap_is_clear_all(&slave_error_mask))
  {
    var->value= const_cast<char *>("OFF");
  }
  else if (bitmap_is_set_all(&slave_error_mask))
  {
    var->value= const_cast<char *>("ALL");
  }
  else
  {
    /* 10 is enough assuming errors are max 4 digits */
    int i;
    var->value= buff;
    for (i= 1;
         i < MAX_SLAVE_ERROR &&
         (buff - var->value) < SHOW_VAR_FUNC_BUFF_SIZE;
         i++)
    {
      if (bitmap_is_set(&slave_error_mask, i))
      {
        buff= int10_to_str(i, buff, 10);
        *buff++= ',';
      }
    }
    if (var->value != buff)
      buff--;				// Remove last ','
    if (i < MAX_SLAVE_ERROR)
      buff= strcpy(buff, "...")+3;  // Couldn't show all errors
    *buff=0;
  }
  return 0;
}

static st_show_var_func_container
show_slave_skip_errors_cont = { &show_slave_skip_errors };


static SHOW_VAR fixed_vars[]= {
  {"log_slave_updates",       (char*) &opt_log_slave_updates,       SHOW_MY_BOOL},
  {"relay_log" , (char*) &opt_relay_logname, SHOW_CHAR_PTR},
  {"relay_log_index", (char*) &opt_relaylog_index_name, SHOW_CHAR_PTR},
  {"relay_log_info_file", (char*) &relay_log_info_file, SHOW_CHAR_PTR},
  {"relay_log_space_limit",   (char*) &relay_log_space_limit,       SHOW_LONGLONG},
  {"slave_load_tmpdir",       (char*) &slave_load_tmpdir,           SHOW_CHAR_PTR},
  {"slave_skip_errors",       (char*) &show_slave_skip_errors_cont,      SHOW_FUNC},
};

bool sys_var_slave_skip_counter::check(Session *session __attribute__((unused)),
                                       set_var *var)
{
  int result= 0;
  pthread_mutex_lock(&LOCK_active_mi);
  pthread_mutex_lock(&active_mi->rli.run_lock);
  if (active_mi->rli.slave_running)
  {
    my_message(ER_SLAVE_MUST_STOP, ER(ER_SLAVE_MUST_STOP), MYF(0));
    result=1;
  }
  pthread_mutex_unlock(&active_mi->rli.run_lock);
  pthread_mutex_unlock(&LOCK_active_mi);
  var->save_result.uint32_t_value= (uint32_t) var->value->val_int();
  return result;
}


bool sys_var_slave_skip_counter::update(Session *session __attribute__((unused)),
                                        set_var *var)
{
  pthread_mutex_lock(&LOCK_active_mi);
  pthread_mutex_lock(&active_mi->rli.run_lock);
  /*
    The following test should normally never be true as we test this
    in the check function;  To be safe against multiple
    SQL_SLAVE_SKIP_COUNTER request, we do the check anyway
  */
  if (!active_mi->rli.slave_running)
  {
    pthread_mutex_lock(&active_mi->rli.data_lock);
    active_mi->rli.slave_skip_counter= var->save_result.uint32_t_value;
    pthread_mutex_unlock(&active_mi->rli.data_lock);
  }
  pthread_mutex_unlock(&active_mi->rli.run_lock);
  pthread_mutex_unlock(&LOCK_active_mi);
  return 0;
}


bool sys_var_sync_binlog_period::update(Session *session __attribute__((unused)),
                                        set_var *var)
{
  sync_binlog_period= (uint32_t) var->save_result.uint64_t_value;
  return 0;
}

int init_replication_sys_vars()
{
  mysql_append_static_vars(fixed_vars, sizeof(fixed_vars) / sizeof(SHOW_VAR));

  if (mysql_add_sys_var_chain(vars.first, my_long_options))
  {
    /* should not happen */
    fprintf(stderr, "failed to initialize replication system variables");
    unireg_abort(1);
  }
  return 0;
}
