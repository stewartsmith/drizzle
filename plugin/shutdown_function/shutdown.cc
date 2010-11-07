/* -*- mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; -*-
 *  vim:expandtab:shiftwidth=2:tabstop=2:smarttab:
 *
 *  Copyright (C) 2010 Brian Aker
 */

#include "config.h"

#include <signal.h>
#include <drizzled/session.h>
#include <drizzled/function/str/strfunc.h>

using namespace drizzled;

#define SHUTDOWN_MESSAGE "Beginning shutdown"

class Shutdown :public Item_str_func
{
public:
  Shutdown() :
    Item_str_func()
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
  NULL,   /* system variables */
  NULL    /* config options */
}
DRIZZLE_DECLARE_PLUGIN_END;
