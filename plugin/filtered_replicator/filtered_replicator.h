/* - mode: c; c-basic-offset: 2; indent-tabs-mode: nil; -*-
 *  vim:expandtab:shiftwidth=2:tabstop=2:smarttab:
 *
 *  Copyright (C) 2009 Sun Microsystems
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
 * @see drizzled/plugin/replicator.h
 * @see drizzled/plugin/applier.h
 */

#ifndef DRIZZLE_PLUGIN_FILTERED_REPLICATOR_H
#define DRIZZLE_PLUGIN_FILTERED_REPLICATOR_H

#include <drizzled/server_includes.h>
#include <drizzled/atomics.h>
#include <drizzled/plugin/replicator.h>
#include <drizzled/plugin/applier.h>

#include <vector>
#include <string>

class FilteredReplicator: public drizzled::plugin::Replicator
{
public:
  FilteredReplicator(const char *in_sch_filters,
                     const char *in_tab_filters);

  /** Destructor */
  ~FilteredReplicator() {}

  /**
   * Replicate a Command message to an Applier.
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
   * @param Command message to be replicated
   */
  void replicate(drizzled::plugin::Applier *in_applier, drizzled::message::Command *to_replicate);
  
  /** 
   * Returns whether the replicator is active.
   */
  bool isActive();

  /**
   * Populate the vector of schemas to filter from the
   * comma-separated list of schemas given. This method
   * clears the vector first.
   *
   * @param[in] input comma-separated filter to use
   */
  void setSchemaFilter(const std::string &input);

  /**
   * Populate the vector of tables to filter from the
   * comma-separated list of tables given. This method
   * clears the vector first.
   *
   * @param[in] input comma-separated filter to use
   */
  void setTableFilter(const std::string &input);

private:
 
  /**
   * Given a comma-separated string, parse that string to obtain
   * each entry and add each entry to the supplied vector.
   *
   * @param[in] input a comma-separated string of entries
   * @param[out] filter a std::vector to be populated with the entries
   *                    from the input string
   */
  void populateFilter(const char *input,
                      std::vector<std::string> &filter);

  /**
   * Search the vector of schemas to filter to determine whether
   * the given schema should be filtered or not. The parameter
   * is obtained from the Command message passed to the replicator.
   *
   * @param[in] schema_name name of schema to search for
   * @return true if the given schema should be filtered; false otherwise
   */
  bool isSchemaFiltered(const std::string &schema_name);

  /**
   * Search the vector of tables to filter to determine whether
   * the given table should be filtered or not. The parameter
   * is obtained from the Command message passed to the replicator.
   *
   * @param[in] table_name name of table to search for
   * @return true if the given table should be filtered; false otherwise
   */
  bool isTableFiltered(const std::string &table_name);

  std::vector<std::string> schemas_to_filter;
  std::vector<std::string> tables_to_filter;
};

#endif /* DRIZZLE_PLUGIN_FILTERED_REPLICATOR_H */
