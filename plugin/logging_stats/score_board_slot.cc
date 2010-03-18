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
 *
 */

#include "score_board_slot.h"

using namespace std;

ScoreBoardSlot::ScoreBoardSlot()
  :
    in_use(false),
    session_id(0)
{
  user_commands= new UserCommands();
}

ScoreBoardSlot::~ScoreBoardSlot()
{
  delete user_commands;
}

UserCommands* ScoreBoardSlot::getUserCommands()
{
  return user_commands;
}

void ScoreBoardSlot::setSessionId(uint64_t in_session_id)
{
  session_id= in_session_id;
}

uint64_t ScoreBoardSlot::getSessionId()
{
  return session_id;
}

void ScoreBoardSlot::setInUse(bool in_in_use)
{
  in_use= in_in_use;
}

bool ScoreBoardSlot::isInUse()
{
  return in_use;
}

void ScoreBoardSlot::setUser(string in_user)
{
  user= in_user;
}

const string& ScoreBoardSlot::getUser()
{
  return user;
}

void ScoreBoardSlot::setIp(string in_ip)
{
  ip= in_ip;
}

const string& ScoreBoardSlot::getIp()
{
  return ip;
}

void ScoreBoardSlot::merge(ScoreBoardSlot *score_board_slot)
{
  user_commands->merge(score_board_slot->getUserCommands());
}

void ScoreBoardSlot::reset()
{
  in_use= false;
  session_id= 0;
  user_commands->reset();
}
