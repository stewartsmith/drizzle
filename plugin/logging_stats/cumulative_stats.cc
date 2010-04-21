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
#include "cumulative_stats.h"

using namespace std;

CumulativeStats::CumulativeStats(uint32_t in_cumulative_stats_by_user_max) 
    :
      cumulative_stats_by_user_max(in_cumulative_stats_by_user_max)  
{
  cumulative_stats_by_user_vector= new vector<ScoreboardSlot *>(cumulative_stats_by_user_max);

  vector<ScoreboardSlot *>::iterator it= cumulative_stats_by_user_vector->begin();
  for (uint32_t j=0; j < cumulative_stats_by_user_max; ++j)
  {
    ScoreboardSlot *scoreboard_slot= new ScoreboardSlot();
    it= cumulative_stats_by_user_vector->insert(it, scoreboard_slot);
  }
  cumulative_stats_by_user_vector->resize(cumulative_stats_by_user_max);

  global_stats= new GlobalStats();
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
}

void CumulativeStats::logUserStats(ScoreboardSlot *scoreboard_slot)
{
  vector<ScoreboardSlot *>::iterator cumulative_it= cumulative_stats_by_user_vector->begin();
  bool found= false;

  /* Search if this is a pre-existing user */

  uint32_t read_index= user_index_reading;

  for (uint32_t j=0; j < read_index; ++j)
  {
    ScoreboardSlot *cumulative_scoreboard_slot= *cumulative_it;
    string user= cumulative_scoreboard_slot->getUser();
    if (user.compare(scoreboard_slot->getUser()) == 0)
    {
      found= true;
      cumulative_scoreboard_slot->merge(scoreboard_slot);
      break;
    }
    ++cumulative_it;
  }

  if (! found)
  {
    /* the user was not found */
    /* its possible multiple simultaneous connections with the same user
       could result in duplicate entries here, not likely and it would
       be harmless except for duplicate users showing up in a query */
    
    if (hasOpenUserSlots())
    { 
      uint32_t our_index= user_index_writing.fetch_and_increment();
      if (our_index < cumulative_stats_by_user_max)
      {
        ScoreboardSlot *cumulative_scoreboard_slot=
          cumulative_stats_by_user_vector->at(our_index);
        cumulative_scoreboard_slot->setUser(scoreboard_slot->getUser());
        cumulative_scoreboard_slot->merge(scoreboard_slot);
        user_index_reading.increment();
      } 
      else 
      {
        isOpenUserSlots= false;
      }
    } 
  }
}

void CumulativeStats::logGlobalStats(ScoreboardSlot* scoreboard_slot)
{
  global_stats->updateUserCommands(scoreboard_slot); 
}
