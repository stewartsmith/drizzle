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

#pragma once

#include <drizzled/item/func.h>

#include <libmemcached/memcached.h>

#include <map>
#include <string>

/* implements memc_behavior_set */
class MemcachedBehaviorSet : public drizzled::Item_str_func
{
public:
  MemcachedBehaviorSet()
    : 
      Item_str_func(),
      failure_buff("0", &drizzled::my_charset_bin),
      success_buff("1", &drizzled::my_charset_bin),
      behavior_map(),
      dist_settings_map(),
      hash_settings_map(),
      ketama_hash_settings_map()
  {
    behavior_map.insert(std::pair<const std::string, memcached_behavior>
      ("MEMCACHED_BEHAVIOR_SUPPORT_CAS", MEMCACHED_BEHAVIOR_SUPPORT_CAS));
    behavior_map.insert(std::pair<const std::string, memcached_behavior>
      ("MEMCACHED_BEHAVIOR_NO_BLOCK", MEMCACHED_BEHAVIOR_NO_BLOCK));
    behavior_map.insert(std::pair<const std::string, memcached_behavior>
      ("MEMCACHED_BEHAVIOR_TCP_NODELAY", MEMCACHED_BEHAVIOR_TCP_NODELAY));
    behavior_map.insert(std::pair<const std::string, memcached_behavior>
      ("MEMCACHED_BEHAVIOR_HASH", MEMCACHED_BEHAVIOR_HASH));
    behavior_map.insert(std::pair<const std::string, memcached_behavior>
      ("MEMCACHED_BEHAVIOR_CACHE_LOOKUPS", MEMCACHED_BEHAVIOR_CACHE_LOOKUPS));
    behavior_map.insert(std::pair<const std::string, memcached_behavior>
      ("MEMCACHED_BEHAVIOR_SOCKET_SEND_SIZE", MEMCACHED_BEHAVIOR_SOCKET_SEND_SIZE));
    behavior_map.insert(std::pair<const std::string, memcached_behavior>
      ("MEMCACHED_BEHAVIOR_SOCKET_RECV_SIZE", MEMCACHED_BEHAVIOR_SOCKET_RECV_SIZE));
    behavior_map.insert(std::pair<const std::string, memcached_behavior>
      ("MEMCACHED_BEHAVIOR_BUFFER_REQUESTS", MEMCACHED_BEHAVIOR_BUFFER_REQUESTS));
    behavior_map.insert(std::pair<const std::string, memcached_behavior>
      ("MEMCACHED_BEHAVIOR_KETAMA", MEMCACHED_BEHAVIOR_KETAMA));
    behavior_map.insert(std::pair<const std::string, memcached_behavior>
      ("MEMCACHED_BEHAVIOR_POLL_TIMEOUT", MEMCACHED_BEHAVIOR_POLL_TIMEOUT));
    behavior_map.insert(std::pair<const std::string, memcached_behavior>
      ("MEMCACHED_BEHAVIOR_RETRY_TIMEOUT", MEMCACHED_BEHAVIOR_RETRY_TIMEOUT));
    behavior_map.insert(std::pair<const std::string, memcached_behavior>
      ("MEMCACHED_BEHAVIOR_DISTRIBUTION", MEMCACHED_BEHAVIOR_DISTRIBUTION));
    behavior_map.insert(std::pair<const std::string, memcached_behavior>
      ("MEMCACHED_BEHAVIOR_USER_DATA", MEMCACHED_BEHAVIOR_USER_DATA));
    behavior_map.insert(std::pair<const std::string, memcached_behavior>
      ("MEMCACHED_BEHAVIOR_SORT_HOSTS", MEMCACHED_BEHAVIOR_SORT_HOSTS));
    behavior_map.insert(std::pair<const std::string, memcached_behavior>
      ("MEMCACHED_BEHAVIOR_VERIFY_KEY", MEMCACHED_BEHAVIOR_VERIFY_KEY));
    behavior_map.insert(std::pair<const std::string, memcached_behavior>
      ("MEMCACHED_BEHAVIOR_CONNECT_TIMEOUT", MEMCACHED_BEHAVIOR_CONNECT_TIMEOUT));
    behavior_map.insert(std::pair<const std::string, memcached_behavior>
      ("MEMCACHED_BEHAVIOR_KETAMA_WEIGHTED", MEMCACHED_BEHAVIOR_KETAMA_WEIGHTED));
    behavior_map.insert(std::pair<const std::string, memcached_behavior>
      ("MEMCACHED_BEHAVIOR_KETAMA_HASH", MEMCACHED_BEHAVIOR_KETAMA_HASH));
    behavior_map.insert(std::pair<const std::string, memcached_behavior>
      ("MEMCACHED_BEHAVIOR_BINARY_PROTOCOL", MEMCACHED_BEHAVIOR_BINARY_PROTOCOL));
    behavior_map.insert(std::pair<const std::string, memcached_behavior>
      ("MEMCACHED_BEHAVIOR_SND_TIMEOUT", MEMCACHED_BEHAVIOR_SND_TIMEOUT));
    behavior_map.insert(std::pair<const std::string, memcached_behavior>
      ("MEMCACHED_BEHAVIOR_RCV_TIMEOUT", MEMCACHED_BEHAVIOR_RCV_TIMEOUT));
    behavior_map.insert(std::pair<const std::string, memcached_behavior>
      ("MEMCACHED_BEHAVIOR_SERVER_FAILURE_LIMIT", MEMCACHED_BEHAVIOR_SERVER_FAILURE_LIMIT));
    behavior_map.insert(std::pair<const std::string, memcached_behavior>
      ("MEMCACHED_BEHAVIOR_IO_MSG_WATERMARK", MEMCACHED_BEHAVIOR_IO_MSG_WATERMARK));
    behavior_map.insert(std::pair<const std::string, memcached_behavior>
      ("MEMCACHED_BEHAVIOR_IO_BYTES_WATERMARK", MEMCACHED_BEHAVIOR_IO_BYTES_WATERMARK));

    dist_settings_map.insert(std::pair<const std::string, uint64_t>
      ("MEMCACHED_DISTRIBUTION_MODULA", MEMCACHED_DISTRIBUTION_MODULA));
    dist_settings_map.insert(std::pair<const std::string, uint64_t>
      ("MEMCACHED_DISTRIBUTION_CONSISTENT", MEMCACHED_DISTRIBUTION_CONSISTENT));
    dist_settings_map.insert(std::pair<const std::string, uint64_t>
      ("MEMCACHED_DISTRIBUTION_CONSISTENT_KETAMA", MEMCACHED_DISTRIBUTION_CONSISTENT_KETAMA));

    hash_settings_map.insert(std::pair<const std::string, uint64_t>
      ("MEMCACHED_HASH_DEFAULT", MEMCACHED_HASH_DEFAULT));
    hash_settings_map.insert(std::pair<const std::string, uint64_t>
      ("MEMCACHED_HASH_MD5", MEMCACHED_HASH_MD5));
    hash_settings_map.insert(std::pair<const std::string, uint64_t>
      ("MEMCACHED_HASH_CRC", MEMCACHED_HASH_CRC));
    hash_settings_map.insert(std::pair<const std::string, uint64_t>
      ("MEMCACHED_HASH_FNV1_64", MEMCACHED_HASH_FNV1_64));
    hash_settings_map.insert(std::pair<const std::string, uint64_t>
      ("MEMCACHED_HASH_FNV1A_64", MEMCACHED_HASH_FNV1A_64));
    hash_settings_map.insert(std::pair<const std::string, uint64_t>
      ("MEMCACHED_HASH_FNV1_32", MEMCACHED_HASH_FNV1_32));
    hash_settings_map.insert(std::pair<const std::string, uint64_t>
      ("MEMCACHED_HASH_FNV1A_32", MEMCACHED_HASH_FNV1A_32));
    hash_settings_map.insert(std::pair<const std::string, uint64_t>
      ("MEMCACHED_HASH_JENKINS", MEMCACHED_HASH_JENKINS));
    hash_settings_map.insert(std::pair<const std::string, uint64_t>
      ("MEMCACHED_HASH_HSIEH", MEMCACHED_HASH_HSIEH));
    hash_settings_map.insert(std::pair<const std::string, uint64_t>
      ("MEMCACHED_HASH_MURMUR", MEMCACHED_HASH_MURMUR));

    ketama_hash_settings_map.insert(std::pair<const std::string, uint64_t>
      ("MEMCACHED_HASH_DEFAULT", MEMCACHED_HASH_DEFAULT));
    ketama_hash_settings_map.insert(std::pair<const std::string, uint64_t>
      ("MEMCACHED_HASH_MD5", MEMCACHED_HASH_MD5));
    ketama_hash_settings_map.insert(std::pair<const std::string, uint64_t>
      ("MEMCACHED_HASH_CRC", MEMCACHED_HASH_CRC));
    ketama_hash_settings_map.insert(std::pair<const std::string, uint64_t>
      ("MEMCACHED_HASH_FNV1_64", MEMCACHED_HASH_FNV1_64));
    ketama_hash_settings_map.insert(std::pair<const std::string, uint64_t>
      ("MEMCACHED_HASH_FNV1A_64", MEMCACHED_HASH_FNV1A_64));
    ketama_hash_settings_map.insert(std::pair<const std::string, uint64_t>
      ("MEMCACHED_HASH_FNV1_32", MEMCACHED_HASH_FNV1_32));
    ketama_hash_settings_map.insert(std::pair<const std::string, uint64_t>
      ("MEMCACHED_HASH_FNV1A_32", MEMCACHED_HASH_FNV1A_32));
  }

  const char *func_name() const
  {
    return "memc_behavior_set";
  }

  drizzled::String *val_str(drizzled::String *);

  void fix_length_and_dec()
  {
    max_length= 32;
  }

private:
  void setFailureString(const char *error);

  drizzled::String failure_buff;
  drizzled::String success_buff;

  std::map<const std::string, memcached_behavior> behavior_map;
  std::map<const std::string, uint64_t> dist_settings_map;
  std::map<const std::string, uint64_t> hash_settings_map;
  std::map<const std::string, uint64_t> ketama_hash_settings_map;
};

