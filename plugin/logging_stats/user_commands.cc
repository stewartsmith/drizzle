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

#include "user_commands.h"

using namespace drizzled;
using namespace std;

const char* UserCommands::USER_COUNTS[] =
{
  "COUNT_SELECT",
  "COUNT_DELETE",
  "COUNT_UPDATE",
  "COUNT_INSERT",
  "COUNT_ROLLBACK",
  "COUNT_COMMIT",
  "COUNT_CREATE",
  "COUNT_ALTER",
  "COUNT_DROP",
  "COUNT_ADMIN"
};

const char* UserCommands::COM_STATUS_VARS[] =
{
  "select",
  "create_table",
  "create_index",
  "alter_table",
  "update",
  "insert",
  "insert_select",
  "delete",
  "truncate",
  "drop_table",
  "drop_index",
  "show_create",
  "show_create_db",
  "load",
  "set_option",
  "unlock_tables",
  "change_db",
  "create_db",
  "drop_db",
  "alter_db",
  "replace",
  "replace_select",
  "check",
  "flush",
  "kill",
  "analyze",
  "rollback",
  "rollback_to_savepoint",
  "commit",
  "savepoint",
  "release_savepoint",
  "begin",
  "rename_table",
  "show_warns",
  "empty_query",
  "show_errors",
  "checksum"
};

UserCommands::UserCommands()
{
  init();
}

void UserCommands::init()
{
  vector<uint64_t>::iterator it= vector_of_command_counts.begin();
  for (int j=0; j < SQLCOM_END; ++j)
  {
    it=  vector_of_command_counts.insert(it, 0);
  }
  vector_of_command_counts.resize(SQLCOM_END);
}

void UserCommands::incrementCount(uint32_t index, uint32_t i)
{
  uint64_t *count= &(vector_of_command_counts.at(index));
  *count= *count + i;
}

uint64_t UserCommands::getCount(uint32_t index)
{
  uint64_t *count= &(vector_of_command_counts.at(index));
  return *count;
}

uint64_t UserCommands::getUserCount(uint32_t index)
{
  switch (index)
  {
    case COUNT_SELECT:
      return getCount(SQLCOM_SELECT);
    case COUNT_DELETE:
      return getCount(SQLCOM_DELETE);
    case COUNT_UPDATE:
      return getCount(SQLCOM_UPDATE);
    case COUNT_INSERT:
      return getCount(SQLCOM_INSERT);
    case COUNT_ROLLBACK:
      return getCount(SQLCOM_ROLLBACK);
    case COUNT_COMMIT:
      return getCount(SQLCOM_COMMIT);
    case COUNT_CREATE:
      return getCount(SQLCOM_CREATE_TABLE);
    case COUNT_ALTER:
      return getCount(SQLCOM_ALTER_TABLE);
    case COUNT_DROP:
      return getCount(SQLCOM_DROP_TABLE);
    default:
      return 0;
  }
}

void UserCommands::reset()
{
  for (uint32_t j= 0; j < SQLCOM_END; ++j)
  {
    uint64_t *count= &(vector_of_command_counts.at(j));
    *count= 0;
  }
}

UserCommands::UserCommands(const UserCommands &user_commands)
{
  init();

  for (uint32_t j= 0; j < SQLCOM_END; ++j)
  {
    uint64_t *my_count= &(vector_of_command_counts.at(j));
    uint64_t other_count= user_commands.vector_of_command_counts.at(j);
    *my_count= other_count;
  }
}

void UserCommands::merge(UserCommands *user_commands)
{
  for (uint32_t j= 0; j < SQLCOM_END; ++j)
  {
    uint64_t *my_count= &(vector_of_command_counts.at(j));
    uint64_t other_count= user_commands->vector_of_command_counts.at(j);
    *my_count= *my_count + other_count;
  }
}

void UserCommands::logCommand(enum_sql_command sql_command)
{
  if (sql_command < SQLCOM_END)
  {
    incrementCount(sql_command);
  }
}
