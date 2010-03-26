/*
 * Copyright (c) 2010, Joseph Daly <skinny.moey@gmail.com>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 *   * Redistributions of source code must retain the above copyright notice,
 *     this list of conditions and the following disclaimer.
 *   * Redistributions in binary form must reproduce the above copyright notice,
 *     this list of conditions and the following disclaimer in the documentation
 *     and/or other materials provided with the distribution.
 *   * Neither the name of Joseph Daly nor the names of its contributors
 *     may be used to endorse or promote products derived from this software
 *     without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

/**
 * @details
 *
 * This plugin tracks the current user commands, and copies them
 * into a cumulative vector of all commands run by a user over time. 
 * The commands are logged using the post() and postEnd() logging APIs.
 * It uses a scoreboard approach that initializes the scoreboard
 * size to the value set by logging_stats_scoreboard_size.
 * Each ScoreboardSlot wraps a UserCommand object containing the commands
 * for a particular session. As other statistics are added they
 * can then be added to the ScoreboardSlot object.
 *
 * Locking  
 *
 * A RW lock is taken to locate a open slot for a session or to locate the
 * slot that the current session has claimed.  
 * 
 * A read lock is taken when the table is queried in the data_dictionary.
 *
 * A RW lock is taken when a new user is added to the cumulative vector
 * repeat connections with a already used user will not use a lock. 
 *  
 * TODO 
 *
 * A pointer to the scoreboard slot could be added to the Session object.
 * This will avoid the session having to do multiple lookups in the scoreboard,
 * this will also avoid having to take a lock to locate the scoreboard slot 
 * being used by a particular session. 
 * 
 */

#include "config.h"
#include "logging_stats.h"
#include "stats_schema.h"
#include <drizzled/session.h>

using namespace drizzled;
using namespace plugin;
using namespace std;

static bool sysvar_logging_stats_enabled= false;

static uint32_t sysvar_logging_stats_scoreboard_size= 2000;

static uint32_t sysvar_logging_stats_max_user_count= 10000;

static uint32_t sysvar_logging_stats_bucket_count= 10;

pthread_rwlock_t LOCK_cumulative_scoreboard_index;

LoggingStats::LoggingStats(string name_arg) : Logging(name_arg)
{
  cumulative_stats_by_user_index= 0;

  current_scoreboard= new Scoreboard(sysvar_logging_stats_scoreboard_size, sysvar_logging_stats_bucket_count);

  cumulative_stats_by_user_vector= new vector<ScoreboardSlot *>(sysvar_logging_stats_max_user_count);
  preAllocateScoreboardSlotVector(sysvar_logging_stats_max_user_count, 
                              cumulative_stats_by_user_vector);
}

LoggingStats::~LoggingStats()
{
  deleteScoreboardSlotVector(cumulative_stats_by_user_vector);
  delete current_scoreboard;
}

void LoggingStats::preAllocateScoreboardSlotVector(uint32_t size, 
                                                   vector<ScoreboardSlot *> *scoreboard_slot_vector)
{
  vector<ScoreboardSlot *>::iterator it= scoreboard_slot_vector->begin();
  for (uint32_t j=0; j < size; j++)
  {
    ScoreboardSlot *scoreboard_slot= new ScoreboardSlot();
    it= scoreboard_slot_vector->insert(it, scoreboard_slot);
  }
  scoreboard_slot_vector->resize(size);
}

void LoggingStats::deleteScoreboardSlotVector(vector<ScoreboardSlot *> *scoreboard_slot_vector)
{
  vector<ScoreboardSlot *>::iterator it= scoreboard_slot_vector->begin();
  for (; it < scoreboard_slot_vector->end(); it++)
  {
    delete *it;
  }
  scoreboard_slot_vector->clear();
  delete scoreboard_slot_vector;
}

bool LoggingStats::isBeingLogged(Session *session)
{
  enum_sql_command sql_command= session->lex->sql_command;

  switch(sql_command)
  {
    case SQLCOM_UPDATE:
    case SQLCOM_DELETE:
    case SQLCOM_INSERT:
    case SQLCOM_ROLLBACK:
    case SQLCOM_COMMIT:
    case SQLCOM_CREATE_TABLE:
    case SQLCOM_ALTER_TABLE:
    case SQLCOM_DROP_TABLE:
    case SQLCOM_SELECT:
      return true;
    default:
      return false;
  }
} 

void LoggingStats::updateCurrentScoreboard(ScoreboardSlot *scoreboard_slot,
                                           Session *session)
{
  enum_sql_command sql_command= session->lex->sql_command;

  UserCommands *user_commands= scoreboard_slot->getUserCommands();

  switch(sql_command)
  {
    case SQLCOM_UPDATE:
      user_commands->incrementUpdateCount();
      break;
    case SQLCOM_DELETE:
      user_commands->incrementDeleteCount();
      break;
    case SQLCOM_INSERT:
      user_commands->incrementInsertCount();
      break;
    case SQLCOM_ROLLBACK:
      user_commands->incrementRollbackCount();
      break;
    case SQLCOM_COMMIT:
      user_commands->incrementCommitCount();
      break;
    case SQLCOM_CREATE_TABLE:
      user_commands->incrementCreateCount();
      break;
    case SQLCOM_ALTER_TABLE:
      user_commands->incrementAlterCount();
      break;
    case SQLCOM_DROP_TABLE:
      user_commands->incrementDropCount();
      break;
    case SQLCOM_SELECT:
      user_commands->incrementSelectCount();
      break;
    default:
      return;
  }
}

