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

UserCommands::UserCommands()
    :
      update_count(0),
      delete_count(0),
      insert_count(0),
      select_count(0),
      rollback_count(0),
      commit_count(0),
      create_count(0),
      alter_count(0),
      drop_count(0),
      admin_count(0)
{}

void UserCommands::reset()
{
  update_count= 0;
  delete_count= 0;
  insert_count= 0;
  select_count= 0;
  rollback_count= 0;
  commit_count= 0;
  create_count= 0;
  alter_count= 0;
  drop_count= 0;
  admin_count= 0;
}

UserCommands::UserCommands(const UserCommands &user_commands)
{
  update_count= user_commands.update_count;
  delete_count= user_commands.delete_count;
  insert_count= user_commands.insert_count;
  select_count= user_commands.select_count;
  rollback_count= user_commands.rollback_count;
  commit_count= user_commands.commit_count;
  create_count= user_commands.create_count;
  alter_count= user_commands.alter_count;
  drop_count= user_commands.drop_count;
  admin_count= user_commands.admin_count;
}

void UserCommands::merge(UserCommands *user_commands)
{
  incrementUpdateCount(user_commands->getUpdateCount());
  incrementDeleteCount(user_commands->getDeleteCount());
  incrementInsertCount(user_commands->getInsertCount());
  incrementSelectCount(user_commands->getSelectCount());
  incrementRollbackCount(user_commands->getRollbackCount());
  incrementCommitCount(user_commands->getCommitCount());
  incrementCreateCount(user_commands->getCreateCount());
  incrementAlterCount(user_commands->getAlterCount());
  incrementDropCount(user_commands->getDropCount());
  incrementAdminCount(user_commands->getAdminCount());
}

uint64_t UserCommands::getSelectCount()
{
  return select_count;
}

void UserCommands::incrementSelectCount(int i)
{
  select_count= select_count + i;
}

uint64_t UserCommands::getUpdateCount()
{
  return update_count;
}

void UserCommands::incrementUpdateCount(int i)
{
  update_count= update_count + i;
}

uint64_t UserCommands::getDeleteCount()
{
  return delete_count;
}

void UserCommands::incrementDeleteCount(int i)
{
  delete_count= delete_count + i;
}

uint64_t UserCommands::getInsertCount()
{
  return insert_count;
}

void UserCommands::incrementInsertCount(int i)
{
  insert_count= insert_count + i;
}

uint64_t UserCommands::getRollbackCount()
{
  return rollback_count;
}

void UserCommands::incrementRollbackCount(int i)
{
  rollback_count= rollback_count + i;
}

uint64_t UserCommands::getCommitCount()
{
  return commit_count;
}

void UserCommands::incrementCommitCount(int i)
{
  commit_count= commit_count + i;
}

uint64_t UserCommands::getCreateCount()
{
  return create_count;
}

void UserCommands::incrementCreateCount(int i)
{
  create_count= create_count + i;
}

uint64_t UserCommands::getAlterCount()
{
  return alter_count;
}

void UserCommands::incrementAlterCount(int i)
{
  alter_count= alter_count + i;
}

uint64_t UserCommands::getDropCount()
{
  return drop_count;
}

void UserCommands::incrementDropCount(int i)
{
  drop_count= drop_count + i;
}

uint64_t UserCommands::getAdminCount()
{
  return admin_count;
}

void UserCommands::incrementAdminCount(int i)
{
  admin_count= admin_count + i;
}
