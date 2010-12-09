/* - mode: c; c-basic-offset: 2; indent-tabs-mode: nil; -*-
 *  vim:expandtab:shiftwidth=2:tabstop=2:smarttab:
 *
 *  Copyright (C) 2010 Brian Aker
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


#include "config.h"
#include "drizzled/message/table.h"

namespace drizzled {
namespace message {
namespace table {

shared_ptr make_shared(const std::string &name_arg, const std::string &schema_arg, const std::string &engine_arg)
{
  shared_ptr shared(new message::Table);

  shared->set_name(name_arg);
  shared->set_schema(schema_arg);
  shared->set_creation_timestamp(time(NULL));
  shared->set_update_timestamp(time(NULL));
  shared->mutable_engine()->set_name(engine_arg);

  /* 36 characters for uuid string +1 for NULL */
  uuid_t uu;
  char uuid_string[37];
  uuid_generate_random(uu);
  uuid_unparse(uu, uuid_string);
  shared->set_uuid(uuid_string, 36);

  shared->set_version(1);

  return shared;
}

void init(drizzled::message::Table &arg, const std::string &name_arg, const std::string &schema_arg, const std::string &engine_arg)
{
  arg.set_name(name_arg);
  arg.set_schema(schema_arg);
  arg.set_creation_timestamp(time(NULL));
  arg.set_update_timestamp(time(NULL));
  arg.mutable_engine()->set_name(engine_arg);

  /* 36 characters for uuid string +1 for NULL */
  uuid_t uu;
  char uuid_string[37];
  uuid_generate_random(uu);
  uuid_unparse(uu, uuid_string);
  arg.set_uuid(uuid_string, 36);

  arg.set_version(1);
}

void update(drizzled::message::Table &arg)
{
  arg.set_version(arg.version() + 1);
  arg.set_update_timestamp(time(NULL));
}

} // namespace table
} // namespace message
} // namespace drizzled
