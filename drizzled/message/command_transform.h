/* -*- mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; -*-
 *  vim:expandtab:shiftwidth=2:tabstop=2:smarttab:
 *
 *  Copyright (C) 2009 Sun Microsystems
 *
 *  Authors:
 *
 *    Jay Pipes <joinfu@sun.com>
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

/**
 * @file
 *
 * Declarations of various routines that can be used to convert
 * Command messages to other formats, including SQL statements.
 */

#ifndef DRIZZLED_MESSAGE_COMMAND_TRANSFORM_H
#define DRIZZLED_MESSAGE_COMMAND_TRANSFORM_H

#include <string>
#include <bitset>

namespace drizzled
{
namespace message
{
/* some forward declarations */
class Command;

enum CommandTransformSqlVariant
{
  ANSI,
  MYSQL_4,
  MYSQL_5,
  DRIZZLE
};

/**
 * This function looks at the Command
 * message and appends a correctly-formatted SQL
 * statement to the supplied destination string.
 *
 * @param Command message to transform
 * @param Destination string to append SQL to
 */
void transformCommand2Sql(const Command &source,
                          std::string *destination,
                          enum CommandTransformSqlVariant sql_variant= DRIZZLE);

} /* end namespace drizzled::message */
} /* end namespace drizzled */

#endif /* DRIZZLED_MESSAGE_COMMAND_TRANSFORM_H */
