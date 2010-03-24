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

#include <config.h>          
#include "stats_schema.h"

using namespace drizzled;
using namespace plugin;
using namespace std;

CurrentCommandsTool::CurrentCommandsTool(LoggingStats *in_logging_stats) :
  plugin::TableFunction("DATA_DICTIONARY", "CURRENT_SQL_COMMANDS")
{
  logging_stats= in_logging_stats;

  add_field("USER");
  add_field("IP");
  add_field("COUNT_SELECT", TableFunction::NUMBER);
  add_field("COUNT_DELETE", TableFunction::NUMBER);
  add_field("COUNT_UPDATE", TableFunction::NUMBER);
  add_field("COUNT_INSERT", TableFunction::NUMBER);
  add_field("COUNT_ROLLBACK", TableFunction::NUMBER);
  add_field("COUNT_COMMIT", TableFunction::NUMBER);
  add_field("COUNT_CREATE", TableFunction::NUMBER);
  add_field("COUNT_ALTER", TableFunction::NUMBER);
  add_field("COUNT_DROP", TableFunction::NUMBER);
  add_field("COUNT_ADMIN", TableFunction::NUMBER);
}

CurrentCommandsTool::Generator::Generator(Field **arg, LoggingStats *logging_stats) :
  plugin::TableFunction::Generator(arg)
{
  current_scoreboard= logging_stats->getCurrentScoreboard();
  current_bucket= 0;
  number_buckets= current_scoreboard->getNumberBuckets();
  setVectorIteratorsAndLock(current_bucket);
}

CurrentCommandsTool::Generator::~Generator()
{
}

void CurrentCommandsTool::Generator::setVectorIteratorsAndLock(uint32_t bucket_number)
{
  vector<ScoreboardSlot* > *scoreboard_vector= 
    current_scoreboard->getVectorOfScoreboardVectors()->at(bucket_number); 

  current_lock= current_scoreboard->getVectorOfScoreboardLocks()->at(bucket_number);

  it= scoreboard_vector->begin();
  end= scoreboard_vector->end();
}

bool CurrentCommandsTool::Generator::populate()
{
  if ((it == end) && (current_bucket == (number_buckets - 1 ))) 
  {
    return false;
  }

  if (it == end)
  {
    current_bucket++;
    setVectorIteratorsAndLock(current_bucket);
  }

  pthread_rwlock_rdlock(current_lock);
  ScoreboardSlot *scoreboard_slot= *it; 

  if (scoreboard_slot->isInUse())
  {
    UserCommands *user_commands= scoreboard_slot->getUserCommands();
    push(scoreboard_slot->getUser());
    push(scoreboard_slot->getIp());
    push(user_commands->getSelectCount());
    push(user_commands->getDeleteCount());
    push(user_commands->getUpdateCount());
    push(user_commands->getInsertCount());
    push(user_commands->getRollbackCount());
    push(user_commands->getCommitCount());
    push(user_commands->getCreateCount());
    push(user_commands->getAlterCount());
    push(user_commands->getDropCount());
    push(user_commands->getAdminCount());
  }
  pthread_rwlock_unlock(current_lock);

  it++;
  return true;
}

CumulativeCommandsTool::CumulativeCommandsTool(LoggingStats *logging_stats) :
  plugin::TableFunction("DATA_DICTIONARY", "CUMULATIVE_SQL_COMMANDS")
{
  outer_logging_stats= logging_stats;

  add_field("USER");
  add_field("COUNT_SELECT", TableFunction::NUMBER);
  add_field("COUNT_DELETE", TableFunction::NUMBER);
  add_field("COUNT_UPDATE", TableFunction::NUMBER);
  add_field("COUNT_INSERT", TableFunction::NUMBER);
  add_field("COUNT_ROLLBACK", TableFunction::NUMBER);
  add_field("COUNT_COMMIT", TableFunction::NUMBER);
  add_field("COUNT_CREATE", TableFunction::NUMBER);
  add_field("COUNT_ALTER", TableFunction::NUMBER);
  add_field("COUNT_DROP", TableFunction::NUMBER);
  add_field("COUNT_ADMIN", TableFunction::NUMBER);
}

CumulativeCommandsTool::Generator::Generator(Field **arg, LoggingStats *in_logging_stats) :
  plugin::TableFunction::Generator(arg)
{
  logging_stats= in_logging_stats;
  record_number= 0;

  if (logging_stats->isEnabled())
  {
    total_records= logging_stats->getCumulativeStatsByUserIndex();
  }
  else
  {
    total_records= 0; 
  }
}

bool CumulativeCommandsTool::Generator::populate()
{
  if (record_number == total_records)
  {
    return false;
  }

  ScoreboardSlot *cumulative_scoreboard_slot= 
    logging_stats->getCumulativeStatsByUserVector()->at(record_number);

  push(cumulative_scoreboard_slot->getUser());
  push(cumulative_scoreboard_slot->getUserCommands()->getSelectCount());
  push(cumulative_scoreboard_slot->getUserCommands()->getDeleteCount());
  push(cumulative_scoreboard_slot->getUserCommands()->getUpdateCount());
  push(cumulative_scoreboard_slot->getUserCommands()->getInsertCount());
  push(cumulative_scoreboard_slot->getUserCommands()->getRollbackCount());
  push(cumulative_scoreboard_slot->getUserCommands()->getCommitCount());
  push(cumulative_scoreboard_slot->getUserCommands()->getCreateCount());
  push(cumulative_scoreboard_slot->getUserCommands()->getAlterCount());
  push(cumulative_scoreboard_slot->getUserCommands()->getDropCount());
  push(cumulative_scoreboard_slot->getUserCommands()->getAdminCount());

  record_number++;
  return true;
}

