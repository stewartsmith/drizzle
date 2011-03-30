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

#include <config.h>
#include <drizzled/statistics_variables.h>
#include "cumulative_stats.h"

using namespace std;
using namespace drizzled;

CumulativeStats::CumulativeStats(uint32_t in_cumulative_stats_by_user_max) 
    :
      cumulative_stats_by_user_max(in_cumulative_stats_by_user_max)  
{
  cumulative_stats_by_user_vector= new vector<ScoreboardSlot *>(cumulative_stats_by_user_max);

  vector<ScoreboardSlot *>::iterator it= cumulative_stats_by_user_vector->begin();
  for (int32_t j=0; j < cumulative_stats_by_user_max; ++j)
  {
    ScoreboardSlot *scoreboard_slot= new ScoreboardSlot();
    it= cumulative_stats_by_user_vector->insert(it, scoreboard_slot);
  }
  cumulative_stats_by_user_vector->resize(cumulative_stats_by_user_max);

  last_valid_index= INVALID_INDEX;
  isOpenUserSlots= true;
  global_stats= new GlobalStats();
  global_status_vars= new StatusVars();

  /* calculate the approximate memory allocation for the cumulative statistics */
  size_t statusVarsSize= sizeof(StatusVars) + sizeof(system_status_var);
  size_t userCommandsSize= sizeof(UserCommands) + sizeof(uint64_t) * SQLCOM_END;

  cumulative_size_bytes= (statusVarsSize + userCommandsSize) * cumulative_stats_by_user_max; 
}

CumulativeStats::~CumulativeStats()
{
  vector<ScoreboardSlot *>::iterator it= cumulative_stats_by_user_vector->begin();
  for (; it < cumulative_stats_by_user_vector->end(); ++it)
  {
    delete *it;
  }
  cumulative_stats_by_user_vector->clear();
  delete cumulative_stats_by_user_vector;
  delete global_stats;
  delete global_status_vars;
}

void CumulativeStats::logUserStats(ScoreboardSlot *scoreboard_slot, bool reserveSlot)
{
  vector<ScoreboardSlot *>::iterator cumulative_it= cumulative_stats_by_user_vector->begin();

  /* Search if this is a pre-existing user */

  int32_t current_index= last_valid_index;

  if (cumulative_stats_by_user_max <= current_index)
  {
    current_index= cumulative_stats_by_user_max;
  }

  for (int32_t j=0; j <= current_index; ++j)
  {
    ScoreboardSlot *cumulative_scoreboard_slot= *cumulative_it;
    string user= cumulative_scoreboard_slot->getUser();
    if (user.compare(scoreboard_slot->getUser()) == 0)
    {
      reserveSlot= false;
      cumulative_scoreboard_slot->merge(scoreboard_slot);
      break;
    }
    ++cumulative_it;
  }
  
  if (reserveSlot)
  {
    /* the user was not found */
    /* its possible multiple simultaneous connections with the same user
       could result in duplicate entries here, not likely and it would
       be harmless except for duplicate users showing up in a query */
    
    if (hasOpenUserSlots())
    { 
      int32_t our_index= last_valid_index.add_and_fetch(1);
      if (our_index < cumulative_stats_by_user_max)
      {
        ScoreboardSlot *cumulative_scoreboard_slot=
          cumulative_stats_by_user_vector->at(our_index);
        cumulative_scoreboard_slot->setUser(scoreboard_slot->getUser());
        cumulative_scoreboard_slot->merge(scoreboard_slot);
        cumulative_scoreboard_slot->setInUse(true);
      } 
      else 
      {
        last_valid_index= cumulative_stats_by_user_max - 1; 
        isOpenUserSlots= false;
      }
    } 
  }
}

void CumulativeStats::logGlobalStats(ScoreboardSlot* scoreboard_slot)
{
  global_stats->updateUserCommands(scoreboard_slot); 
}

void CumulativeStats::logGlobalStatusVars(ScoreboardSlot* scoreboard_slot)
{
  global_status_vars->merge(scoreboard_slot->getStatusVars());
}

int32_t CumulativeStats::getCumulativeStatsLastValidIndex()
{
  if (last_valid_index < cumulative_stats_by_user_max)
  {
    return last_valid_index;
  } 
  else 
  {
    return cumulative_stats_by_user_max;
  }
}

void CumulativeStats::sumCurrentScoreboard(Scoreboard *scoreboard,
                                           StatusVars *current_status_vars,
                                           UserCommands *current_user_commands)
{
  /* the vector of vectors */
  vector<vector<ScoreboardSlot* >* > *vector_of_scoreboard_vectors= 
    scoreboard->getVectorOfScoreboardVectors();

  /* iterate through each vector from above and sum each ScoreboardSlot */

  vector<vector<ScoreboardSlot* >* >::iterator v_of_scoreboard_v_begin_it= vector_of_scoreboard_vectors->begin(); 

  vector<vector<ScoreboardSlot* >* >::iterator v_of_scoreboard_v_end_it= vector_of_scoreboard_vectors->end(); 

  for (; v_of_scoreboard_v_begin_it != v_of_scoreboard_v_end_it; ++v_of_scoreboard_v_begin_it)
  {
    vector<ScoreboardSlot* > *scoreboard_vector= *v_of_scoreboard_v_begin_it;
    
    vector<ScoreboardSlot* >::iterator scoreboard_vector_it= scoreboard_vector->begin();
    vector<ScoreboardSlot* >::iterator scoreboard_vector_end= scoreboard_vector->end();
    for (; scoreboard_vector_it != scoreboard_vector_end; ++scoreboard_vector_it)
    {
      ScoreboardSlot *scoreboard_slot= *scoreboard_vector_it;
      if (scoreboard_slot->isInUse())
      {
        if (current_status_vars)
        {
          current_status_vars->merge(scoreboard_slot->getStatusVars());
        }

        if (current_user_commands)
        {
          current_user_commands->merge(scoreboard_slot->getUserCommands());
        }
      }
    }
  }
}
