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
 * This class defines the following DATA_DICTIONARY tables:
 *
 * drizzle> describe GLOBAL_STATEMENTS;
 * +----------------+---------+-------+---------+-----------------+-----------+
 * | Field          | Type    | Null  | Default | Default_is_NULL | On_Update |
 * +----------------+---------+-------+---------+-----------------+-----------+
 * | VARIABLE_NAME  | VARCHAR | FALSE |         | FALSE           |           |
 * | VARIABLE_VALUE | BIGINT  | FALSE |         | FALSE           |           |
 * +----------------+---------+-------+---------+-----------------+-----------+
 *
 * drizzle> describe SESSION_STATEMENTS;
 * +----------------+---------+-------+---------+-----------------+-----------+
 * | Field          | Type    | Null  | Default | Default_is_NULL | On_Update |
 * +----------------+---------+-------+---------+-----------------+-----------+
 * | VARIABLE_NAME  | VARCHAR | FALSE |         | FALSE           |           |
 * | VARIABLE_VALUE | BIGINT  | FALSE |         | FALSE           |           |
 * +----------------+---------+-------+---------+-----------------+-----------+
 *
 * drizzle> describe CURRENT_SQL_COMMANDS;
 * +----------------+---------+-------+---------+-----------------+-----------+
 * | Field          | Type    | Null  | Default | Default_is_NULL | On_Update |
 * +----------------+---------+-------+---------+-----------------+-----------+
 * | USERNAME       | VARCHAR | FALSE |         | FALSE           |           |
 * | IP             | VARCHAR | FALSE |         | FALSE           |           |
 * | COUNT_SELECT   | BIGINT  | FALSE |         | FALSE           |           |
 * | COUNT_DELETE   | BIGINT  | FALSE |         | FALSE           |           |
 * | COUNT_UPDATE   | BIGINT  | FALSE |         | FALSE           |           |
 * | COUNT_INSERT   | BIGINT  | FALSE |         | FALSE           |           |
 * | COUNT_ROLLBACK | BIGINT  | FALSE |         | FALSE           |           |
 * | COUNT_COMMIT   | BIGINT  | FALSE |         | FALSE           |           |
 * | COUNT_CREATE   | BIGINT  | FALSE |         | FALSE           |           |
 * | COUNT_ALTER    | BIGINT  | FALSE |         | FALSE           |           |
 * | COUNT_DROP     | BIGINT  | FALSE |         | FALSE           |           |
 * | COUNT_ADMIN    | BIGINT  | FALSE |         | FALSE           |           |
 * +----------------+---------+-------+---------+-----------------+-----------+
 *
 * drizzle> describe CUMULATIVE_SQL_COMMANDS;
 * +----------------+---------+-------+---------+-----------------+-----------+
 * | Field          | Type    | Null  | Default | Default_is_NULL | On_Update |
 * +----------------+---------+-------+---------+-----------------+-----------+
 * | USERNAME       | VARCHAR | FALSE |         | FALSE           |           |
 * | COUNT_SELECT   | BIGINT  | FALSE |         | FALSE           |           |
 * | COUNT_DELETE   | BIGINT  | FALSE |         | FALSE           |           |
 * | COUNT_UPDATE   | BIGINT  | FALSE |         | FALSE           |           |
 * | COUNT_INSERT   | BIGINT  | FALSE |         | FALSE           |           |
 * | COUNT_ROLLBACK | BIGINT  | FALSE |         | FALSE           |           |
 * | COUNT_COMMIT   | BIGINT  | FALSE |         | FALSE           |           |
 * | COUNT_CREATE   | BIGINT  | FALSE |         | FALSE           |           |
 * | COUNT_ALTER    | BIGINT  | FALSE |         | FALSE           |           |
 * | COUNT_DROP     | BIGINT  | FALSE |         | FALSE           |           |
 * | COUNT_ADMIN    | BIGINT  | FALSE |         | FALSE           |           |
 * +----------------+---------+-------+---------+-----------------+-----------+
 *
 * drizzle> describe CUMULATIVE_USER_STATS;
 * +---------------------+---------+-------+---------+-----------------+-----------+
 * | Field               | Type    | Null  | Default | Default_is_NULL | On_Update |
 * +---------------------+---------+-------+---------+-----------------+-----------+
 * | USERNAME            | VARCHAR | FALSE |         | FALSE           |           | 
 * | BYTES_RECEIVED      | VARCHAR | FALSE |         | FALSE           |           | 
 * | BYTES_SENT          | VARCHAR | FALSE |         | FALSE           |           | 
 * | DENIED_CONNECTIONS  | VARCHAR | FALSE |         | FALSE           |           | 
 * | LOST_CONNECTIONS    | VARCHAR | FALSE |         | FALSE           |           | 
 * | ACCESS_DENIED       | VARCHAR | FALSE |         | FALSE           |           | 
 * | CONNECTED_TIME_SEC  | VARCHAR | FALSE |         | FALSE           |           | 
 * | EXECUTION_TIME_NSEC | VARCHAR | FALSE |         | FALSE           |           | 
 * | ROWS_FETCHED        | VARCHAR | FALSE |         | FALSE           |           | 
 * | ROWS_UPDATED        | VARCHAR | FALSE |         | FALSE           |           | 
 * | ROWS_DELETED        | VARCHAR | FALSE |         | FALSE           |           | 
 * | ROWS_INSERTED       | VARCHAR | FALSE |         | FALSE           |           | 
 * +---------------------+---------+-------+---------+-----------------+-----------+
 *
 */

