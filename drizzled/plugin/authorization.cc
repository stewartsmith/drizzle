/* -*- mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; -*-
 *  vim:expandtab:shiftwidth=2:tabstop=2:smarttab:
 *
 *  Copyright (C) 2010 Monty Taylor
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

#include <config.h>

#include <vector>

#include <drizzled/plugin/authorization.h>
#include <drizzled/identifier.h>
#include <drizzled/error.h>
#include <drizzled/session.h>
#include <drizzled/gettext.h>

namespace drizzled
{

std::vector<plugin::Authorization *> authorization_plugins;


bool plugin::Authorization::addPlugin(plugin::Authorization *auth)
{
  if (auth != NULL)
    authorization_plugins.push_back(auth);

  return false;
}

void plugin::Authorization::removePlugin(plugin::Authorization *auth)
{
  if (auth != NULL)
  {
    authorization_plugins.erase(std::find(authorization_plugins.begin(),
                                          authorization_plugins.end(),
                                          auth));
  }
}

namespace
{

class RestrictDbFunctor :
  public std::unary_function<plugin::Authorization *, bool>
{
  const identifier::User &user_ctx;
  const identifier::Schema& schema;

public:
  RestrictDbFunctor(const identifier::User &user_ctx_arg,
                    const identifier::Schema& schema_arg) :
    std::unary_function<plugin::Authorization *, bool>(),
    user_ctx(user_ctx_arg),
    schema(schema_arg)
  { }

  inline result_type operator()(argument_type auth)
  {
    return auth->restrictSchema(user_ctx, schema);
  }
};

class RestrictTableFunctor :
  public std::unary_function<plugin::Authorization *, bool>
{
  const identifier::User& user_ctx;
  const identifier::Table& table;
public:
  RestrictTableFunctor(const identifier::User& user_ctx_arg,
                       const identifier::Table& table_arg) :
    std::unary_function<plugin::Authorization *, bool>(),
    user_ctx(user_ctx_arg),
    table(table_arg)
  { }

  inline result_type operator()(argument_type auth)
  {
    return auth->restrictTable(user_ctx, table);
  }
};

class RestrictProcessFunctor :
  public std::unary_function<plugin::Authorization *, bool>
{
  const identifier::User &user_ctx;
  const identifier::User &session_ctx;
public:
  RestrictProcessFunctor(const identifier::User &user_ctx_arg,
                         const identifier::User &session_ctx_arg) :
    std::unary_function<plugin::Authorization *, bool>(),
    user_ctx(user_ctx_arg),
    session_ctx(session_ctx_arg)
  { }

  inline result_type operator()(argument_type auth)
  {
    return auth->restrictProcess(user_ctx, session_ctx);
  }
};

class PruneSchemaFunctor :
  public std::unary_function<identifier::Schema&, bool>
{
  const drizzled::identifier::User& user_ctx;
public:
  PruneSchemaFunctor(const drizzled::identifier::User& user_ctx_arg) :
    std::unary_function<identifier::Schema&, bool>(),
    user_ctx(user_ctx_arg)
  { }

  inline result_type operator()(argument_type auth)
  {
    return not plugin::Authorization::isAuthorized(user_ctx, auth, false);
  }
};

} /* namespace */

bool plugin::Authorization::isAuthorized(const identifier::User& user_ctx,
                                         const identifier::Schema& schema_identifier,
                                         bool send_error)
{
  /* If we never loaded any authorization plugins, just return true */
  if (authorization_plugins.empty())
    return true;

  /* Use find_if instead of foreach so that we can collect return codes */
  std::vector<plugin::Authorization *>::const_iterator iter=
    std::find_if(authorization_plugins.begin(),
                 authorization_plugins.end(),
                 RestrictDbFunctor(user_ctx, schema_identifier));


  /*
   * If iter is == end() here, that means that all of the plugins returned
   * false, which means that that each of them believe the user is authorized
   * to view the resource in question.
   */
  if (iter != authorization_plugins.end())
  {
    if (send_error)
    {
      error::access(user_ctx, schema_identifier);
    }
    return false;
  }
  return true;
}

bool plugin::Authorization::isAuthorized(const drizzled::identifier::User& user_ctx,
                                         const identifier::Table& table_identifier,
                                         bool send_error)
{
  /* If we never loaded any authorization plugins, just return true */
  if (authorization_plugins.empty())
    return true;

  /* Use find_if instead of foreach so that we can collect return codes */
  std::vector<plugin::Authorization *>::const_iterator iter=
    std::find_if(authorization_plugins.begin(),
            authorization_plugins.end(),
            RestrictTableFunctor(user_ctx, table_identifier));

  /*
   * If iter is == end() here, that means that all of the plugins returned
   * false, which means that that each of them believe the user is authorized
   * to view the resource in question.
   */
  if (iter != authorization_plugins.end())
  {
    if (send_error)
    {
      error::access(user_ctx, table_identifier);
    }
    return false;
  }
  return true;
}

bool plugin::Authorization::isAuthorized(const drizzled::identifier::User& user_ctx,
                                         const Session& session,
                                         bool send_error)
{
  /* If we never loaded any authorization plugins, just return true */
  if (authorization_plugins.empty())
    return true;
  
  // To make sure we hold the user structure we need to have a shred_ptr so
  // that we increase the count on the object.
  drizzled::identifier::user::ptr session_ctx= session.user();


  /* Use find_if instead of foreach so that we can collect return codes */
  std::vector<plugin::Authorization *>::const_iterator iter=
    std::find_if(authorization_plugins.begin(),
                 authorization_plugins.end(),
                 RestrictProcessFunctor(user_ctx, *session_ctx));

  /*
   * If iter is == end() here, that means that all of the plugins returned
   * false, which means that that each of them believe the user is authorized
   * to view the resource in question.
   */

  if (iter != authorization_plugins.end())
  {
    if (send_error)
    {
      my_error(ER_KILL_DENIED_ERROR, MYF(0), session.thread_id);
    }
    return false;
  }

  return true;
}

void plugin::Authorization::pruneSchemaNames(const drizzled::identifier::User& user_ctx,
                                             identifier::schema::vector &set_of_schemas)
{
  /* If we never loaded any authorization plugins, just return true */
  if (authorization_plugins.empty())
    return;

  set_of_schemas.erase(std::remove_if(set_of_schemas.begin(),
                                      set_of_schemas.end(),
                                      PruneSchemaFunctor(user_ctx)),
                       set_of_schemas.end());
}

} /* namespace drizzled */
