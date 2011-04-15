/*
 * Copyright (C) 2010 Joseph Daly <skinny.moey@gmail.com>
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
 * This plugin tracks session and global statistics, as well as user statistics. 
 * The commands are logged using the post() and postEnd() logging APIs. 
 * The statistics are stored in a Scoreboard where each active session owns a 
 * ScoreboardSlot during the sessions active lifetime. 
 * 
 * Scoreboard
 *
 * The scoreboard is a pre-allocated vector of vectors of ScoreboardSlots. It 
 * can be thought of as a vector of buckets where each bucket contains 
 * pre-allocated ScoreboardSlots. To determine which bucket gets used for 
 * recording statistics the modulus operator is used on the session_id. This 
 * will result in a bucket to search for a unused ScoreboardSlot. Once a 
 * ScoreboardSlot is found the index of the slot is stored in the Session 
 * for later use. 
 *
 * Locking  
 * 
 * Each vector in the Scoreboard has its own lock. This allows session 2 
 * to not have to wait for session 1 to locate a slot to use, as they
 * will be in different buckets.  A lock is taken to locate a open slot
 * in the scoreboard. Subsequent queries by the session will not take
 * a lock.  
 *
 * A read lock is taken on the scoreboard vector when the table is queried 
 * in the data_dictionary. The "show status" and "show global status" do
 * not take a read lock when the data_dictionary table is queried, the 
 * user is not displayed in these results so it is not necessary. 
 *
 * Atomics
 *
 * The cumulative statistics use atomics, for the index into the vector
 * marking the last index that is used by a user. New users will increment
 * the atomic and claim the slot for use.  
 * 
 * System Variables
 * 
 * logging_stats_scoreboard_size - the size of the scoreboard this corresponds
 *   to the maximum number of concurrent connections that can be tracked
 *
 * logging_stats_max_user_count - this is used for cumulative statistics it 
 *   represents the maximum users that can be tracked 
 * 
 * logging_stats_bucket_count - the number of buckets to have in the scoreboard
 *   this splits up locking across several buckets so the entire scoreboard is 
 *   not locked at a single point in time.
 * 
 * logging_stats_enabled - enable/disable plugin 
 * 
 * TODO 
 *
 * Allow expansion of Scoreboard and cumulative vector 
 * 
 */

#include <config.h>
#include "user_commands.h"
#include "status_vars.h"
#include "global_stats.h"
#include "logging_stats.h"
#include "status_tool.h"
#include "stats_schema.h"
#include <boost/program_options.hpp>
#include <drizzled/module/option_map.h>
#include <drizzled/session.h>
#include <drizzled/session/times.h>
#include <drizzled/sql_lex.h>
#include <drizzled/statistics_variables.h>

namespace po= boost::program_options;
using namespace drizzled;
using namespace plugin;
using namespace std;

static bool sysvar_logging_stats_enabled= true;

typedef constrained_check<uint32_t, 50000, 10> scoreboard_size_constraint;
static scoreboard_size_constraint sysvar_logging_stats_scoreboard_size;

typedef constrained_check<uint32_t, 50000, 100> max_user_count_constraint;
static max_user_count_constraint sysvar_logging_stats_max_user_count;

typedef constrained_check<uint32_t, 500, 5> bucket_count_constraint;
static bucket_count_constraint sysvar_logging_stats_bucket_count;

LoggingStats::LoggingStats(string name_arg) : Logging(name_arg)
{
  current_scoreboard= new Scoreboard(sysvar_logging_stats_scoreboard_size, 
                                     sysvar_logging_stats_bucket_count);

  cumulative_stats= new CumulativeStats(sysvar_logging_stats_max_user_count); 
}

LoggingStats::~LoggingStats()
{
  delete current_scoreboard;
  delete cumulative_stats;
}

void LoggingStats::updateCurrentScoreboard(ScoreboardSlot *scoreboard_slot,
                                           Session *session)
{
  enum_sql_command sql_command= session->lex().sql_command;

  scoreboard_slot->getUserCommands()->logCommand(sql_command);

  /* If a flush occurred copy over values before setting new values */
  if (scoreboard_slot->getStatusVars()->hasBeenFlushed(session))
  {
    cumulative_stats->logGlobalStatusVars(scoreboard_slot);
  }
  scoreboard_slot->getStatusVars()->logStatusVar(session);
}

bool LoggingStats::resetGlobalScoreboard()
{
  cumulative_stats->getGlobalStatusVars()->reset();
  cumulative_stats->getGlobalStats()->getUserCommands()->reset();

  ScoreBoardVectors *vector_of_scoreboard_vectors=
    current_scoreboard->getVectorOfScoreboardVectors();

  ScoreBoardVectors::iterator v_of_scoreboard_v_begin_it= vector_of_scoreboard_vectors->begin();

  ScoreBoardVectors::iterator v_of_scoreboard_v_end_it= vector_of_scoreboard_vectors->end();

  for (; v_of_scoreboard_v_begin_it != v_of_scoreboard_v_end_it; ++v_of_scoreboard_v_begin_it)
  {
    std::vector<ScoreboardSlot* > *scoreboard_vector= *v_of_scoreboard_v_begin_it;

    std::vector<ScoreboardSlot* >::iterator scoreboard_vector_it= scoreboard_vector->begin();
    std::vector<ScoreboardSlot* >::iterator scoreboard_vector_end= scoreboard_vector->end();
    for (; scoreboard_vector_it != scoreboard_vector_end; ++scoreboard_vector_it)
    {
      ScoreboardSlot *scoreboard_slot= *scoreboard_vector_it;
      scoreboard_slot->getStatusVars()->reset();
      scoreboard_slot->getUserCommands()->reset();
    }
  }

  return false;
}