#include <config.h>
#include <drizzled/statistics_variables.h>
#include "stats_schema.h"
#include <sstream>

using namespace drizzled;
using namespace plugin;
using namespace std;

SessionStatementsTool::SessionStatementsTool(LoggingStats *in_logging_stats) :
  plugin::TableFunction("DATA_DICTIONARY", "SESSION_STATEMENTS")
{
  logging_stats= in_logging_stats;
  add_field("VARIABLE_NAME");
  add_field("VARIABLE_VALUE", 1024);
}

SessionStatementsTool::Generator::Generator(Field **arg, LoggingStats *in_logging_stats) :
  plugin::TableFunction::Generator(arg)
{
  count= 0;

  /* Set user_commands */
  Scoreboard *current_scoreboard= in_logging_stats->getCurrentScoreboard();

  uint32_t bucket_number= current_scoreboard->getBucketNumber(&getSession());

  std::vector<ScoreboardSlot* > *scoreboard_vector=
     current_scoreboard->getVectorOfScoreboardVectors()->at(bucket_number);

  ScoreboardSlot *scoreboard_slot= NULL;
  for (std::vector<ScoreboardSlot *>::iterator it= scoreboard_vector->begin();
       it != scoreboard_vector->end(); ++it)
  {
    scoreboard_slot= *it;
    if (scoreboard_slot->getSessionId() == getSession().getSessionId())
    {
      break;
    }
  }

  user_commands= NULL;

  if (scoreboard_slot != NULL)
  {
    user_commands= scoreboard_slot->getUserCommands();
  }
}

bool SessionStatementsTool::Generator::populate()
{
  if (user_commands == NULL)
  {
    return false;
  } 

  uint32_t number_identifiers= UserCommands::getStatusVarsCount();

  if (count == number_identifiers)
  {
    return false;
  }

  push(UserCommands::COM_STATUS_VARS[count]);
  ostringstream oss;
  oss << user_commands->getCount(count);
  push(oss.str()); 

  ++count;
  return true;
}

GlobalStatementsTool::GlobalStatementsTool(LoggingStats *in_logging_stats) :
  plugin::TableFunction("DATA_DICTIONARY", "GLOBAL_STATEMENTS")
{   
  logging_stats= in_logging_stats;
  add_field("VARIABLE_NAME");
  add_field("VARIABLE_VALUE", 1024);
}

GlobalStatementsTool::Generator::Generator(Field **arg, LoggingStats *in_logging_stats) :
  plugin::TableFunction::Generator(arg)
{
  count= 0;
  /* add the current scoreboard and the saved global statements */
  global_stats_to_display= new GlobalStats();
  CumulativeStats *cumulativeStats= in_logging_stats->getCumulativeStats();
  cumulativeStats->sumCurrentScoreboard(in_logging_stats->getCurrentScoreboard(), 
                                        NULL, global_stats_to_display->getUserCommands());
  global_stats_to_display->merge(in_logging_stats->getCumulativeStats()->getGlobalStats()); 
}

GlobalStatementsTool::Generator::~Generator()
{
  delete global_stats_to_display;
}

bool GlobalStatementsTool::Generator::populate()
{
  uint32_t number_identifiers= UserCommands::getStatusVarsCount(); 
  if (count == number_identifiers)
  {
    return false;
  }

  push(UserCommands::COM_STATUS_VARS[count]);
  ostringstream oss;
  oss << global_stats_to_display->getUserCommands()->getCount(count);
  push(oss.str());

  ++count;
  return true;
}

