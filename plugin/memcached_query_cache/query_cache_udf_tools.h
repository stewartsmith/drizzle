/* 
 * Copyright (C) 2010 Djellel Eddine Difallah
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
 *   * Neither the name of Djellel Eddine Difallah nor the names of its contributors
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
 *
 * Defines the PRINT_QUERY_CACHE_META(KEY) UDF
 *
 * @details
 *
 * PRINT_QUERY_CACHE_META(key);
 *
 * prints a text representation of the meta data associated
 * to a key stored in the cache
 */

#pragma once

#include <drizzled/item/func.h>
#include <drizzled/function/str/strfunc.h>

class PrintQueryCacheMetaFunction : public drizzled::Item_str_func
{
public:
  PrintQueryCacheMetaFunction() : Item_str_func() {}
  drizzled::String *val_str(drizzled::String*);

  void fix_length_and_dec();

  const char *func_name() const
  {
    return "print_query_cache_meta";
  }

  bool check_argument_count(int n)
  {
    return (n == 1);
  }
};
class QueryCacheFlushFunction: public drizzled::Item_int_func
{
  drizzled::String value;
  drizzled::String failure_buff;
public:
  QueryCacheFlushFunction()
    :
      Item_int_func(),
      failure_buff("0", &drizzled::my_charset_bin)
  {}

  const char *func_name() const
  {
    return "query_cache_flush";
  }

  int64_t val_int();

  void fix_length_and_dec()
  {
    max_length= 32;
  }
  /*
   * query_cache_flush takes 0 or 1 arguments
   */
  bool check_argument_count(int n)
  {
    return ((n == 1)||(n == 0));
  }

};

