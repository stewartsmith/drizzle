/* -*- mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; -*-
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

#ifndef DRIZZLED_STATEMENT_CREATE_SCHEMA_H
#define DRIZZLED_STATEMENT_CREATE_SCHEMA_H

#include <drizzled/statement.h>
#include <drizzled/message/schema.pb.h>
#include <uuid/uuid.h>

namespace drizzled
{
class Session;

namespace statement
{

class CreateSchema : public Statement
{
public:
  CreateSchema(Session *in_session)
    :
      Statement(in_session),
      is_if_not_exists(false)
  {
  }

  bool execute();
  bool is_if_not_exists;
  message::Schema schema_message;

  bool validateSchemaOptions();
};

} /* end namespace statement */

} /* end namespace drizzled */

#endif /* DRIZZLED_STATEMENT_CREATE_SCHEMA_H */
