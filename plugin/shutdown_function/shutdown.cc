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
#include <drizzled/session.h>
#include <drizzled/sql_base.h>
#include <drizzled/function/str/strfunc.h>
#include <drizzled/plugin/function.h>

using namespace drizzled;

#define SHUTDOWN_MESSAGE "Beginning shutdown"

class Shutdown : public Item_str_func
{
public:
  Shutdown()
  { }

  void fix_length_and_dec()
  {
    max_length= sizeof(SHUTDOWN_MESSAGE) * system_charset_info->mbmaxlen;
    maybe_null= true;
  }
  const char *func_name() const { return "shutdown"; }
  const char *fully_qualified_func_name() const { return "shutdown()"; }

  String *val_str(String *str)
  {
    kill_drizzle();

    str->copy(SHUTDOWN_MESSAGE, sizeof(SHUTDOWN_MESSAGE) -1, system_charset_info);

    return str;
  }
};

static int initialize(drizzled::module::Context &context)
{
  context.add(new plugin::Create_function<Shutdown>("shutdown"));
  return 0;
}

DRIZZLE_DECLARE_PLUGIN
{
  DRIZZLE_VERSION_ID,
  "shutdown",
  "1.0",
  "Brian Aker",
  "Cause the database to shutdown.",
  PLUGIN_LICENSE_BSD,
  initialize, /* Plugin Init */
  NULL,   /* depends */
  NULL    /* config options */
}
DRIZZLE_DECLARE_PLUGIN_END;
