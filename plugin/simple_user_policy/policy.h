/* -*- mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; -*-
 *  vim:expandtab:shiftwidth=2:tabstop=2:smarttab:
 *
 *  Copyright (C) 2010 Monty Taylor <mordred@inaugust.com>
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


#ifndef PLUGIN_SIMPLE_USER_POLICY_POLICY_H
#define PLUGIN_SIMPLE_USER_POLICY_POLICY_H

#include <iostream>

#include <drizzled/plugin/authorization.h>

namespace simple_user_policy
{

class Policy :
  public drizzled::plugin::Authorization
{
public:
  Policy() :
    drizzled::plugin::Authorization("Simple User Policy")
  { }

  virtual bool restrictSchema(const drizzled::SecurityContext &user_ctx,
                              drizzled::SchemaIdentifier::const_reference schema);

  virtual bool restrictProcess(const drizzled::SecurityContext &user_ctx,
                               const drizzled::SecurityContext &session_ctx);
};

inline bool Policy::restrictSchema(const drizzled::SecurityContext &user_ctx,
                                   drizzled::SchemaIdentifier::const_reference schema)
{
  if ((user_ctx.getUser() == "root")
      || schema.compare("data_dictionary")
      || schema.compare("information_schema"))
    return false;
  return not schema.compare(user_ctx.getUser());
}

inline bool Policy::restrictProcess(const drizzled::SecurityContext &user_ctx,
                                    const drizzled::SecurityContext &session_ctx)
{
  if (user_ctx.getUser() == "root")
    return false;
  return user_ctx.getUser() != session_ctx.getUser();
}

} /* namespace simple_user_policy */

#endif /* PLUGIN_SIMPLE_USER_POLICY_POLICY_H */
