/* 
 * Copyright (c) 2009, Padraig O'Sullivan
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
 *   * Neither the name of Padraig O'Sullivan nor the names of its contributors
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

#ifndef PLUGIN_MEMCACHED_STATS_SYSVAR_HOLDER_H
#define PLUGIN_MEMCACHED_STATS_SYSVAR_HOLDER_H

#include <pthread.h>

#include <string>

class SysvarHolder
{
public:

  static SysvarHolder &singleton()
  {
    static SysvarHolder holder;
    return holder;
  }

  void setServersString(const std::string &in_servers)
  {
    servers_string.assign(in_servers);
  }

  void setServersStringVar(const std::string &in_servers)
  {
    pthread_mutex_lock(&mutex);
    servers_string.assign(in_servers);
  }

  void updateServersSysvar(const char **var_ptr)
  {
    *var_ptr= servers_string.c_str();
    pthread_mutex_unlock(&mutex);
  }

  const std::string getServersString() const
  {
    return servers_string;
  }

  void setMemoryPtr(void *mem_ptr_in)
  {
    memory_ptr= mem_ptr_in;
  }

private:

  pthread_mutex_t mutex;

  std::string servers_string;
  void *memory_ptr;

  SysvarHolder()
    :
      mutex(),
      servers_string()
      memory_ptr(NULL)
  {
    pthread_mutex_init(&mutex, NULL);
  }

  ~SysvarHolder()
  {
    pthread_mutex_destroy(&mutex);
    free(memory_ptr);
  }

  SysvarHolder(const SysvarHolder&);
};

#endif /* PLUGIN_MEMCACHED_STATS_SYSVAR_HOLDER_H */
