/* 
 * Copyright (c) 2010, Djellel Eddine Difallah
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
 * Implements the PRINT_QUERY_CACHE_META(key) and QUERY_CACHE_FLUSH(expiry_time) UDFs.
 */

#include "config.h"
#include <drizzled/plugin/function.h>
#include <drizzled/item/func.h>
#include <drizzled/function/str/strfunc.h>
#include <drizzled/error.h>
#include "drizzled/internal/my_sys.h"
#include "drizzled/charset.h"

#include <fcntl.h>

#include "query_cache_udf_tools.h"
#include "query_cache_service.h"
#include "memcached_qc.h"

#include <drizzled/message/resultset.pb.h>
#include <google/protobuf/io/zero_copy_stream.h>
#include <google/protobuf/io/zero_copy_stream_impl.h>
#include <google/protobuf/io/coded_stream.h>
#include <google/protobuf/text_format.h>

using namespace std;
using namespace drizzled;
using namespace google;


plugin::Create_function<PrintQueryCacheMetaFunction> *print_query_cache_meta_func_factory= NULL;

void PrintQueryCacheMetaFunction::fix_length_and_dec()
{
  max_length= 2 * 1024 * 1024; /* 2MB size limit seems ok... */
  args[0]->collation.set(
    get_charset_by_csname(args[0]->collation.collation->csname,
                          MY_CS_BINSORT), DERIVATION_COERCIBLE);
}

String *PrintQueryCacheMetaFunction::val_str(String *str)
{
  assert(fixed == true);

  String *key_arg= args[0]->val_str(str);

  if (key_arg == NULL)
  {
    my_error(ER_INVALID_NULL_ARGUMENT, MYF(0), func_name());
    null_value= true;
    return NULL;
  }

  null_value= false;

  map<string, message::Resultset>::iterator it= QueryCacheService::cache.find(String_to_std_string(*key_arg));

  if (it == QueryCacheService::cache.end())
  {
    my_error(ER_INVALID_NULL_ARGUMENT, MYF(0), func_name());
    null_value= true;  
    return NULL;
  }

  message::Resultset resultset_message= it->second;

  string resultset_text;
  protobuf::TextFormat::PrintToString(resultset_message, &resultset_text);

  if (str->alloc(resultset_text.length()))
  {
    null_value= true;
    return NULL;
  }

  str->length(resultset_text.length());

  strncpy(str->ptr(), resultset_text.c_str(), resultset_text.length());

  return str;
}

int64_t QueryCacheFlushFunction::val_int()
{
  bool res;
  time_t expiration= 0;
  null_value= false;

  if ((arg_count != 0 && arg_count != 1) ||!MemcachedQueryCache::getClient())
  {
    return 0;
  }

  if (arg_count == 1)
  {
    String *tmp_exp= args[0]->val_str(&value);

    expiration= (time_t)atoi(tmp_exp->c_ptr());
  }

  res= MemcachedQueryCache::getClient()->flush(expiration);
  QueryCacheService::cache.clear();
  QueryCacheService::cachedTables.clear();

  if (not res)
  {
    return 0;
  }

  return 1;
}
