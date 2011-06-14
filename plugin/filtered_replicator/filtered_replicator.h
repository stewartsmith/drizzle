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
 * Defines the API of a simple filtered replicator.
 *
 * @see drizzled/plugin/transaction_replicator.h
 * @see drizzled/plugin/transaction_applier.h
 */

#pragma once

#include <drizzled/atomics.h>
#include <drizzled/plugin/transaction_replicator.h>

#include PCRE_HEADER

#include <vector>
#include <string>

namespace drizzle_plugin
{

class FilteredReplicator :
  public drizzled::plugin::TransactionReplicator
{
public:
  FilteredReplicator(std::string name_arg,
                     const std::string &sch_filter,
                     const std::string &tab_filter,
                     const std::string &sch_regex,
                     const std::string &tab_regex);

  /** Destructor */
  ~FilteredReplicator();

  /**
   * Replicate a Transaction message to an Applier.
   *
   * @note
   *
   * It is important to note that memory allocation for the 
   * supplied pointer is not guaranteed after the completion 
   * of this function -- meaning the caller can dispose of the
   * supplied message.  Therefore, replicators and appliers 
   * implementing an asynchronous replication system must copy
   * the supplied message to their own controlled memory storage
   * area.
   *
   * @param Applier to replicate to
   * @param Session descriptor
   * @param Transaction message to be replicated
   */
  drizzled::plugin::ReplicationReturnCode
  replicate(drizzled::plugin::TransactionApplier *in_applier,
            drizzled::Session &in_session,
            drizzled::message::Transaction &to_replicate);
  
  /**
   * Populate the vector of schemas to filter from the
   * comma-separated list of schemas given. This method
   * clears the vector first.
   *
   * @param[in] input comma-separated filter to use
   */
  void setSchemaFilter(const std::string &input);

  /**
   * @return string of comma-separated list of schemas to filter
   */
  const std::string &getSchemaFilter() const
  {
    return _sch_filter;
  }

  /**
   * Populate the vector of tables to filter from the
   * comma-separated list of tables given. This method
   * clears the vector first.
   *
   * @param[in] input comma-separated filter to use
   */
  void setTableFilter(const std::string &input);

  /**
   * @return string of comma-separated list of tables to filter
   */
  const std::string &getTableFilter() const
  {
    return _tab_filter;
  }

  /**
   * Update the given system variable and release the mutex
   * associated with this system variable.
   *
   * @param[out] var_ptr the system variable to update
   */
  void updateTableSysvar(const char **var_ptr)
  {
    *var_ptr= _tab_filter.c_str();
    pthread_mutex_unlock(&sysvar_tab_lock);
  }

  /**
   * Update the given system variable and release the mutex
   * associated with this system variable.
   *
   * @param[out] var_ptr the system variable to update
   */
  void updateSchemaSysvar(const char **var_ptr)
  {
    *var_ptr= _sch_filter.c_str();
    pthread_mutex_unlock(&sysvar_sch_lock);
  }

private:
 
  /**
   * Given a comma-separated string, parse that string to obtain
   * each entry and add each entry to the supplied vector.
   *
   * @param[in] input a comma-separated string of entries
   * @param[out] filter a std::vector to be populated with the entries
   *                    from the input string
   */
  void populateFilter(std::string input,
                      std::vector<std::string> &filter);

  /**
   * Search the vector of schemas to filter to determine whether
   * the given schema should be filtered or not. The parameter
   * is obtained from the Transaction message passed to the replicator.
   *
   * @param[in] schema_name name of schema to search for
   * @return true if the given schema should be filtered; false otherwise
   */
  bool isSchemaFiltered(const std::string &schema_name);

  /**
   * Search the vector of tables to filter to determine whether
   * the given table should be filtered or not. The parameter
   * is obtained from the Transaction message passed to the replicator.
   *
   * @param[in] table_name name of table to search for
   * @return true if the given table should be filtered; false otherwise
   */
  bool isTableFiltered(const std::string &table_name);
  
  /**
   * Given a supplied Statement message, parse out the table
   * and schema name from the various metadata and header 
   * pieces for the different Statement types.
   *
   * @param[in] Statement to parse
   * @param[out] Schema name to fill
   * @param[out] Table name to fill
   */
  void parseStatementTableMetadata(const drizzled::message::Statement &in_statement,
                                   std::string &in_schema_name,
                                   std::string &in_table_name) const;

  /**
   * If the command message consists of raw SQL, this method parses
   * a string representation of the raw SQL and extracts the schema
   * name and table name from that raw SQL.
   *
   * @param[in] sql std::string representation of the raw SQL
   * @param[out] schema_name parameter to be populated with the 
   *                         schema name from the parsed SQL
   * @param[out] table_name parameter to be populated with the table
   *                        name from the parsed SQL
   */
  void parseQuery(const std::string &sql,
                  std::string &schema_name,
                  std::string &table_name);

  /*
   * Vectors of the tables and schemas to filter.
   */
  std::vector<std::string> schemas_to_filter;
  std::vector<std::string> tables_to_filter;

  /*
   * Variables to contain the string representation of the
   * comma-separated lists of schemas and tables to filter.
   */
  std::string _sch_filter;
  std::string _tab_filter;

  const std::string _sch_regex;
  const std::string _tab_regex;

  /*
   * We need locks to protect the vectors when they are
   * being updated and accessed. It would be nice to use
   * r/w locks here since the vectors will mostly be 
   * accessed in a read-only fashion and will be only updated
   * rarely.
   */
  pthread_mutex_t sch_vector_lock;
  pthread_mutex_t tab_vector_lock;

  /*
   * We need a lock to protect the system variables
   * that can be updated. We have a lock for each 
   * system variable.
   */
  pthread_mutex_t sysvar_sch_lock;
  pthread_mutex_t sysvar_tab_lock;

  pcre *sch_re;
  pcre *tab_re;
};

} /* namespace drizzle_plugin */

