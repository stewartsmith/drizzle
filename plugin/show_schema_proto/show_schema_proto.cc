/* vim: expandtab:shiftwidth=2:tabstop=2:smarttab:
   Copyright (C) 2009 Sun Microsystems, Inc.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA */

#include <config.h>

#include <drizzled/charset.h>
#include <drizzled/error.h>
#include <drizzled/function/str/strfunc.h>
#include <drizzled/internal/my_sys.h>
#include <drizzled/item/func.h>
#include <drizzled/message/schema.h>
#include <drizzled/plugin/function.h>
#include <drizzled/plugin/storage_engine.h>

#include <iostream>
#include <stdio.h>
#include <string>

#include <google/protobuf/io/zero_copy_stream.h>
#include <google/protobuf/io/zero_copy_stream_impl.h>
#include <google/protobuf/text_format.h>

using namespace std;
using namespace drizzled;
using namespace google;

class ShowSchemaProtoFunction : public Item_str_func
{
public:
  ShowSchemaProtoFunction() : Item_str_func() {}
  String *val_str(String*);

  void fix_length_and_dec()
  {
    max_length= 16384; /* Guesswork? */
    args[0]->collation.set(
      get_charset_by_csname(args[0]->collation.collation->csname,
                            MY_CS_BINSORT), DERIVATION_COERCIBLE);
  }

  const char *func_name() const
  {
    return "show_schema_proto";
  }

  bool check_argument_count(int n)
  {
    return (n == 1);
  }
};


String *ShowSchemaProtoFunction::val_str(String *str)
{
  assert(fixed == true);

  String *db_sptr= args[0]->val_str(str);

  if (db_sptr == NULL)
  {
    null_value= true;
    return NULL;
  }

  null_value= false;

  const char* db= db_sptr->c_ptr_safe();

  string proto_as_text("");
  message::schema::shared_ptr proto;


  identifier::Schema schema_identifier(db);
  if (not (proto= plugin::StorageEngine::getSchemaDefinition(schema_identifier)))
  {
    my_error(ER_BAD_DB_ERROR, schema_identifier);
    return NULL;
  }

  protobuf::TextFormat::PrintToString(*proto, &proto_as_text);

  str->alloc(proto_as_text.length());
  str->length(proto_as_text.length());

  strncpy(str->ptr(),proto_as_text.c_str(), proto_as_text.length());

  return str;
}

plugin::Create_function<ShowSchemaProtoFunction> *show_schema_proto_func= NULL;

static int initialize(module::Context &context)
{
  show_schema_proto_func= new plugin::Create_function<ShowSchemaProtoFunction>("show_schema_proto");
  context.add(show_schema_proto_func);
  return 0;
}

DRIZZLE_DECLARE_PLUGIN
{
  DRIZZLE_VERSION_ID,
  "show_schema_proto",
  "1.0",
  "Stewart Smith",
  "Shows text representation of schema definition proto",
  PLUGIN_LICENSE_GPL,
  initialize, /* Plugin Init */
  NULL,   /* depends */
  NULL    /* config options */
}
DRIZZLE_DECLARE_PLUGIN_END;
