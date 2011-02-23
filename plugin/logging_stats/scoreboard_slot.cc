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
 *
 */

#include <config.h>
#include <drizzled/plugin.h>
#include <drizzled/statistics_variables.h>
#include <drizzled/session.h>

#include "scoreboard_slot.h"

using namespace std;

ScoreboardSlot::ScoreboardSlot()
  :
    in_use(false),
    session_id(0)
{
  user_commands= new UserCommands();
  status_vars= new StatusVars();
}

ScoreboardSlot::~ScoreboardSlot()
{
  delete user_commands;
  delete status_vars;
}

ScoreboardSlot::ScoreboardSlot(const ScoreboardSlot &scoreboard_slot)
{
  user_commands= new UserCommands(*scoreboard_slot.user_commands);
  status_vars= new StatusVars(*scoreboard_slot.status_vars);
  user.assign(scoreboard_slot.user);
  ip.assign(scoreboard_slot.ip);
  in_use= scoreboard_slot.in_use;
  session_id= scoreboard_slot.session_id;
}

UserCommands* ScoreboardSlot::getUserCommands()
{
  return user_commands;
}

StatusVars* ScoreboardSlot::getStatusVars()
{
  return status_vars;
}

void ScoreboardSlot::setSessionId(drizzled::session_id_t in_session_id)
{
  session_id= in_session_id;
}

drizzled::session_id_t ScoreboardSlot::getSessionId()
{
  return session_id;
}

void ScoreboardSlot::setInUse(bool in_in_use)
{
  in_use= in_in_use;
}

bool ScoreboardSlot::isInUse()
{
  return in_use;
}

void ScoreboardSlot::setUser(string in_user)
{
  user.assign(in_user);
}

const string& ScoreboardSlot::getUser()
{
  return user;
}

void ScoreboardSlot::setIp(string in_ip)
{
  ip.assign(in_ip);
}

const string& ScoreboardSlot::getIp()
{
  return ip;
}

void ScoreboardSlot::merge(ScoreboardSlot *score_board_slot)
{
  user_commands->merge(score_board_slot->getUserCommands());
  status_vars->merge(score_board_slot->getStatusVars());
}

void ScoreboardSlot::reset()
{
  in_use= false;
  session_id= 0;
  user_commands->reset();
  status_vars->reset();
}
