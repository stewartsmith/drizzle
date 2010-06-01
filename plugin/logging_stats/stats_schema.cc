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
 * This class defines the following DATA_DICTIONARY tables:
 *
 * drizzle> describe GLOBAL_STATEMENTS_NEW;
 * +----------------+---------+-------+---------+-----------------+-----------+
 * | Field          | Type    | Null  | Default | Default_is_NULL | On_Update |
 * +----------------+---------+-------+---------+-----------------+-----------+
 * | VARIABLE_NAME  | VARCHAR | FALSE |         | FALSE           |           |
 * | VARIABLE_VALUE | BIGINT  | FALSE |         | FALSE           |           |
 * +----------------+---------+-------+---------+-----------------+-----------+
 *
 * drizzle> describe SESSION_STATEMENTS_NEW;
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
 * | USER           | VARCHAR | FALSE |         | FALSE           |           |
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
 * | USER           | VARCHAR | FALSE |         | FALSE           |           |
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
 */

#include <config.h>          
#include "stats_schema.h"
#include "scoreboard.h"
#include "status_vars.h"

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

SessionStatementsTool::Generator::Generator(Field **arg, LoggingStats *logging_stats) :
  plugin::TableFunction::Generator(arg)
{
  count= 0;

  /* Set user_commands */
  Scoreboard *current_scoreboard= logging_stats->getCurrentScoreboard();

  Session *this_session= current_session;

  ScoreboardSlot *scoreboard_slot= current_scoreboard->findOurScoreboardSlot(this_session);

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

GlobalStatementsTool::Generator::Generator(Field **arg, LoggingStats *logging_stats) :
  plugin::TableFunction::Generator(arg)
{
  count= 0;
  global_stats= logging_stats->getCumulativeStats()->getGlobalStats();
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
  oss << global_stats->getUserCommands()->getCount(count);
  push(oss.str());

  ++count;
  return true;
}

CurrentCommandsTool::CurrentCommandsTool(LoggingStats *logging_stats) :
  plugin::TableFunction("DATA_DICTIONARY", "CURRENT_SQL_COMMANDS")
{
  outer_logging_stats= logging_stats;

  add_field("USER");
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
  vector<ScoreboardSlot* > *scoreboard_vector= 
    current_scoreboard->getVectorOfScoreboardVectors()->at(bucket_number); 

  current_lock= current_scoreboard->getVectorOfScoreboardLocks()->at(bucket_number);

  scoreboard_vector_it= scoreboard_vector->begin();
  scoreboard_vector_end= scoreboard_vector->end();
  pthread_rwlock_rdlock(current_lock);
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
    pthread_rwlock_unlock(current_lock); 
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

  add_field("USER");

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

StatusVarTool::StatusVarTool(LoggingStats *logging_stats) :
  plugin::TableFunction("DATA_DICTIONARY", "CUMULATIVE_USER_STATS")
{
  outer_logging_stats= logging_stats;

  add_field("USER");
  add_field("BYTES_RECEIVED", TableFunction::NUMBER);
  add_field("BYTES_SENT", TableFunction::NUMBER);
}

StatusVarTool::Generator::Generator(Field **arg, LoggingStats *logging_stats) :
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

bool StatusVarTool::Generator::populate()
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

class show_var_cmp_functor
{
  public:
  show_var_cmp_functor() { }
  inline bool operator()(const drizzle_show_var *var1, const drizzle_show_var *var2) const
  {
    int val= strcmp(var1->name, var2->name);
    return (val < 0);
  }
};



SessionStatusTool::SessionStatusTool(LoggingStats *logging_stats) :
  plugin::TableFunction("DATA_DICTIONARY", "SESSION_STATUS_NEW")
{
  outer_logging_stats= logging_stats;

  add_field("VARIABLE_NAME");
  add_field("VARIABLE_VALUE", 1024);

  init();
}

void SessionStatusTool::init()
{
  vector<drizzle_show_var* >::iterator all_status_vars_iterator= all_status_vars.begin();

  drizzle_show_var *var= &StatusVars::status_vars_defs[0];

  uint32_t count= 1;
  while (var != NULL)
  {
    if (var->name == NULL)
    {   
      break;
    }   

    all_status_vars_iterator= all_status_vars.insert(all_status_vars_iterator, 
                                                     var);
    var= &StatusVars::status_vars_defs[count];
    ++count;
  }
  sort(all_status_vars.begin(), all_status_vars.end(), show_var_cmp_functor());
}

SessionStatusTool::Generator::Generator(Field **arg, LoggingStats *logging_stats, 
                                        vector<drizzle_show_var *> *all_status_vars) :
  plugin::TableFunction::Generator(arg)
{
  all_status_vars_it= all_status_vars->begin();
  all_status_vars_end= all_status_vars->end();

  Session *this_session= current_session;

  ScoreboardSlot *scoreboard_slot= logging_stats->getCurrentScoreboard()->findOurScoreboardSlot(this_session);

  status_vars= NULL;

  if (scoreboard_slot != NULL)
  {
    status_vars= scoreboard_slot->getStatusVars();
  }
}

bool SessionStatusTool::Generator::populate()
{
  if (status_vars == NULL)
  {
    return false;
  }

  while (all_status_vars_it != all_status_vars_end)
  {
    drizzle_show_var *variables= *all_status_vars_it;     

    if ((variables == NULL) || (variables->name == NULL))
    {
      return false;
    }

    drizzle_show_var *var;
    MY_ALIGNED_BYTE_ARRAY(buff_data, SHOW_VAR_FUNC_BUFF_SIZE, int64_t);
    char * const buff= (char *) &buff_data;

    /*
      if var->type is SHOW_FUNC, call the function.
      Repeat as necessary, if new var is again SHOW_FUNC
    */
    {
      drizzle_show_var tmp;

      for (var= variables; var->type == SHOW_FUNC; var= &tmp)
        ((mysql_show_var_func)((st_show_var_func_container *)var->value)->func)(&tmp, buff);
    }

    if (isWild(variables->name))
    {
      variables++;
      continue;
    }

    fill(variables->name, var->value, var->type);

    ++all_status_vars_it;

    return true;
  }

  return false;
}

void SessionStatusTool::Generator::fill(const string &name, char *value, SHOW_TYPE show_type)
{
  struct system_status_var *status_var;
  std::ostringstream oss;

  std::string return_value;
  
  status_var= status_vars->getStatusVarCounters();

  if (show_type == SHOW_SYS)
  {
    show_type= ((sys_var*) value)->show_type();
    value= (char*) ((sys_var*) value)->value_ptr(&(getSession()), OPT_SESSION,
                                                 &null_lex_str);
  }

  /*
    note that value may be == buff. All SHOW_xxx code below
    should still work in this case
  */
  switch (show_type) 
  {
  case SHOW_DOUBLE_STATUS:
    value= ((char *) status_var + (ulong) value);
    /* fall through */
  case SHOW_DOUBLE:
    oss.precision(6);
    oss << *(double *) value;
    return_value= oss.str();
    break;
  case SHOW_LONG_STATUS:
    value= ((char *) status_var + (ulong) value);
    /* fall through */
  case SHOW_LONG:
    oss << *(long*) value;
    return_value= oss.str();
    break;
  case SHOW_LONGLONG_STATUS:
    value= ((char *) status_var + (uint64_t) value);
    /* fall through */
  case SHOW_LONGLONG:
    oss << *(int64_t*) value;
    return_value= oss.str();
    break;
  case SHOW_SIZE:
    oss << *(size_t*) value;
    return_value= oss.str();
    break;
  case SHOW_HA_ROWS:
    oss << (int64_t) *(ha_rows*) value;
    return_value= oss.str();
    break;
  case SHOW_BOOL:
  case SHOW_MY_BOOL:
    return_value= *(bool*) value ? "ON" : "OFF";
    break;
  case SHOW_INT:
  case SHOW_INT_NOFLUSH: // the difference lies in refresh_status()
    oss << (long) *(uint32_t*) value;
    return_value= oss.str();
    break;
  case SHOW_CHAR:
    {
      if (value)
        return_value= value;
      break;
    }
  case SHOW_CHAR_PTR:
    {
      if (*(char**) value)
        return_value= *(char**) value;

      break;
    }
  case SHOW_UNDEF:
    break;                                        // Return empty string
  case SHOW_SYS:                                  // Cannot happen
  default:
    assert(0);
    break;
  }

  push(name);
  if (return_value.length())
  {
    push(return_value);
  }
  else
  {
    push(" ");
  }
}
