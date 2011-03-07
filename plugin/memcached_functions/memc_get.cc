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
/**
 * @file
 *   memc_get UDF implementation 
 */

#include <config.h>
#include <drizzled/function/str/strfunc.h>

#include "memcached_functions.h"
#include "memc_get.h"

#include <libmemcached/memcached.h>

#include <string>

using namespace std;
using namespace drizzled;

/**
 * value function for memc_get UDF
 *
 * @return string value from memcached 
 */
String *MemcachedGet::val_str(String *str)
{
  memcached_return rc;
  String *key;
  uint32_t flags;
  size_t val_len;

  if (arg_count != 1 ||
      ! (key= args[0]->val_str(str)) ||
      ! memc)
  {
    return &failure_buff;
  }

  char *value= memcached_get(memc, key->c_ptr(), key->length(), &val_len, &flags, &rc);
  if (rc != MEMCACHED_SUCCESS)
  {
    null_value= true;
    return NULL;
  }

  buffer.realloc(val_len);
  buffer.length(val_len);
  memcpy(buffer.ptr(), value, val_len);
  free(value);

  return &buffer;
}

