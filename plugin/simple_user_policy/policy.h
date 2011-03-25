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


#pragma once

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

  virtual bool restrictSchema(const drizzled::identifier::User &user_ctx,
                              const drizzled::identifier::Schema& schema);

  virtual bool restrictProcess(const drizzled::identifier::User &user_ctx,
                               const drizzled::identifier::User &session_ctx);
};

inline bool Policy::restrictSchema(const drizzled::identifier::User &user_ctx,
                                   const drizzled::identifier::Schema& schema)
{
  if ((user_ctx.username() == "root")
      || schema.compare("data_dictionary")
      || schema.compare("information_schema"))
  {
    return false;
  }

  return not schema.compare(user_ctx.username());
}

inline bool Policy::restrictProcess(const drizzled::identifier::User &user_ctx,
                                    const drizzled::identifier::User &session_ctx)
{
  if (user_ctx.username() == "root")
    return false;

  return user_ctx.username() != session_ctx.username();
}

} /* namespace simple_user_policy */

