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


#include <config.h>

#include <uuid/uuid.h>

#include <drizzled/show.h>
#include <drizzled/message/schema.h>
#include <drizzled/session.h>
#include <drizzled/charset.h>

#include <drizzled/catalog/local.h>

namespace drizzled {
namespace message {
namespace schema {

shared_ptr make_shared(const identifier::Schema& identifier)
{
  shared_ptr shared(new message::Schema);

  init(*shared, identifier.getSchemaName());

  return shared;
}

shared_ptr make_shared(const std::string &name_arg)
{
  shared_ptr shared(new message::Schema);

  init(*shared, name_arg);

  return shared;
}

void init(drizzled::message::Schema &arg, const std::string &name_arg)
{
  arg.set_name(name_arg);
  arg.set_catalog(drizzled::catalog::local()->name());
  arg.mutable_engine()->set_name(std::string("filesystem")); // For the moment we have only one.
  arg.set_creation_timestamp(time(NULL));
  arg.set_update_timestamp(time(NULL));
  if (not arg.has_collation())
  {
    arg.set_collation(default_charset_info->name);
  }

  /* 36 characters for uuid string +1 for NULL */
  uuid_t uu;
  char uuid_string[37];
  uuid_generate_random(uu);
  uuid_unparse(uu, uuid_string);
  arg.set_uuid(uuid_string, 36);

  arg.set_version(1);
}

} // namespace schema
} // namespace message
} // namespace drizzled
