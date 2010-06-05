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

#include "config.h"

#include "status_tool.h"

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
  vector<drizzle_show_var *>::iterator all_status_vars_iterator;          
  while (true)
  {
    var= &StatusVars::status_vars_defs[count];
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
                                 vector<drizzle_show_var *> *all_status_vars, 
                                 bool inIsLocal) :
  TableFunction::Generator(arg),
  logging_stats(in_logging_stats),
  isLocal(inIsLocal)   
{
  all_status_vars_it= all_status_vars->begin();
  all_status_vars_end= all_status_vars->end();

  init();

/*
  status_var_to_display= NULL;
  if (isLocal)
  {
    Session *this_session= current_session;
    ScoreboardSlot *scoreboard_slot= logging_stats->getCurrentScoreboard()->findOurScoreboardSlot(this_session);

    if (scoreboard_slot != NULL)
    {
      status_var_to_display= scoreboard_slot->getStatusVars();
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
    cumulativeStats->sumCurrentScoreboardStatusVars(logging_stats->getCurrentScoreboard(), status_var_to_display);
    status_var_to_display->merge(logging_stats->getCumulativeStats()->getGlobalStatusVars());
  }
*/
}

void StatusTool::Generator::init()
{
  status_var_to_display= NULL;
  if (isLocal)
  {
    Session *this_session= current_session;
    ScoreboardSlot *scoreboard_slot= logging_stats->getCurrentScoreboard()->findOurScoreboardSlot(this_session);

    if (scoreboard_slot != NULL)
    {
      status_var_to_display= scoreboard_slot->getStatusVars();
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
    cumulativeStats->sumCurrentScoreboardStatusVars(logging_stats->getCurrentScoreboard(), status_var_to_display);
    status_var_to_display->merge(logging_stats->getCumulativeStats()->getGlobalStatusVars());
  }
}

StatusTool::Generator::~Generator()
{
  if (! isLocal) 
  {
    delete status_var_to_display;
  }
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
        ((mysql_show_var_func)((st_show_var_func_container *)var->value)->func)(&tmp, buff);
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

extern drizzled::KEY_CACHE dflt_key_cache_var, *dflt_key_cache;

void StatusTool::Generator::fill(const string &name, char *value, SHOW_TYPE show_type)
{
  struct system_status_var *status_var;
  ostringstream oss;
  string return_value;

  status_var= status_var_to_display->getStatusVarCounters();

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
  case SHOW_KEY_CACHE_LONG:
    value= (char*) dflt_key_cache + (unsigned long)value;
    oss << *(long*) value;
    return_value= oss.str();
    break;
  case SHOW_KEY_CACHE_LONGLONG:
    value= (char*) dflt_key_cache + (unsigned long)value;
    oss << *(int64_t*) value;
    return_value= oss.str();
    break;
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
