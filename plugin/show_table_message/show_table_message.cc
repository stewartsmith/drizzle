/* vim: expandtab:shiftwidth=2:tabstop=2:smarttab:
   Copyright (C) 2009 Sun Microsystems
   Copyright (C) 2010 Stewart Smith

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
#include <drizzled/plugin/function.h>
#include <drizzled/item/func.h>
#include <drizzled/function/str/strfunc.h>
#include <drizzled/error.h>
#include <drizzled/current_session.h>
#include <drizzled/charset.h>

#include <stdio.h>
#include <iostream>
#include <string>
#include <drizzled/message/table.pb.h>
#include <google/protobuf/io/zero_copy_stream.h>
#include <google/protobuf/io/zero_copy_stream_impl.h>
#include <google/protobuf/text_format.h>

using namespace std;
using namespace drizzled;
using namespace google;

class ShowTableMessageFunction : public Item_str_func
{
public:
  ShowTableMessageFunction() : Item_str_func() {}
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
    return "show_table_message";
  }

  bool check_argument_count(int n)
  {
    return (n == 2);
  }
};


String *ShowTableMessageFunction::val_str(String *str)
{
  assert(fixed == true);

  String *db_sptr= args[0]->val_str(str);
  String *table_name_sptr= args[1]->val_str(str);

  if (db_sptr == NULL || table_name_sptr == NULL)
  {
    null_value= true;
    return NULL;
  }

  null_value= false;

  const char* db= db_sptr->c_ptr_safe();
  const char* table_name= table_name_sptr->c_ptr_safe();

  string proto_as_text("");
  message::Table proto;

  TableIdentifier table_identifier(db, table_name);

  int err= plugin::StorageEngine::getTableDefinition(*current_session, table_identifier, &proto);

  if (err != EEXIST)
  {
    my_error(ER_NO_SUCH_TABLE, MYF(0), db, table_name);
    return NULL;
  }

  protobuf::TextFormat::PrintToString(proto, &proto_as_text);

  if (str->alloc(proto_as_text.length()))
  {
    null_value= true;
    return NULL;
  }

  str->length(proto_as_text.length());

  strncpy(str->ptr(),proto_as_text.c_str(), proto_as_text.length());

  return str;
}

plugin::Create_function<ShowTableMessageFunction> *show_table_message_func= NULL;

static int initialize(plugin::Registry &registry)
{
  show_table_message_func= new plugin::Create_function<ShowTableMessageFunction>("show_table_message");
  registry.add(show_table_message_func);
  return 0;
}

static int finalize(plugin::Registry &registry)
{
  registry.remove(show_table_message_func);
  delete show_table_message_func;
  return 0;
}

DRIZZLE_DECLARE_PLUGIN
{
  DRIZZLE_VERSION_ID,
  "show_table_message",
  "1.0",
  "Stewart Smith",
  "Shows text representation of table definition protobuf message",
  PLUGIN_LICENSE_GPL,
  initialize, /* Plugin Init */
  finalize,   /* Plugin Deinit */
  NULL,   /* system variables */
  NULL    /* config options */
}
DRIZZLE_DECLARE_PLUGIN_END;
