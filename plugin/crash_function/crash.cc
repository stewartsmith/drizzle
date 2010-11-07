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

class Crash :public Item_str_func
{
public:
  Crash() :
    Item_str_func()
  { }


  void fix_length_and_dec()
  {
    max_length= MAX_FIELD_NAME * system_charset_info->mbmaxlen;
    maybe_null= true;
  }
  const char *func_name() const { return "crash"; }
  const char *fully_qualified_func_name() const { return "crash()"; }

  String *val_str(String *str)
  {
    raise(SIGSEGV);

    return str;
  }
};

static int initialize(drizzled::module::Context &context)
{
  context.add(new plugin::Create_function<Crash>("crash"));
  return 0;
}

DRIZZLE_DECLARE_PLUGIN
{
  DRIZZLE_VERSION_ID,
  "crash",
  "1.0",
  "Brian Aker",
  "Cause the database to crash.",
  PLUGIN_LICENSE_BSD,
  initialize, /* Plugin Init */
  NULL,   /* system variables */
  NULL    /* config options */
}
DRIZZLE_DECLARE_PLUGIN_END;
