/* -*- mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; -*-
 *  vim:expandtab:shiftwidth=2:tabstop=2:smarttab:
 *
 *  Copyright (C) 2008 Sun Microsystems, Inc.
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

#include <config.h>

#include <drizzled/charset.h>
#include <drizzled/function/str/strfunc.h>
#include <drizzled/item/func.h>
#include <drizzled/plugin/function.h>

#include <uuid/uuid.h>

#define UUID_LENGTH (8+1+4+1+4+1+4+1+12)

namespace plugin {
namespace uuid {

class Generate: public drizzled::Item_str_func
{
public:
  Generate(): drizzled::Item_str_func() {}
  void fix_length_and_dec()
  {
    collation.set(drizzled::system_charset_info);
    /*
       NOTE! uuid() should be changed to use 'ascii'
       charset when hex(), format(), md5(), etc, and implicit
       number-to-string conversion will use 'ascii'
    */
    max_length= UUID_LENGTH * drizzled::system_charset_info->mbmaxlen;
  }
  const char *func_name() const{ return "uuid"; }
  drizzled::String *val_str(drizzled::String *);
};

drizzled::String *Generate::val_str(drizzled::String *str)
{
  uuid_t uu;
  char *uuid_string;

  /* 36 characters for uuid string +1 for NULL */
  str->realloc(UUID_LENGTH+1);
  str->length(UUID_LENGTH);
  str->set_charset(drizzled::system_charset_info);
  uuid_string= (char *) str->ptr();
  uuid_generate(uu);
  uuid_unparse(uu, uuid_string);

  return str;
}

} // uuid
} // plugin

static int initialize(drizzled::module::Context &context)
{
  context.add(new drizzled::plugin::Create_function<plugin::uuid::Generate>("uuid"));

  return 0;
}

DRIZZLE_DECLARE_PLUGIN
{
  DRIZZLE_VERSION_ID,
  "uuid",
  "1.1",
  "Stewart Smith, Brian Aker",
  "UUID() function using libuuid",
  drizzled::PLUGIN_LICENSE_GPL,
  initialize, /* Plugin Init */
  NULL,   /* depends */
  NULL    /* config options */
}
DRIZZLE_DECLARE_PLUGIN_END;
