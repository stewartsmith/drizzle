/* - mode: c; c-basic-offset: 2; indent-tabs-mode: nil; -*-
 *  vim:expandtab:shiftwidth=2:tabstop=2:smarttab:
 *
 *  Copyright (C) 2009 Sun Microsystems, Inc.
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

/**
 * @file
 *
 * Defines the implementation of a simple replicator that can filter
 * events based on a schema or table name.
 *
 * @details
 *
 * This is a very simple implementation.  All we do is maintain two
 * std::vectors:
 *
 *  1) contains all the schema names to filter
 *  2) contains all the table names to filter
 *
 * If an event is on a schema or table in the vectors described above, then
 * the event will not be passed along to the applier.
 */

#include <config.h>
#include <drizzled/gettext.h>
#include <drizzled/plugin/transaction_applier.h>
#include <drizzled/message/transaction.pb.h>
#include <drizzled/plugin.h>

#include <drizzled/item/string.h>
#include "filtered_replicator.h"
#include <boost/program_options.hpp>
#include <drizzled/module/option_map.h>
#include <vector>
#include <string>
namespace po= boost::program_options;
using namespace std;
using namespace drizzled;

namespace drizzle_plugin
{

static string sysvar_filtered_replicator_sch_filters;
static string sysvar_filtered_replicator_tab_filters;

FilteredReplicator::FilteredReplicator(string name_arg,
                                       const std::string &sch_filter,
                                       const std::string &tab_filter,
                                       const std::string &sch_regex,
                                       const std::string &tab_regex) :
  plugin::TransactionReplicator(name_arg),
  schemas_to_filter(),
  tables_to_filter(),
  _sch_filter(sch_filter),
  _tab_filter(tab_filter),
  _sch_regex(sch_regex),
  _tab_regex(tab_regex),
  sch_re(NULL),
  tab_re(NULL)
{
  /* 
   * Add each of the specified schemas to the vector of schemas
   * to filter.
   */
  if (not _sch_filter.empty())
  {
    populateFilter(_sch_filter, schemas_to_filter);
  }

  /* 
   * Add each of the specified tables to the vector of tables
   * to filter.
   */
  if (not _tab_filter.empty())
  {
    populateFilter(_tab_filter, tables_to_filter);
  }

  /* 
   * Compile the regular expression for schema's to filter
   * if one is specified.
   */
  if (not _sch_regex.empty())
  {
    const char *error= NULL;
    int32_t error_offset= 0;
    sch_re= pcre_compile(_sch_regex.c_str(),
                         0,
                         &error,
                         &error_offset,
                         NULL);
  }

  /* 
   * Compile the regular expression for table's to filter
   * if one is specified.
   */
  if (not _tab_regex.empty())
  {
    const char *error= NULL;
    int32_t error_offset= 0;
    tab_re= pcre_compile(_tab_regex.c_str(),
                         0,
                         &error,
                         &error_offset,
                         NULL);
  }

  pthread_mutex_init(&sch_vector_lock, NULL);
  pthread_mutex_init(&tab_vector_lock, NULL);
  pthread_mutex_init(&sysvar_sch_lock, NULL);
  pthread_mutex_init(&sysvar_tab_lock, NULL);
}

FilteredReplicator::~FilteredReplicator()
{
  if (sch_re)
  {
    pcre_free(sch_re);
  }
  if (tab_re)
  {
    pcre_free(tab_re);
  }

  pthread_mutex_destroy(&sch_vector_lock);
  pthread_mutex_destroy(&tab_vector_lock);
  pthread_mutex_destroy(&sysvar_sch_lock);
  pthread_mutex_destroy(&sysvar_tab_lock);

}

void FilteredReplicator::parseStatementTableMetadata(const message::Statement &in_statement,
                                                     string &in_schema_name,
                                                     string &in_table_name) const
{
  switch (in_statement.type())
  {
    case message::Statement::INSERT:
    {
      const message::TableMetadata &metadata= in_statement.insert_header().table_metadata();
      in_schema_name.assign(metadata.schema_name());
      in_table_name.assign(metadata.table_name());
      break;
    }
    case message::Statement::UPDATE:
    {
      const message::TableMetadata &metadata= in_statement.update_header().table_metadata();
      in_schema_name.assign(metadata.schema_name());
      in_table_name.assign(metadata.table_name());
      break;
    }
    case message::Statement::DELETE:
    {
      const message::TableMetadata &metadata= in_statement.delete_header().table_metadata();
      in_schema_name.assign(metadata.schema_name());
      in_table_name.assign(metadata.table_name());
      break;
    }
    case message::Statement::CREATE_SCHEMA:
    {
      in_schema_name.assign(in_statement.create_schema_statement().schema().name());
      in_table_name.clear();
      break;
    }
    case message::Statement::ALTER_SCHEMA:
    {
      in_schema_name.assign(in_statement.alter_schema_statement().after().name());
      in_table_name.clear();
      break;
    }
    case message::Statement::DROP_SCHEMA:
    {
      in_schema_name.assign(in_statement.drop_schema_statement().schema_name());
      in_table_name.clear();
      break;
    }
    case message::Statement::CREATE_TABLE:
    {
      in_schema_name.assign(in_statement.create_table_statement().table().schema());
      in_table_name.assign(in_statement.create_table_statement().table().name());
      break;
    }
    case message::Statement::ALTER_TABLE:
    {
      in_schema_name.assign(in_statement.alter_table_statement().after().schema());
      in_table_name.assign(in_statement.alter_table_statement().after().name());
      break;
    }
    case message::Statement::DROP_TABLE:
    {
      const message::TableMetadata &metadata= in_statement.drop_table_statement().table_metadata();
      in_schema_name.assign(metadata.schema_name());
      in_table_name.assign(metadata.table_name());
      break;
    }
    default:
    {
      /* All other types have no schema and table information */
      in_schema_name.clear();
      in_table_name.clear();
      break;
    }
  }  
}

plugin::ReplicationReturnCode
FilteredReplicator::replicate(plugin::TransactionApplier *in_applier,
                              Session &in_session,
                              message::Transaction &to_replicate)
{
  string schema_name;
  string table_name;

  size_t num_statements= to_replicate.statement_size();

  /* 
   * We build a new transaction message containing only Statement
   * messages that have not been filtered.
   *
   * @todo A more efficient method would be to rework the pointers
   * that the to_replicate.statement() vector contains and remove
   * the statement pointers that are filtered...
   */
  message::Transaction filtered_transaction;

  for (size_t x= 0; x < num_statements; ++x)
  {
    schema_name.clear();
    table_name.clear();

    const message::Statement &statement= to_replicate.statement(x);

    /*
     * First, we check to see if the command consists of raw SQL. If so,
     * we need to parse this SQL and determine whether to filter the event
     * based on the information we obtain from the parsed SQL.
     * If not raw SQL, check if this event should be filtered or not
     * based on the schema and table names in the command message.
     *
     * The schema and table names are stored in TableMetadata headers
     * for most types of Statement messages.
     */
    if (statement.type() == message::Statement::RAW_SQL)
    {
      parseQuery(statement.sql(), schema_name, table_name);
    }
    else
    {
      parseStatementTableMetadata(statement, schema_name, table_name);
    }

    /*
     * Convert the schema name and table name strings to lowercase so that it
     * does not matter what case the table or schema name was specified in. We
     * also keep all entries in the vectors of schemas and tables to filter in
     * lowercase.
     */
    std::transform(schema_name.begin(), schema_name.end(),
                  schema_name.begin(), ::tolower);
    std::transform(table_name.begin(), table_name.end(),
                  table_name.begin(), ::tolower);

    if (! isSchemaFiltered(schema_name) &&
        ! isTableFiltered(table_name))
    {
      message::Statement *s= filtered_transaction.add_statement();
      *s= statement; /* copy contruct */
    }
  }

  if (filtered_transaction.statement_size() > 0)
  {

    /*
     * We can now simply call the applier's apply() method, passing
     * along the supplied command.
     */
    message::TransactionContext *tc= filtered_transaction.mutable_transaction_context();
    *tc= to_replicate.transaction_context(); /* copy construct */
    return in_applier->apply(in_session, filtered_transaction);
  }
  return plugin::SUCCESS;
}

void FilteredReplicator::populateFilter(std::string input,
                                        std::vector<string> &filter)
{
  /*
   * Convert the input string to lowercase so that all entries in the vector
   * will be in lowercase.
   */
  std::transform(input.begin(), input.end(),
                 input.begin(), ::tolower);
  string::size_type last_pos= input.find_first_not_of(',', 0);
  string::size_type pos= input.find_first_of(',', last_pos);

  while (pos != string::npos || last_pos != string::npos)
  {
    filter.push_back(input.substr(last_pos, pos - last_pos));
    last_pos= input.find_first_not_of(',', pos);
    pos= input.find_first_of(',', last_pos);
  }
}

bool FilteredReplicator::isSchemaFiltered(const string &schema_name)
{
  pthread_mutex_lock(&sch_vector_lock);
  std::vector<string>::iterator it= find(schemas_to_filter.begin(), schemas_to_filter.end(), schema_name);
  if (it != schemas_to_filter.end())
  {
    pthread_mutex_unlock(&sch_vector_lock);
    return true;
  }
  pthread_mutex_unlock(&sch_vector_lock);

  /* 
   * If regular expression matching is enabled for schemas to filter, then
   * we check to see if this schema name matches the regular expression that
   * has been specified. 
   */
  if (not _sch_regex.empty())
  {
    int32_t result= pcre_exec(sch_re, NULL, schema_name.c_str(), schema_name.length(), 0, 0, NULL, 0);
    if (result >= 0)
      return true;
  }

  return false;
}

bool FilteredReplicator::isTableFiltered(const string &table_name)
{
  pthread_mutex_lock(&tab_vector_lock);
  std::vector<string>::iterator it= find(tables_to_filter.begin(), tables_to_filter.end(), table_name);
  if (it != tables_to_filter.end())
  {
    pthread_mutex_unlock(&tab_vector_lock);
    return true;
  }
  pthread_mutex_unlock(&tab_vector_lock);

  /* 
   * If regular expression matching is enabled for tables to filter, then
   * we check to see if this table name matches the regular expression that
   * has been specified. 
   */
  if (not _tab_regex.empty())
  {
    int32_t result= pcre_exec(tab_re, NULL, table_name.c_str(), table_name.length(), 0, 0, NULL, 0);
    if (result >= 0)
      return true;
  }

  return false;
}

void FilteredReplicator::parseQuery(const string &sql,
                                    string &schema_name,
                                    string &table_name)
{
  /*
   * Determine what type of SQL we are dealing with e.g. create table,
   * drop table, etc.
   */
  string::size_type pos= sql.find_first_of(' ', 0);
  string type= sql.substr(0, pos);

  /*
   * Convert the type string to uppercase here so that it doesn't
   * matter what case the user entered the statement in.
   */
  std::transform(type.begin(), type.end(),
                 type.begin(), ::toupper);

  if (type.compare("DROP") == 0)
  {
    /*
     * The schema and table name can be either the third word
     * or the fifth word in a DROP TABLE statement...so we extract
     * the third word from the SQL and see whether it is and IF or
     * not.
     */
    pos= sql.find_first_of(' ', 11);
    string cmp_str= sql.substr(11, pos - 11);
    string target_name("");
    if (cmp_str.compare("IF") == 0)
    {
      /* the name must be the fifth word */
      pos= sql.find_first_of(' ', 21);
      target_name.assign(sql.substr(21, pos - 21));
    }
    else
    {
      target_name.assign(cmp_str);
    }
    /*
     * Determine whether the name is a concatenation of the schema
     * name and table name i.e. schema.table or just the table name
     * on its own.
     */
    pos= target_name.find_first_of('.', 0);
    if (pos != string::npos)
    {
      /*
       * There is a schema name here...
       */
      schema_name.assign(target_name.substr(0, pos));
      /*
       * The rest of the name string is the table name.
       */
      table_name.assign(target_name.substr(pos + 1));
    }
    else
    {
      table_name.assign(target_name);
    }
  }
  else if (type.compare("CREATE") == 0)
  {
    /*
     * The schema and table name are always the third word
     * in a CREATE TABLE statement...always (unless there is
     * some crazy syntax I am unaware of).
     */
    pos= sql.find_first_of(' ', 13);
    string target_name= sql.substr(13, pos - 13);
    /*
     * Determine whether the name is a concatenation of the schema
     * name and table name i.e. schema.table or just the table name
     * on its own.
     */
    pos= target_name.find_first_of('.', 0);
    if (pos != string::npos)
    {
      /*
       * There is a schema name here...
       */
      schema_name.assign(target_name.substr(0, pos));
      /*
       * The rest of the name string is the table name.
       */
      table_name.assign(target_name.substr(pos + 1));
    }
    else
    {
      table_name.assign(target_name);
    }
  }
  else
  {
    /* we only deal with DROP and CREATE table for the moment */
    return;
  }
}

void FilteredReplicator::setSchemaFilter(const string &input)
{
  pthread_mutex_lock(&sch_vector_lock);
  pthread_mutex_lock(&sysvar_sch_lock);
  _sch_filter.assign(input);
  schemas_to_filter.clear();
  populateFilter(_sch_filter, schemas_to_filter);
  pthread_mutex_unlock(&sysvar_sch_lock);
  pthread_mutex_unlock(&sch_vector_lock);
}

void FilteredReplicator::setTableFilter(const string &input)
{
  pthread_mutex_lock(&tab_vector_lock);
  pthread_mutex_lock(&sysvar_tab_lock);
  _tab_filter.assign(input);
  tables_to_filter.clear();
  populateFilter(_tab_filter, tables_to_filter);
  pthread_mutex_unlock(&sysvar_tab_lock);
  pthread_mutex_unlock(&tab_vector_lock);
}

static FilteredReplicator *filtered_replicator= NULL; /* The singleton replicator */

static int filtered_schemas_validate(Session*, set_var *var)
{
  const char *input= var->value->str_value.ptr();
  if (input == NULL)
    return 1;

  if (input && filtered_replicator)
  {
    filtered_replicator->setSchemaFilter(input);
    return 0;
  }
  return 1;
}


static int filtered_tables_validate(Session*, set_var *var)
{
  const char *input= var->value->str_value.ptr();
  if (input == NULL)
    return 1;

  if (input && filtered_replicator)
  {
    filtered_replicator->setTableFilter(input);
    return 0;
  }
  return 1;
}


static int init(module::Context &context)
{
  const module::option_map &vm= context.getOptions();
  
  filtered_replicator= new FilteredReplicator("filtered_replicator",
                                              vm["filteredschemas"].as<string>(),
                                              vm["filteredtables"].as<string>(),
                                              vm["schemaregex"].as<string>(),
                                              vm["tableregex"].as<string>());

  context.add(filtered_replicator);
  context.registerVariable(new sys_var_std_string("filteredschemas",
                                                  sysvar_filtered_replicator_sch_filters,
                                                  filtered_schemas_validate));
  context.registerVariable(new sys_var_std_string("filteredtables",
                                                  sysvar_filtered_replicator_tab_filters,
                                                  filtered_tables_validate));

  context.registerVariable(new sys_var_const_string_val("schemaregex",
                                                        vm["schemaregex"].as<string>()));
  context.registerVariable(new sys_var_const_string_val("tableregex",
                                                        vm["tableregex"].as<string>()));

  return 0;
}

static void init_options(drizzled::module::option_context &context)
{
  context("filteredschemas",
          po::value<string>(&sysvar_filtered_replicator_sch_filters)->default_value(""),
          N_("Comma-separated list of schemas to exclude"));
  context("filteredtables",
          po::value<string>(&sysvar_filtered_replicator_tab_filters)->default_value(""),
          N_("Comma-separated list of tables to exclude"));
  context("schemaregex", 
          po::value<string>()->default_value(""),
          N_("Regular expression to apply to schemas to exclude"));
  context("tableregex", 
          po::value<string>()->default_value(""),
          N_("Regular expression to apply to tables to exclude"));
}

} /* namespace drizzle_plugin */

DRIZZLE_PLUGIN(drizzle_plugin::init, NULL, drizzle_plugin::init_options);
