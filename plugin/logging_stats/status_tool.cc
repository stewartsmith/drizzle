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
 * This class defines the "show status" and "show global status" 
 * DATA_DICTIONARY tables:
 * 
 * drizzle> describe SESSION_STATUS;
 * +----------------+---------+-------+---------+-----------------+-----------+
 * | Field          | Type    | Null  | Default | Default_is_NULL | On_Update |
 * +----------------+---------+-------+---------+-----------------+-----------+
 * | VARIABLE_NAME  | VARCHAR | FALSE |         | FALSE           |           | 
 * | VARIABLE_VALUE | VARCHAR | FALSE |         | FALSE           |           | 
 * +----------------+---------+-------+---------+-----------------+-----------+
 *
 * drizzle> describe GLOBAL_STATUS;
 * +----------------+---------+-------+---------+-----------------+-----------+
 * | Field          | Type    | Null  | Default | Default_is_NULL | On_Update |
 * +----------------+---------+-------+---------+-----------------+-----------+
 * | VARIABLE_NAME  | VARCHAR | FALSE |         | FALSE           |           | 
 * | VARIABLE_VALUE | VARCHAR | FALSE |         | FALSE           |           | 
 * +----------------+---------+-------+---------+-----------------+-----------+
 *
 */

#include <config.h>

#include "status_tool.h"
#include <drizzled/status_helper.h>

#include <vector>
#include <sstream>

using namespace drizzled;
using namespace plugin;
using namespace std;


class ShowVarCmpFunctor 
{
  public:
  ShowVarCmpFunctor() { }
  inline bool operator()(const drizzle_show_var *var1, const drizzle_show_var *var2) const
  {
    int val= strcmp(var1->name, var2->name);
    return (val < 0);
  }
};

StatusTool::StatusTool(LoggingStats *in_logging_stats, bool inIsLocal) :
  TableFunction("DATA_DICTIONARY", inIsLocal ? "SESSION_STATUS" : "GLOBAL_STATUS"),
  outer_logging_stats(in_logging_stats),
  isLocal(inIsLocal)
{
  add_field("VARIABLE_NAME");
  add_field("VARIABLE_VALUE", 1024);

  drizzle_show_var *var= NULL;
  uint32_t count= 0;
  std::vector<drizzle_show_var *>::iterator all_status_vars_iterator= all_status_vars.begin();
  while (true)
  {
    var= &StatusHelper::status_vars_defs[count];
    if ((var == NULL) || (var->name == NULL))
    {
      break;
    }
    all_status_vars_iterator= all_status_vars.insert(all_status_vars_iterator, var);
    ++count;
  }
  sort(all_status_vars.begin(), all_status_vars.end(), ShowVarCmpFunctor());
}

StatusTool::Generator::Generator(Field **arg, LoggingStats *in_logging_stats,
                                 std::vector<drizzle_show_var *> *in_all_status_vars, 
                                 bool inIsLocal) :
  TableFunction::Generator(arg),
  logging_stats(in_logging_stats),
  isLocal(inIsLocal)   
{
  all_status_vars_it= in_all_status_vars->begin();
  all_status_vars_end= in_all_status_vars->end();

  status_var_to_display= NULL;
  if (isLocal)
  {
    ScoreboardSlot *scoreboard_slot= logging_stats->getCurrentScoreboard()->findOurScoreboardSlot(&getSession());

    if (scoreboard_slot != NULL)
    {
      /* A copy of the current status vars for a particular session */ 
      status_var_to_display= new StatusVars(*scoreboard_slot->getStatusVars());

      /* Sum the current scoreboard this will give us a value for any variables that have global meaning */
      StatusVars current_scoreboard_status_vars;
      CumulativeStats *cumulativeStats= logging_stats->getCumulativeStats();
      cumulativeStats->sumCurrentScoreboard(logging_stats->getCurrentScoreboard(), &current_scoreboard_status_vars, NULL);

      /* Copy the above to get a value for any variables that have global meaning */  
      status_var_to_display->copyGlobalVariables(logging_stats->getCumulativeStats()->getGlobalStatusVars());
      status_var_to_display->copyGlobalVariables(&current_scoreboard_status_vars); 
    } 
    else 
    {
      status_var_to_display= NULL;
    }
  }
  else // global status 
  {
    status_var_to_display= new StatusVars();
    CumulativeStats *cumulativeStats= logging_stats->getCumulativeStats();
    cumulativeStats->sumCurrentScoreboard(logging_stats->getCurrentScoreboard(), status_var_to_display, NULL);
    status_var_to_display->merge(logging_stats->getCumulativeStats()->getGlobalStatusVars());
  }
}

StatusTool::Generator::~Generator()
{
  delete status_var_to_display;
}

bool StatusTool::Generator::populate()
{
  if (status_var_to_display == NULL)
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
        ((drizzle_show_var_func)((st_show_var_func_container *)var->value)->func)(&tmp, buff);
    }

    if (isWild(variables->name))
    {
      ++all_status_vars_it;
      continue;
    }

    fill(variables->name, var->value, var->type);

    ++all_status_vars_it;

    return true;
  }

  return false;
}

void StatusTool::Generator::fill(const string &name, char *value, SHOW_TYPE show_type)
{
  struct system_status_var *status_var;
  ostringstream oss;
  string return_value;

  status_var= status_var_to_display->getStatusVarCounters();

  return_value= StatusHelper::fillHelper(status_var, value, show_type);

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
