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
#include "memc_behavior_get.h"

#include <libmemcached/memcached.h>

#include <string>
#include <algorithm>

using namespace std;
using namespace drizzled;

void MemcachedBehaviorGet::setFailureString(const char *error)
{
  size_t size= strlen(error);
  failure_buff.realloc(size);
  failure_buff.length(size);
  memcpy(failure_buff.ptr(), error, size);
}

String *MemcachedBehaviorGet::val_str(String *str)
{
  memcached_behavior mbehavior;
  uint64_t isetting;
  map<const string, memcached_behavior>::iterator it;
  String *tmp_behavior;

  if (arg_count != 1 ||
      ! (tmp_behavior= args[0]->val_str(str)) ||
      ! memc)
  {
    setFailureString("USAGE: memc_behavior_get('<behavior type>')");
    return &failure_buff;
  }

  string behavior(tmp_behavior->c_ptr());

  /*
   * We don't want the user to have to type in all input in upper
   * case so we transform the input strings to upper case here.
   */
  std::transform(behavior.begin(), behavior.end(),
                 behavior.begin(), ::toupper);

  it = behavior_map.find(behavior);
  if (it == behavior_map.end()) 
  {
    setFailureString("UNKNOWN BEHAVIOR TYPE!");
    return &failure_buff;
  }
	
  mbehavior= behavior_map[behavior];

  isetting= memcached_behavior_get(memc, mbehavior);

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
    if (isetting == 1)
      return_buff.append("1");
    else if (isetting == 0)
      return_buff.append("0");
    else
    {
      setFailureString("INVALID VALUE FOR BEHAVIOR - MUST be 1 OR 0!");
      return &failure_buff;
    }
    break;
  case MEMCACHED_BEHAVIOR_DISTRIBUTION:
    {
      string setting(dist_settings_reverse_map[isetting]);
      return_buff.append(setting.c_str());
    }
    break;
  case MEMCACHED_BEHAVIOR_HASH:
    {
      string setting(hash_settings_reverse_map[isetting]);
      return_buff.append(setting.c_str());
    }
    break;
  case MEMCACHED_BEHAVIOR_KETAMA_HASH:
    {
      string setting(ketama_hash_settings_reverse_map[isetting]);
      return_buff.append(setting.c_str());
    }
    break;
  case MEMCACHED_BEHAVIOR_SOCKET_SEND_SIZE:
  case MEMCACHED_BEHAVIOR_SOCKET_RECV_SIZE:
  case MEMCACHED_BEHAVIOR_POLL_TIMEOUT:
  case MEMCACHED_BEHAVIOR_CONNECT_TIMEOUT:
  case MEMCACHED_BEHAVIOR_RETRY_TIMEOUT:
  case MEMCACHED_BEHAVIOR_IO_MSG_WATERMARK:
  case MEMCACHED_BEHAVIOR_IO_BYTES_WATERMARK:
    {
      size_t setting_len= 0;
      char tmp_buff[16];

      snprintf(tmp_buff, 16, "%"PRIu64, isetting);
      setting_len= strlen(tmp_buff);
      return_buff.realloc(setting_len);
      return_buff.length(setting_len);
      memcpy(return_buff.ptr(),tmp_buff, setting_len);
    }
    break;
  default:
    break;
  }

  return &return_buff;
}
