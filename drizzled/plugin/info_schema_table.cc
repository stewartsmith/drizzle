/* -*- mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; -*-
 *  vim:expandtab:shiftwidth=2:tabstop=2:smarttab:
 *
 *  Copyright (C) 2008 Sun Microsystems
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
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

#include "drizzled/server_includes.h"
#include "drizzled/plugin/info_schema_table.h"
#include "drizzled/gettext.h"
#include "drizzled/session.h"
#include "drizzled/lex_string.h"

#include <vector>

using namespace std;

namespace drizzled
{

vector<plugin::InfoSchemaTable *> all_schema_tables;


bool plugin::InfoSchemaTable::addPlugin(plugin::InfoSchemaTable *schema_table)
{
  if (schema_table->getFirstColumnIndex() == 0)
    schema_table->setFirstColumnIndex(-1);
  if (schema_table->getSecondColumnIndex() == 0)
   schema_table->setSecondColumnIndex(-1);

  all_schema_tables.push_back(schema_table);
  return false;
}

void plugin::InfoSchemaTable::removePlugin(plugin::InfoSchemaTable *table)
{
  all_schema_tables.erase(remove_if(all_schema_tables.begin(),
                                    all_schema_tables.end(),
                                    bind2nd(equal_to<plugin::InfoSchemaTable *>(),
                                            table)),
                          all_schema_tables.end());
}


class AddSchemaTable : public unary_function<plugin::InfoSchemaTable *, bool>
{
  Session *session;
  const char *wild;
  vector<LEX_STRING*> &files;

public:
  AddSchemaTable(Session *session_arg, vector<LEX_STRING*> &files_arg, const char *wild_arg)
    : session(session_arg), wild(wild_arg), files(files_arg)
  {}

  result_type operator() (argument_type schema_table)
  {
    if (schema_table->isHidden())
    {
      return false;
    }

    const string &schema_table_name= schema_table->getTableName();

    if (wild && wild_case_compare(files_charset_info, schema_table_name.c_str(), wild))
    {
      return false;
    }

    LEX_STRING *file_name= 0;
    file_name= session->make_lex_string(file_name, schema_table_name.c_str(),
                                        schema_table_name.length(), true);
    if (file_name == NULL)
    {
      return true;
    }

    files.push_back(file_name);
    return false;
  }
};

class FindSchemaTableByName : public unary_function<plugin::InfoSchemaTable *, bool>
{
  const char *table_name;
public:
  FindSchemaTableByName(const char *table_name_arg)
    : table_name(table_name_arg) {}
  result_type operator() (argument_type schema_table)
  {
    return ! my_strcasecmp(system_charset_info,
                           schema_table->getTableName().c_str(),
                           table_name);
  }
};

plugin::InfoSchemaTable *plugin::InfoSchemaTable::getTable(const char *table_name)
{
  vector<plugin::InfoSchemaTable *>::iterator iter=
    find_if(all_schema_tables.begin(),
            all_schema_tables.end(),
            FindSchemaTableByName(table_name));

  if (iter != all_schema_tables.end())
  {
    return *iter;
  }

  return NULL;

}

void plugin::InfoSchemaTable::getTableNames(set<string>& tables_names)
{
  for (vector<plugin::InfoSchemaTable *>::iterator iter= all_schema_tables.begin();
       iter != all_schema_tables.end();
       iter++)
  {
    if ((*iter)->isHidden() == false)
      tables_names.insert((*iter)->getTableName().c_str());
  }
}

int plugin::InfoSchemaTable::addTableToList(Session *session,
                                     vector<LEX_STRING*> &files,
                                     const char *wild)
{

  vector<plugin::InfoSchemaTable *>::iterator iter=
    find_if(all_schema_tables.begin(),
            all_schema_tables.end(),
            AddSchemaTable(session, files, wild));

  if (iter != all_schema_tables.end())
  {
    return 1;
  }

  return 0;
}

} /* namespace drizzled */
