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

#include <drizzled/identifier.h>
#include <drizzled/message/catalog.h>
#include <uuid/uuid.h>

#include <boost/make_shared.hpp>

namespace drizzled {
namespace message {
namespace catalog {


shared_ptr make_shared(const identifier::Catalog &identifier)
{
  shared_ptr message= boost::make_shared< value_type>();
  assert(not identifier.getName().empty());
  message->set_name(identifier.getName());

  message->set_creation_timestamp(time(NULL));
  message->set_update_timestamp(time(NULL));
  message->mutable_engine()->set_name("default");

  /* 36 characters for uuid string +1 for NULL */
  uuid_t uu;
  char uuid_string[37];
  uuid_generate_random(uu);
  uuid_unparse(uu, uuid_string);
  message->set_uuid(uuid_string, 36);

  message->set_version(1);

  return message;
}

} /* namespace catalog */
} /* namespace message */
} /* namespace drizzled */
