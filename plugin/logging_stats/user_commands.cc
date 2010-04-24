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

#include "user_commands.h"

using namespace drizzled;


const char* UserCommands::IDENTIFIERS[] = 
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

UserCommands::UserCommands()
{
  init();
}

void UserCommands::init()
{
  select_count= 0;
  delete_count= 0;
  update_count= 0;
  insert_count= 0;
  rollback_count= 0;
  commit_count= 0;
  create_count= 0;
  alter_count= 0;
  drop_count= 0;
  admin_count= 0;
  vector_of_command_counts.push_back(&select_count);
  vector_of_command_counts.push_back(&delete_count);
  vector_of_command_counts.push_back(&update_count);
  vector_of_command_counts.push_back(&insert_count);
  vector_of_command_counts.push_back(&rollback_count);
  vector_of_command_counts.push_back(&commit_count);
  vector_of_command_counts.push_back(&create_count);
  vector_of_command_counts.push_back(&alter_count);
  vector_of_command_counts.push_back(&drop_count);
  vector_of_command_counts.push_back(&admin_count);
  size= vector_of_command_counts.size();
}

void UserCommands::incrementCount(uint32_t index, uint32_t i)
{
  uint64_t *count= vector_of_command_counts.at(index);
  *count= *count + i;
}

uint64_t UserCommands::getCount(uint32_t index)
{
  uint64_t *count= vector_of_command_counts.at(index);
  return *count;
}

void UserCommands::reset()
{
  for (uint32_t j= 0; j < size; ++j)
  {
    uint64_t *count= vector_of_command_counts.at(j);
    *count= 0;
  }
}

UserCommands::UserCommands(const UserCommands &user_commands)
{
  init();  

  for (uint32_t j= 0; j < size; ++j)
  {
    uint64_t *my_count= vector_of_command_counts.at(j);
    uint64_t *other_count= user_commands.vector_of_command_counts.at(j);
    *my_count= *other_count;
  }
}

void UserCommands::merge(UserCommands *user_commands)
{
  for (uint32_t j= 0; j < size; ++j)
  {
    uint64_t *my_count= vector_of_command_counts.at(j);
    uint64_t *other_count= user_commands->vector_of_command_counts.at(j);
    *my_count= *my_count + *other_count;
  }
}

void UserCommands::logCommand(enum_sql_command sql_command)
{
  switch(sql_command)
  {
    case SQLCOM_UPDATE:
      incrementCount(COUNT_UPDATE);
      break;
    case SQLCOM_DELETE:
      incrementCount(COUNT_DELETE);
      break;
    case SQLCOM_INSERT:
      incrementCount(COUNT_INSERT);
      break;
    case SQLCOM_ROLLBACK:
      incrementCount(COUNT_ROLLBACK);
      break;
    case SQLCOM_COMMIT:
      incrementCount(COUNT_COMMIT);
      break;
    case SQLCOM_CREATE_TABLE:
      incrementCount(COUNT_CREATE);
      break;
    case SQLCOM_ALTER_TABLE:
      incrementCount(COUNT_ALTER);
      break;
    case SQLCOM_DROP_TABLE:
      incrementCount(COUNT_DROP);
      break;
    case SQLCOM_SELECT:
      incrementCount(COUNT_SELECT);
      break;
    default:
      return;
  }
}
