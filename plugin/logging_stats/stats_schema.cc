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

CommandsTool::CommandsTool(LoggingStats *logging_stats) :
  plugin::TableFunction("DATA_DICTIONARY", "SQL_COMMANDS_BY_USER")
{
  outer_logging_stats= logging_stats;  

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

CommandsTool::Generator::Generator(Field **arg, LoggingStats *in_logging_stats) :
  plugin::TableFunction::Generator(arg)
{
  pthread_rwlock_rdlock(&LOCK_scoreboard);
  logging_stats= in_logging_stats;

  if (logging_stats->isEnabled())
  {
    record_number= 0;
  } 
  else 
  {
    record_number= logging_stats->getScoreBoardSize(); 
  }
}

CommandsTool::Generator::~Generator()
{
  pthread_rwlock_unlock(&LOCK_scoreboard);
}

bool CommandsTool::Generator::populate()
{
  if (record_number == logging_stats->getScoreBoardSize())
  {
    return false;
  }
  
  ScoreBoardSlot *score_board_slots= logging_stats->getScoreBoardSlots();

  while (record_number < logging_stats->getScoreBoardSize())
  {
    ScoreBoardSlot *score_board_slot= &score_board_slots[record_number];
    if (score_board_slot->isInUse())
    {
      UserCommands *user_commands= score_board_slot->getUserCommands();
      push(score_board_slot->getUser());
      push(score_board_slot->getIp());
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
      record_number++;
      return true;
    }
    else 
    {
      record_number++;
    }
  }

  return false;
}
