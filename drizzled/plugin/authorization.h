/* -*- mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; -*-
 *  vim:expandtab:shiftwidth=2:tabstop=2:smarttab:
 *
 *  Definitions required for Authorization plugin
 *
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

#include <drizzled/plugin.h>
#include <drizzled/plugin/plugin.h>
#include <drizzled/identifier.h>

#include <string>
#include <set>

#include <drizzled/visibility.h>

namespace drizzled
{

namespace plugin
{

class DRIZZLED_API Authorization : public Plugin
{
  Authorization();
  Authorization(const Authorization &);
  Authorization& operator=(const Authorization &);
public:
  explicit Authorization(std::string name_arg)
    : Plugin(name_arg, "Authorization")
  {}
  virtual ~Authorization() {}

  /**
   * Should we restrict the current user's access to this schema?
   *
   * @param Current security context
   * @param Database to check against
   *
   * @returns true if the user cannot access the schema
   */
  virtual bool restrictSchema(const drizzled::identifier::User &user_ctx,
                              const identifier::Schema& schema)= 0;

  /**
   * Should we restrict the current user's access to this table?
   *
   * @param Current security context
   * @param Database to check against
   * @param Table to check against
   *
   * @returns true if the user cannot access the table
   */
  virtual bool restrictTable(const drizzled::identifier::User& user_ctx,
                             const drizzled::identifier::Table& table);

  /**
   * Should we restrict the current user's access to see this process?
   *
   * @param Current security context
   * @param Database to check against
   * @param Table to check against
   *
   * @returns true if the user cannot see the process
   */
  virtual bool restrictProcess(const drizzled::identifier::User &user_ctx,
                               const drizzled::identifier::User &session_ctx);

  /** Server API method for checking schema authorization */
  static bool isAuthorized(const drizzled::identifier::User& user_ctx,
                           const identifier::Schema& schema_identifier,
                           bool send_error= true);

  /** Server API method for checking table authorization */
  static bool isAuthorized(const drizzled::identifier::User& user_ctx,
                           const drizzled::identifier::Table& table_identifier,
                           bool send_error= true);

  /** Server API method for checking process authorization */
  static bool isAuthorized(const drizzled::identifier::User& user_ctx,
                           const Session &session,
                           bool send_error= true);

  /**
   * Server API helper method for applying authorization tests
   * to a set of schema names (for use in the context of getSchemaNames
   */
  static void pruneSchemaNames(const drizzled::identifier::User& user_ctx,
                               identifier::schema::vector &set_of_schemas);
  
  /**
   * Standard plugin system registration hooks
   */
  static bool addPlugin(plugin::Authorization *auth);
  static void removePlugin(plugin::Authorization *auth);

};

inline bool Authorization::restrictTable(const drizzled::identifier::User& user_ctx,
                                         const drizzled::identifier::Table& table)
{
  return restrictSchema(user_ctx, table);
}

inline bool Authorization::restrictProcess(const drizzled::identifier::User &,
                                           const drizzled::identifier::User &)
{
  return false;
}

} /* namespace plugin */

} /* namespace drizzled */