CurrentCommandsTool::CurrentCommandsTool(LoggingStats *logging_stats) :
  plugin::TableFunction("DATA_DICTIONARY", "CURRENT_SQL_COMMANDS")
{
  outer_logging_stats= logging_stats;

  add_field("USERNAME");
  add_field("IP");

  uint32_t number_commands= UserCommands::getUserCounts();

  for (uint32_t j= 0; j < number_commands; ++j)
  {
    add_field(UserCommands::USER_COUNTS[j], TableFunction::NUMBER);
  } 
}

CurrentCommandsTool::Generator::Generator(Field **arg, LoggingStats *logging_stats) :
  plugin::TableFunction::Generator(arg)
{
  inner_logging_stats= logging_stats;

  isEnabled= inner_logging_stats->isEnabled();

  if (isEnabled == false)
  {
    return;
  }

  current_scoreboard= logging_stats->getCurrentScoreboard();
  current_bucket= 0;

  vector_of_scoreboard_vectors_it= current_scoreboard->getVectorOfScoreboardVectors()->begin();
  vector_of_scoreboard_vectors_end= current_scoreboard->getVectorOfScoreboardVectors()->end();

  setVectorIteratorsAndLock(current_bucket);
}

void CurrentCommandsTool::Generator::setVectorIteratorsAndLock(uint32_t bucket_number)
{
  std::vector<ScoreboardSlot* > *scoreboard_vector= 
    current_scoreboard->getVectorOfScoreboardVectors()->at(bucket_number); 

  current_lock= current_scoreboard->getVectorOfScoreboardLocks()->at(bucket_number);

  scoreboard_vector_it= scoreboard_vector->begin();
  scoreboard_vector_end= scoreboard_vector->end();
  current_lock->lock_shared();
}

bool CurrentCommandsTool::Generator::populate()
{
  if (isEnabled == false)
  {
    return false;
  }

  while (vector_of_scoreboard_vectors_it != vector_of_scoreboard_vectors_end)
  {
    while (scoreboard_vector_it != scoreboard_vector_end)
    {
      ScoreboardSlot *scoreboard_slot= *scoreboard_vector_it; 
      if (scoreboard_slot->isInUse())
      {
        UserCommands *user_commands= scoreboard_slot->getUserCommands();
        push(scoreboard_slot->getUser());
        push(scoreboard_slot->getIp());

        uint32_t number_commands= UserCommands::getUserCounts(); 

        for (uint32_t j= 0; j < number_commands; ++j)
        {
          push(user_commands->getUserCount(j));
        }

        ++scoreboard_vector_it;
        return true;
      }
      ++scoreboard_vector_it;
    }
    
    ++vector_of_scoreboard_vectors_it;
    current_lock->unlock_shared();
    ++current_bucket;
    if (vector_of_scoreboard_vectors_it != vector_of_scoreboard_vectors_end)
    {
      setVectorIteratorsAndLock(current_bucket); 
    } 
  }

  return false;
}

CumulativeCommandsTool::CumulativeCommandsTool(LoggingStats *logging_stats) :
  plugin::TableFunction("DATA_DICTIONARY", "CUMULATIVE_SQL_COMMANDS")
{
  outer_logging_stats= logging_stats;

  add_field("USERNAME");

  uint32_t number_commands= UserCommands::getUserCounts();

  for (uint32_t j= 0; j < number_commands; ++j)
  {
    add_field(UserCommands::USER_COUNTS[j], TableFunction::NUMBER);
  }
}

CumulativeCommandsTool::Generator::Generator(Field **arg, LoggingStats *logging_stats) :
  plugin::TableFunction::Generator(arg)
{
  inner_logging_stats= logging_stats;
  record_number= 0;

  if (inner_logging_stats->isEnabled())
  {
    last_valid_index= inner_logging_stats->getCumulativeStats()->getCumulativeStatsLastValidIndex();
  }
  else
  {
    last_valid_index= INVALID_INDEX; 
  }
}

bool CumulativeCommandsTool::Generator::populate()
{
  if ((record_number > last_valid_index) || (last_valid_index == INVALID_INDEX))
  {
    return false;
  }

  while (record_number <= last_valid_index)
  {
    ScoreboardSlot *cumulative_scoreboard_slot= 
      inner_logging_stats->getCumulativeStats()->getCumulativeStatsByUserVector()->at(record_number);

    if (cumulative_scoreboard_slot->isInUse())
    {
      UserCommands *user_commands= cumulative_scoreboard_slot->getUserCommands(); 
      push(cumulative_scoreboard_slot->getUser());

      uint32_t number_commands= UserCommands::getUserCounts();

      for (uint32_t j= 0; j < number_commands; ++j)
      {
        push(user_commands->getUserCount(j));
      }
      ++record_number;
      return true;
    } 
    else 
    {
      ++record_number;
    }
  }

  return false;
}