bool LoggingStats::post(Session *session)
{
  if (! isEnabled() || (session->getSessionId() == 0))
  {
    return false;
  }

  /* exit early if we are not logging this type of command */
  if (isBeingLogged(session) == false)
  {
    return false;
  }

  ScoreboardSlot *scoreboard_slot= current_scoreboard->findScoreboardSlotToLog(session);

  updateCurrentScoreboard(scoreboard_slot, session);

  return false;
}

bool LoggingStats::postEnd(Session *session)
{
  if (! isEnabled() || (session->getSessionId() == 0))
  {
    return false;
  }

  ScoreboardSlot *scoreboard_slot= current_scoreboard->findAndResetScoreboardSlot(session);

  if (scoreboard_slot)
  {
    vector<ScoreboardSlot *>::iterator cumulative_it= cumulative_stats_by_user_vector->begin();
    bool found= false;
    for (uint32_t h= 0; h < cumulative_stats_by_user_index; h++)
    {
      ScoreboardSlot *cumulative_scoreboard_slot= *cumulative_it;
      string user= cumulative_scoreboard_slot->getUser();
      if (user.compare(scoreboard_slot->getUser()) == 0)
      {
        found= true;
        cumulative_scoreboard_slot->merge(scoreboard_slot);
        break;
      }
      cumulative_it++;
    }

    if (! found)
    {
      updateCumulativeStatsByUserVector(scoreboard_slot);
    }
    delete scoreboard_slot;
  }

  return false;
}

void LoggingStats::updateCumulativeStatsByUserVector(ScoreboardSlot *current_scoreboard_slot)
{
  pthread_rwlock_wrlock(&LOCK_cumulative_scoreboard_index);
  ScoreboardSlot *cumulative_scoreboard_slot=
    cumulative_stats_by_user_vector->at(cumulative_stats_by_user_index);
  string cumulative_scoreboard_user(current_scoreboard_slot->getUser());
  cumulative_scoreboard_slot->setUser(cumulative_scoreboard_user);
  cumulative_scoreboard_slot->merge(current_scoreboard_slot);
  cumulative_stats_by_user_index++;
  pthread_rwlock_unlock(&LOCK_cumulative_scoreboard_index);
}


/* Plugin initialization and system variables */

static LoggingStats *logging_stats= NULL;

static CurrentCommandsTool *current_commands_tool= NULL;

static CumulativeCommandsTool *cumulative_commands_tool= NULL;

static void enable(Session *,
                   drizzle_sys_var *,
                   void *var_ptr,
                   const void *save)
{
  if (logging_stats)
  {
    if (*(bool *)save != false)
    {
      logging_stats->enable();
      *(bool *) var_ptr= (bool) true;
    }
    else
    {
      logging_stats->disable();
      *(bool *) var_ptr= (bool) false;
    }
  }
}

static bool initTable()
{
  current_commands_tool= new(nothrow)CurrentCommandsTool(logging_stats);

  if (! current_commands_tool)
  {
    return true;
  }

  cumulative_commands_tool= new(nothrow)CumulativeCommandsTool(logging_stats);

  if (! cumulative_commands_tool)
  {
    return true;
  }

  return false;
}

static int init(Context &context)
{
  logging_stats= new LoggingStats("logging_stats");

  if (initTable())
  {
    return 1;
  }

  context.add(logging_stats);
  context.add(current_commands_tool);
  context.add(cumulative_commands_tool);

  if (sysvar_logging_stats_enabled)
  {
    logging_stats->enable();
  }

  return 0;
}

static DRIZZLE_SYSVAR_UINT(max_user_count,
                           sysvar_logging_stats_max_user_count,
                           PLUGIN_VAR_RQCMDARG,
                           N_("Max number of users that will be logged"),
                           NULL, /* check func */
                           NULL, /* update func */
                           10000, /* default */
                           500, /* minimum */
                           50000,
                           0);

static DRIZZLE_SYSVAR_UINT(max_bucket_count,
                           sysvar_logging_stats_bucket_count,
                           PLUGIN_VAR_RQCMDARG,
                           N_("Max number of vector buckets to construct for logging"),
                           NULL, /* check func */
                           NULL, /* update func */
                           10, /* default */
                           5, /* minimum */
                           100,
                           0);

static DRIZZLE_SYSVAR_UINT(scoreboard_size,
                           sysvar_logging_stats_scoreboard_size,
                           PLUGIN_VAR_RQCMDARG,
                           N_("Max number of concurrent sessions that will be logged"),
                           NULL, /* check func */
                           NULL, /* update func */
                           2000, /* default */
                           1000, /* minimum */
                           50000, 
                           0);

static DRIZZLE_SYSVAR_BOOL(enable,
                           sysvar_logging_stats_enabled,
                           PLUGIN_VAR_NOCMDARG,
                           N_("Enable Logging Statistics Collection"),
                           NULL, /* check func */
                           enable, /* update func */
                           false /* default */);

static drizzle_sys_var* system_var[]= {
  DRIZZLE_SYSVAR(max_user_count),
  DRIZZLE_SYSVAR(max_bucket_count),
  DRIZZLE_SYSVAR(scoreboard_size),
  DRIZZLE_SYSVAR(enable),
  NULL
};

DRIZZLE_DECLARE_PLUGIN
{
  DRIZZLE_VERSION_ID,
  "logging_stats",
  "0.1",
  "Joseph Daly",
  N_("User Statistics as DATA_DICTIONARY tables"),
  PLUGIN_LICENSE_BSD,
  init,   /* Plugin Init      */
  system_var, /* system variables */
  NULL    /* config options   */
}
DRIZZLE_DECLARE_PLUGIN_END;
