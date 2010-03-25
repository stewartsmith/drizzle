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

#ifndef PLUGIN_LOGGING_STATS_SCORE_BOARD_SLOT_H
#define PLUGIN_LOGGING_STATS_SCORE_BOARD_SLOT_H

#include "user_commands.h"
#include <drizzled/atomics.h>

#include <string>

class ScoreBoardSlot
{
public:
  ScoreBoardSlot() 
    :
      session_id(0)
  {
    in_use= false;
  }

  ~ScoreBoardSlot() 
  {
    delete user_commands;
  }

  void setUserCommands(UserCommands *in_user_commands)
  {
    user_commands= in_user_commands;
  }

  UserCommands* getUserCommands()
  {
    return user_commands;
  }

  void setSessionId(uint64_t in_session_id)
  {
    session_id= in_session_id;
  }

  uint64_t getSessionId()
  {
    return session_id;
  }

  void setInUse()
  {
    in_use= true;
  }

  bool isInUse() const
  {
    return in_use;
  }

  void setUser(std::string in_user)
  {
    user= in_user;
  }

  const std::string& getUser()
  {
    return user; 
  }

  void setIp(std::string in_ip)
  {
    ip= in_ip;
  }

  const std::string& getIp()
  {
    return ip;
  }

  void reset()
  {
    in_use= false;
    session_id= 0;
    if (user_commands)
    {
      user_commands->reset();
    }
  }

private:
  UserCommands *user_commands;
  std::string user;
  std::string ip;
  drizzled::atomic<bool> in_use;
  uint64_t session_id;
};
 
#endif /* PLUGIN_LOGGING_STATS_SCORE_BOARD_SLOT_H */
