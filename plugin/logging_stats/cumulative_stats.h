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

#pragma once

#include "scoreboard_slot.h"
#include "scoreboard.h"
#include "global_stats.h"

#include <drizzled/atomics.h>

#include <vector>

static const int32_t INVALID_INDEX= -1;

class CumulativeStats
{
public:
  CumulativeStats(uint32_t in_cumulative_stats_by_user_max);

  ~CumulativeStats();

  void logUserStats(ScoreboardSlot* scoreboard_slot, bool reserveSlot);

  void logGlobalStats(ScoreboardSlot* scoreboard_slot);

  void logGlobalStatusVars(ScoreboardSlot* scoreboard_slot);

  std::vector<ScoreboardSlot* > *getCumulativeStatsByUserVector()
  {
    return cumulative_stats_by_user_vector;
  }

  GlobalStats *getGlobalStats()
  {
    return global_stats;
  }

  StatusVars *getGlobalStatusVars()
  {
    return global_status_vars;
  }

  int32_t getCumulativeStatsByUserMax()
  {
    return cumulative_stats_by_user_max; 
  }

  uint64_t getCumulativeSizeBytes()
  {
    return cumulative_size_bytes;
  }

  int32_t getCumulativeStatsLastValidIndex();

  bool hasOpenUserSlots()
  {
    return isOpenUserSlots;
  }

  void sumCurrentScoreboard(Scoreboard *scoreboard, 
                            StatusVars *current_status_vars,
                            UserCommands *current_user_commands);

private:
  std::vector<ScoreboardSlot* > *cumulative_stats_by_user_vector;
  GlobalStats *global_stats; 
  StatusVars *global_status_vars;
  uint64_t cumulative_size_bytes;
  int32_t cumulative_stats_by_user_max;
  drizzled::atomic<int32_t> last_valid_index;
  bool isOpenUserSlots;
};

