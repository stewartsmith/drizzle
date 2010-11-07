/* -*- mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; -*-
 *  vim:expandtab:shiftwidth=2:tabstop=2:smarttab:
 *
 *  Copyright (C) 2008 Sun Microsystems
 *  Copyright (C) 2010 Stewart Smith
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; version 2 of the License.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 */

#include "config.h"
#include <drizzled/plugin/function.h>
#include <drizzled/item/func.h>
#include <drizzled/function/str/strfunc.h>
#include <uuid/uuid.h>

#define UUID_LENGTH (8+1+4+1+4+1+4+1+12)

using namespace drizzled;

class UuidFunction: public Item_str_func
{
public:
  UuidFunction(): Item_str_func() {}
  void fix_length_and_dec() {
    collation.set(system_charset_info);
    /*
       NOTE! uuid() should be changed to use 'ascii'
       charset when hex(), format(), md5(), etc, and implicit
       number-to-string conversion will use 'ascii'
    */
    max_length= UUID_LENGTH * system_charset_info->mbmaxlen;
  }
  const char *func_name() const{ return "uuid"; }
  String *val_str(String *);
};

String *UuidFunction::val_str(String *str)
{
  uuid_t uu;
  char *uuid_string;

  /* 36 characters for uuid string +1 for NULL */
  str->realloc(UUID_LENGTH+1);
  str->length(UUID_LENGTH);
  str->set_charset(system_charset_info);
  uuid_string= (char *) str->ptr();
  uuid_generate_random(uu);
  uuid_unparse(uu, uuid_string);

  return str;
}

plugin::Create_function<UuidFunction> *uuid_function= NULL;

static int initialize(drizzled::module::Context &context)
{
  uuid_function= new plugin::Create_function<UuidFunction>("uuid");
  context.add(uuid_function);
  return 0;
}

DRIZZLE_DECLARE_PLUGIN
{
  DRIZZLE_VERSION_ID,
  "uuid",
  "1.0",
  "Stewart Smith",
  "UUID() function using libuuid",
  PLUGIN_LICENSE_GPL,
  initialize, /* Plugin Init */
  NULL,   /* system variables */
  NULL    /* config options */
}
DRIZZLE_DECLARE_PLUGIN_END;