bool LoggingStats::post(Session *session)
{
  if (! isEnabled() || (session->getSessionId() == 0))
  {
    return false;
  }

  ScoreboardSlot *scoreboard_slot= current_scoreboard->findScoreboardSlotToLog(session);

  /* Its possible that the scoreboard is full with active sessions in which case 
     this could be null */
  if (scoreboard_slot)
  {
    updateCurrentScoreboard(scoreboard_slot, session);
  }
  return false;
}

bool LoggingStats::postEnd(Session *session)
{
  if (! isEnabled() || (session->getSessionId() == 0))
  {
    return false;
  }

  bool isInScoreboard= false;
  ScoreboardSlot *scoreboard_slot= current_scoreboard->findOurScoreboardSlot(session);

  if (scoreboard_slot)
  {
    isInScoreboard= true;
  } 
  else 
  { 
    /* the session did not have a slot reserved, that could be because the scoreboard was
       full, but most likely its a failed authentication so post() is never called where
       the slot is assigned. Log the global status values below, and if the user has a slot
       log to it, but do not reserve a new slot for a user. If it was a failed authentication
       the scoreboard would be filled up quickly with invalid users. 
    */
    scoreboard_slot= new ScoreboardSlot();
    scoreboard_slot->setUser(session->user()->username());
    scoreboard_slot->setIp(session->user()->address());
  }

  scoreboard_slot->getStatusVars()->logStatusVar(session);
  boost::posix_time::ptime end(boost::posix_time::microsec_clock::universal_time());
  uint64_t end_time= (end - session->times.epoch()).total_seconds();
  scoreboard_slot->getStatusVars()->getStatusVarCounters()->connection_time= end_time - session->times.getConnectSeconds();

  cumulative_stats->logUserStats(scoreboard_slot, isInScoreboard);
  cumulative_stats->logGlobalStats(scoreboard_slot);
  cumulative_stats->logGlobalStatusVars(scoreboard_slot);

  if (isInScoreboard)
  {
    scoreboard_slot->reset();
  } 
  else 
  {
    delete scoreboard_slot;
  } 

  return false;
}

/* Plugin initialization and system variables */

static LoggingStats *logging_stats= NULL;

static CurrentCommandsTool *current_commands_tool= NULL;

static CumulativeCommandsTool *cumulative_commands_tool= NULL;

static GlobalStatementsTool *global_statements_tool= NULL;

static SessionStatementsTool *session_statements_tool= NULL;

static StatusTool *global_status_tool= NULL;

static StatusTool *session_status_tool= NULL;

static CumulativeUserStatsTool *cumulative_user_stats_tool= NULL;

static ScoreboardStatsTool *scoreboard_stats_tool= NULL;

static void enable(Session *, sql_var_t)
{
  if (logging_stats)
  {
    if (sysvar_logging_stats_enabled)
    {
      logging_stats->enable();
    }
    else
    {
      logging_stats->disable();
    }
  }
}

static int init(drizzled::module::Context &context)
{
  const module::option_map &vm= context.getOptions();
  sysvar_logging_stats_enabled= not vm.count("disable");

  logging_stats= new LoggingStats("logging_stats");
  current_commands_tool= new CurrentCommandsTool(logging_stats);
  cumulative_commands_tool= new CumulativeCommandsTool(logging_stats);
  global_statements_tool= new GlobalStatementsTool(logging_stats);
  session_statements_tool= new SessionStatementsTool(logging_stats);
  session_status_tool= new StatusTool(logging_stats, true);
  global_status_tool= new StatusTool(logging_stats, false);
  cumulative_user_stats_tool= new CumulativeUserStatsTool(logging_stats);
  scoreboard_stats_tool= new ScoreboardStatsTool(logging_stats);

  context.add(logging_stats);
  context.add(current_commands_tool);
  context.add(cumulative_commands_tool);
  context.add(global_statements_tool);
  context.add(session_statements_tool);
  context.add(session_status_tool);
  context.add(global_status_tool);
  context.add(cumulative_user_stats_tool);
  context.add(scoreboard_stats_tool);

  if (sysvar_logging_stats_enabled)
    logging_stats->enable();

  context.registerVariable(new sys_var_constrained_value_readonly<uint32_t>("max_user_count", sysvar_logging_stats_max_user_count));
  context.registerVariable(new sys_var_constrained_value_readonly<uint32_t>("bucket_count", sysvar_logging_stats_bucket_count));
  context.registerVariable(new sys_var_constrained_value_readonly<uint32_t>("scoreboard_size", sysvar_logging_stats_scoreboard_size));
  context.registerVariable(new sys_var_bool_ptr("enable", &sysvar_logging_stats_enabled, enable));

  return 0;
}


static void init_options(drizzled::module::option_context &context)
{
  context("max-user-count",
          po::value<max_user_count_constraint>(&sysvar_logging_stats_max_user_count)->default_value(500),
          _("Max number of users that will be logged"));
  context("bucket-count",
          po::value<bucket_count_constraint>(&sysvar_logging_stats_bucket_count)->default_value(10),
          _("Max number of range locks to use for Scoreboard"));
  context("scoreboard-size",
          po::value<scoreboard_size_constraint>(&sysvar_logging_stats_scoreboard_size)->default_value(2000),
          _("Max number of concurrent sessions that will be logged"));
  context("disable", _("Enable Logging Statistics Collection"));
}

DRIZZLE_DECLARE_PLUGIN
{
  DRIZZLE_VERSION_ID,
  "logging_stats",
  "0.1",
  "Joseph Daly",
  N_("User Statistics as DATA_DICTIONARY tables"),
  PLUGIN_LICENSE_BSD,
  init,   /* Plugin Init      */
  NULL, /* depends */
  init_options    /* config options   */
}
DRIZZLE_DECLARE_PLUGIN_END;
