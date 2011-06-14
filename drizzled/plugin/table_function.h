/* -*- mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; -*-
 *  vim:expandtab:shiftwidth=2:tabstop=2:smarttab:
 *
 *  Definitions required for TableFunction plugin
 *
 *  Copyright (C) 2010 Sun Microsystems, Inc.
 *  Copyright (C) 2010 Monty Taylor
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

#pragma once

#include <drizzled/definitions.h>
#include <drizzled/plugin.h>
#include <drizzled/plugin/plugin.h>
#include <drizzled/identifier.h>
#include <drizzled/message/table.pb.h>
#include <drizzled/charset.h>
#include <drizzled/field.h>

#include <string>
#include <set>
#include <algorithm>

#include <drizzled/visibility.h>

namespace drizzled {
namespace plugin {

#define TABLE_FUNCTION_BLOB_SIZE 2049

// Not thread safe, but plugins are just loaded in a single thread right
// now.
static const char *local_string_append(const char *arg1, const char *arg2)
{
  static char buffer[1024];
  char *buffer_ptr= buffer;
  strcpy(buffer_ptr, arg1);
  buffer_ptr+= strlen(arg1);
  buffer_ptr[0]= '-';
  buffer_ptr++;
  strcpy(buffer_ptr, arg2);

  return buffer;
}

class DRIZZLED_API TableFunction : public Plugin
{
  TableFunction();
  TableFunction(const TableFunction &);
  TableFunction& operator=(const TableFunction &);

  message::Table proto;
  identifier::Table identifier;
  std::string local_path;
  std::string original_table_label;

  void setName(); // init name

  void init();


public:
  TableFunction(const char *schema_arg, const char *table_arg) :
    Plugin(local_string_append(schema_arg, table_arg) , "TableFunction"),
    identifier(schema_arg, table_arg),
    original_table_label(table_arg)
  {
    init();
  }

  virtual ~TableFunction() {}

  static bool addPlugin(TableFunction *function);
  static void removePlugin(TableFunction *)
  { }
  static TableFunction *getFunction(const std::string &arg);
  static void getNames(const std::string &arg,
                       std::set<std::string> &set_of_names);

  enum ColumnType {
    BOOLEAN,
    NUMBER,
    STRING,
    VARBINARY,
    SIZE
  };

  class Generator
  {
    Field **columns;
    Field **columns_iterator;
    Session *session;

  protected:
  	LEX& lex();
  	statement::Statement& statement();

    drizzled::Session &getSession()
    {
      return *session;
    }

  public:
    const charset_info_st *scs;

    Generator(Field **arg);
    virtual ~Generator()
    { }

    /*
      Return type is bool meaning "are there more rows".
    */
    bool sub_populate(uint32_t field_size);

    virtual bool populate()
    {
      return false;
    }

    void push(uint64_t arg);
    void push(int64_t arg);
    void push(const char *arg, uint32_t length= 0);
    void push(const std::string& arg);
    void push(bool arg);
    void push();

    bool isWild(const std::string &predicate);
  };

  void define(message::Table &arg)
  {
    arg.CopyFrom(proto);
  }

  const std::string &getTableLabel()
  {
    return original_table_label;
  }

  const std::string &getIdentifierTableName()
  {
    return identifier.getTableName();
  }

  const std::string &getSchemaHome()
  {
    return identifier.getSchemaName();
  }

  const std::string &getPath()
  {
    return identifier.getPath();
  }

  virtual Generator *generator(Field **arg);

  void add_field(const char *label,
                 message::Table::Field::FieldType type,
                 uint32_t length= 0);

  void add_field(const char *label,
                 uint32_t field_length= MAXIMUM_IDENTIFIER_LENGTH);

  void add_field(const char *label,
                 TableFunction::ColumnType type,
                 bool is_default_null= true);

  void add_field(const char *label,
                 TableFunction::ColumnType type,
                 uint32_t field_length,
                 bool is_default_null= false);

  virtual bool visible() const { return true; }
};

} /* namespace plugin */
} /* namespace drizzled */

