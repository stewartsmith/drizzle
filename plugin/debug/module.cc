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

#include <config.h>

#include <signal.h>

#include <drizzled/function/func.h>
#include <drizzled/item/cmpfunc.h>
#include <drizzled/item/function/boolean.h>
#include <drizzled/plugin/function.h>
#include <drizzled/util/backtrace.h>

using namespace drizzled;

namespace debug {

class Assert :public item::function::Boolean
{
public:
  Assert() :
    item::function::Boolean()
  {
    unsigned_flag= true;
  }

  const char *func_name() const { return "assert_and_crash"; }
  const char *fully_qualified_func_name() const { return "assert_and_crash()"; }

  bool val_bool()
  {
    String _res;
    String *res= args[0]->val_str(&_res);

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

class Backtrace :public item::function::Boolean
{
public:
  Backtrace() :
    item::function::Boolean()
  {
    unsigned_flag= true;
  }

  const char *func_name() const { return "backtrace"; }
  const char *fully_qualified_func_name() const { return "backtrace()"; }

  bool val_bool()
  {
    util::custom_backtrace();
    return true;
  }

  int64_t val_int()
  {
    return val_bool();
  }
};

class Crash :public item::function::Boolean
{
public:
  Crash() :
    item::function::Boolean()
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
  context.add(new drizzled::plugin::Create_function<debug::Assert>("assert_and_crash"));
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
  NULL,   /* depends */
  NULL    /* config options */
}
DRIZZLE_DECLARE_PLUGIN_END;
