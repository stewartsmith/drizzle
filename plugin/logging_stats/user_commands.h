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

#ifndef PLUGIN_LOGGING_STATS_USER_COMMANDS_H
#define PLUGIN_LOGGING_STATS_USER_COMMANDS_H

#include <drizzled/common.h>
#include <drizzled/enum.h>
#include <string>
#include <vector>


class UserCommands
{
public:

  UserCommands();

  UserCommands(const UserCommands &user_commands);

  enum command_count_index {
    COUNT_SELECT,
    COUNT_DELETE,
    COUNT_UPDATE,
    COUNT_INSERT,
    COUNT_ROLLBACK,
    COUNT_COMMIT,
    COUNT_CREATE,
    COUNT_ALTER,
    COUNT_DROP,
    COUNT_ADMIN
  }; 

  void incrementCount(uint32_t index, uint32_t i= 1);

  uint64_t getCount(uint32_t index);

  void merge(UserCommands *user_commands);

  void reset();

  void logCommand(drizzled::enum_sql_command sql_command);

  void init();

private:
  std::vector<uint64_t*> vector_of_command_counts;
  
  uint32_t size;
  uint64_t select_count;
  uint64_t delete_count;
  uint64_t update_count;
  uint64_t insert_count;
  uint64_t rollback_count;
  uint64_t commit_count;
  uint64_t create_count;
  uint64_t alter_count;
  uint64_t drop_count;
  uint64_t admin_count;
};

#endif /* PLUGIN_LOGGING_STATS_USER_COMMANDS_H */