CumulativeUserStatsTool::CumulativeUserStatsTool(LoggingStats *logging_stats) :
  plugin::TableFunction("DATA_DICTIONARY", "CUMULATIVE_USER_STATS")
{
  outer_logging_stats= logging_stats;

  add_field("USERNAME");
  add_field("BYTES_RECEIVED");
  add_field("BYTES_SENT");
  add_field("DENIED_CONNECTIONS");
  add_field("LOST_CONNECTIONS");
  add_field("ACCESS_DENIED");
  add_field("CONNECTED_TIME_SEC");
  add_field("EXECUTION_TIME_NSEC");
  add_field("ROWS_FETCHED");
  add_field("ROWS_UPDATED");
  add_field("ROWS_DELETED");
  add_field("ROWS_INSERTED");
}

CumulativeUserStatsTool::Generator::Generator(Field **arg, LoggingStats *logging_stats) :
  plugin::TableFunction::Generator(arg)
{
  inner_logging_stats= logging_stats;
  record_number= 0;

  if (inner_logging_stats->isEnabled())
  {
    last_valid_index= inner_logging_stats->getCumulativeStats()->getCumulativeStatsLastValidIndex();
  }
  else
  {
    last_valid_index= INVALID_INDEX;
  }
}

bool CumulativeUserStatsTool::Generator::populate()
{
  if ((record_number > last_valid_index) || (last_valid_index == INVALID_INDEX))
  {
    return false;
  }

  while (record_number <= last_valid_index)
  {
    ScoreboardSlot *cumulative_scoreboard_slot=
      inner_logging_stats->getCumulativeStats()->getCumulativeStatsByUserVector()->at(record_number);

    if (cumulative_scoreboard_slot->isInUse())
    {
      StatusVars *status_vars= cumulative_scoreboard_slot->getStatusVars();
      push(cumulative_scoreboard_slot->getUser());

      push(status_vars->getStatusVarCounters()->bytes_received);
      push(status_vars->getStatusVarCounters()->bytes_sent);
      push(status_vars->getStatusVarCounters()->aborted_connects);
      push(status_vars->getStatusVarCounters()->aborted_threads);
      push(status_vars->getStatusVarCounters()->access_denied);
      push(status_vars->getStatusVarCounters()->connection_time);
      push(status_vars->getStatusVarCounters()->execution_time_nsec);
      push(status_vars->sent_row_count);
      push(status_vars->getStatusVarCounters()->updated_row_count);
      push(status_vars->getStatusVarCounters()->deleted_row_count);
      push(status_vars->getStatusVarCounters()->inserted_row_count);

      ++record_number;
      return true;
    }
    else
    {
      ++record_number;
    }
  }

  return false;
}

ScoreboardStatsTool::ScoreboardStatsTool(LoggingStats *logging_stats) :
  plugin::TableFunction("DATA_DICTIONARY", "SCOREBOARD_STATISTICS")
{
  outer_logging_stats= logging_stats;

  add_field("SCOREBOARD_SIZE", TableFunction::NUMBER);
  add_field("NUMBER_OF_RANGE_LOCKS", TableFunction::NUMBER);
  add_field("MAX_USERS_LOGGED", TableFunction::NUMBER);
  add_field("MEMORY_USAGE_BYTES", TableFunction::NUMBER);
}

ScoreboardStatsTool::Generator::Generator(Field **arg, LoggingStats *logging_stats) :
  plugin::TableFunction::Generator(arg)
{
  inner_logging_stats= logging_stats;
  is_last_record= false; 
}

bool ScoreboardStatsTool::Generator::populate()
{
  if (is_last_record)
  {
    return false;
  }

  Scoreboard *scoreboard= inner_logging_stats->getCurrentScoreboard();
  CumulativeStats *cumulativeStats= inner_logging_stats->getCumulativeStats();

  push(static_cast<uint64_t>(scoreboard->getNumberPerBucket() * scoreboard->getNumberBuckets()));
  push(static_cast<uint64_t>(scoreboard->getNumberBuckets()));
  push(static_cast<uint64_t>(cumulativeStats->getCumulativeStatsByUserMax()));
  push(cumulativeStats->getCumulativeSizeBytes() + scoreboard->getScoreboardSizeBytes()); 
  
  is_last_record= true;

  return true;
}
