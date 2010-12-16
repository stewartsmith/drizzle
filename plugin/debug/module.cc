/* -*- mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; -*-
 *  vim:expandtab:shiftwidth=2:tabstop=2:smarttab:
 *
 * Copyright (C) 2010 Brian Aker
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *
 *     * The names of its contributors may not be used to endorse or
 * promote products derived from this software without specific prior
 * written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */

#include "config.h"

#include <signal.h>

#include <drizzled/util/backtrace.h>
#include <drizzled/function/func.h>
#include <drizzled/item/cmpfunc.h>

using namespace drizzled;

namespace debug {

class Assert :public Item_bool_func
{
public:
  Assert() :
    Item_bool_func()
  {
    unsigned_flag= true;
  }

  const char *func_name() const { return "assert"; }
  const char *fully_qualified_func_name() const { return "assert()"; }

  bool val_bool()
  {
    drizzled::String _res;
    drizzled::String *res= args[0]->val_str(&_res);

    null_value= false;

    if (not res or not res->length())
    {
      assert(0);
      return true;
    }

    return false;
  }

  int64_t val_int()
  {
    return val_bool();
  }

  bool check_argument_count(int n)
  {
    return (n == 1);
  }
};

class Backtrace :public Item_bool_func
{
public:
  Backtrace() :
    Item_bool_func()
  {
    unsigned_flag= true;
  }

  const char *func_name() const { return "backtrace"; }
  const char *fully_qualified_func_name() const { return "backtrace()"; }

  bool val_bool()
  {
    drizzled::util::custom_backtrace();
    return true;
  }

  int64_t val_int()
  {
    return val_bool();
  }
};

class Crash :public Item_bool_func
{
public:
  Crash() :
    Item_bool_func()
  { }

  const char *func_name() const { return "crash"; }
  const char *fully_qualified_func_name() const { return "crash()"; }

  bool val_bool()
  {
    raise(SIGSEGV);

    return true;
  }

  int64_t val_int()
  {
    return val_bool();
  }
};

} // namespace debug

static int initialize(drizzled::module::Context &context)
{
  context.add(new drizzled::plugin::Create_function<debug::Assert>("assert"));
  context.add(new drizzled::plugin::Create_function<debug::Backtrace>("backtrace"));
  context.add(new drizzled::plugin::Create_function<debug::Crash>("crash"));

  return 0;
}

DRIZZLE_DECLARE_PLUGIN
{
  DRIZZLE_VERSION_ID,
  "debug",
  "1.1",
  "Brian Aker",
  "Useful functions for programmers to debug the server.",
  PLUGIN_LICENSE_BSD,
  initialize, /* Plugin Init */
  NULL,   /* system variables */
  NULL    /* config options */
}
DRIZZLE_DECLARE_PLUGIN_END;
