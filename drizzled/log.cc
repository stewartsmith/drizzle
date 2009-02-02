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
#include <drizzled/session.h>
#include <drizzled/handler.h>

/* max size of the log message */
#define MY_OFF_T_UNDEF (~(my_off_t)0UL)

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

drizzle_declare_plugin(binlog)
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
drizzle_declare_plugin_end;
