/* - mode: c; c-basic-offset: 2; indent-tabs-mode: nil; -*-
 *  vim:expandtab:shiftwidth=2:tabstop=2:smarttab:
 *
 * Copyright (C) 2009, Patrick "CaptTofu" Galbraith, Padraig O'Sullivan
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
 *   * Neither the name of Patrick Galbraith nor the names of its contributors
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
#include <drizzled/item/func.h>
#include <drizzled/function/str/strfunc.h>

#include "memcached_functions.h"
#include "memc_behavior_set.h"

#include <libmemcached/memcached.h>

#include <string>
#include <algorithm>

using namespace std;
using namespace drizzled;

void MemcachedBehaviorSet::setFailureString(const char *error)
{
  size_t size= strlen(error);
  failure_buff.realloc(size);
  failure_buff.length(size);
  memcpy(failure_buff.ptr(), error, size);
}

String *MemcachedBehaviorSet::val_str(String *str)
{
  memcached_return rc;
  memcached_behavior mbehavior;
  uint64_t isetting= 0;
  map<const string, memcached_behavior>::iterator it_behav;
  map<const string, uint64_t>::iterator it_hash;
  map<const string, uint64_t>::iterator it_dist;
  String *tmp_behavior;
  String *tmp_setting;

  if (arg_count != 2 ||
      ! (tmp_behavior= args[0]->val_str(str)) ||
      ! (tmp_setting= args[1]->val_str(str)) ||
      ! memc)
  {
    setFailureString("USAGE: memc_behavior_set('<behavior type>','<value>')");
    return &failure_buff;
  }

  string behavior(tmp_behavior->c_ptr());
  string setting(tmp_setting->c_ptr());

  /*
   * We don't want the user to have to type in all input in upper
   * case so we transform the input strings to upper case here.
   */
  std::transform(behavior.begin(), behavior.end(),
                 behavior.begin(), ::toupper);
  std::transform(setting.begin(), setting.end(),
                 setting.begin(), ::toupper);

  it_behav= behavior_map.find(behavior);
  if (it_behav == behavior_map.end())
  {
    setFailureString("UNKNOWN BEHAVIOR TYPE!");
    return &failure_buff;
  }
  mbehavior= behavior_map[behavior];

  switch (mbehavior)
  {
  case MEMCACHED_BEHAVIOR_SUPPORT_CAS:
  case MEMCACHED_BEHAVIOR_NO_BLOCK:
  case MEMCACHED_BEHAVIOR_BUFFER_REQUESTS:
  case MEMCACHED_BEHAVIOR_USER_DATA:
  case MEMCACHED_BEHAVIOR_SORT_HOSTS:
  case MEMCACHED_BEHAVIOR_VERIFY_KEY:
  case MEMCACHED_BEHAVIOR_TCP_NODELAY:
  case MEMCACHED_BEHAVIOR_KETAMA:
  case MEMCACHED_BEHAVIOR_CACHE_LOOKUPS:
    if (setting.compare("1") == 0)
    {
      isetting= 1;
    }
    else if (setting.compare("0") == 0)
    {
      isetting= 0;
    }
    else
    {
      setFailureString("INVALID VALUE FOR BEHAVIOR - MUST be 1 OR 0!");
      return &failure_buff;
    }
    break;
  case MEMCACHED_BEHAVIOR_DISTRIBUTION:
    it_dist= dist_settings_map.find(setting);
    if (it_dist == dist_settings_map.end())
    {
      setFailureString("INVALID VALUE FOR DISTRIBUTION!");
      return &failure_buff;
    }
    isetting= dist_settings_map[setting];
    break;
  case MEMCACHED_BEHAVIOR_HASH:
    it_hash= hash_settings_map.find(setting);
    if (it_hash == hash_settings_map.end())
    {
      setFailureString("INVALID VALUE FOR MEMCACHED HASH ALGORITHM!");
      return &failure_buff;
    }
    isetting= hash_settings_map[setting];
    break;
  case MEMCACHED_BEHAVIOR_KETAMA_HASH:
    isetting= ketama_hash_settings_map[setting];
    if (! isetting)
    {
      setFailureString("INVALID VALUE FOR KETAMA HASH ALGORITHM!");
      return &failure_buff;
    }
    break;
  case MEMCACHED_BEHAVIOR_SOCKET_SEND_SIZE:
  case MEMCACHED_BEHAVIOR_SOCKET_RECV_SIZE:
  case MEMCACHED_BEHAVIOR_POLL_TIMEOUT:
  case MEMCACHED_BEHAVIOR_CONNECT_TIMEOUT:
  case MEMCACHED_BEHAVIOR_RETRY_TIMEOUT:
  case MEMCACHED_BEHAVIOR_IO_MSG_WATERMARK:
  case MEMCACHED_BEHAVIOR_IO_BYTES_WATERMARK:
    /*
      What type of check the values passed to these behaviors?
      Range?
    */
    break;
  default:
    break;
  }

  rc= memcached_behavior_set(memc, mbehavior, isetting);

  if (rc != MEMCACHED_SUCCESS)
  {
    return &failure_buff;
  }

  return &success_buff;
}

