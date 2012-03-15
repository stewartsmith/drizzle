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
#include "cumulative_stats.h"
#include "scoreboard.h"

#include <drizzled/atomics.h>
#include <drizzled/enum.h>
#include <drizzled/plugin/logging.h>

#include <string>
#include <vector>

class LoggingStats: public drizzled::plugin::Logging
{
public:

  LoggingStats(std::string name_arg);

  ~LoggingStats();

  virtual bool post(drizzled::Session *session);

  virtual bool postEnd(drizzled::Session *session);

  virtual bool resetGlobalScoreboard();

  bool isEnabled() const
  {
    return is_enabled;
  }

  void enable()
  {
    is_enabled= true;
  }

  void disable()
  {
    is_enabled= false;
  }

  Scoreboard *getCurrentScoreboard()
  {          
    return current_scoreboard;
  }

  CumulativeStats *getCumulativeStats()
  {
    return cumulative_stats;
  }

private:
  Scoreboard *current_scoreboard;

  CumulativeStats *cumulative_stats;

  drizzled::atomic<bool> is_enabled;

  void updateCurrentScoreboard(ScoreboardSlot *scoreboard_slot, drizzled::Session *session);

  typedef std::vector<std::vector<ScoreboardSlot* >* > ScoreBoardVectors;
};
