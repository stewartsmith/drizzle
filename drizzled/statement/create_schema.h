/* -*- mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; -*-
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

#pragma once

#include <drizzled/session.h>
#include <drizzled/statement.h>
#include <drizzled/message/schema.pb.h>
#include <uuid/uuid.h>

namespace drizzled {
namespace statement {

class CreateSchema : public Statement
{
public:
  CreateSchema(Session *in_session) :
    Statement(in_session),
    is_if_not_exists(false)
  {
    set_command(SQLCOM_CREATE_DB);
  }

  bool execute();
  bool is_if_not_exists;
  message::Schema schema_message;

  bool validateSchemaOptions();
private:
  bool check(const identifier::Schema &identifier);
};

} /* end namespace statement */
} /* end namespace drizzled */

