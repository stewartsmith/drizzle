/* - mode: c; c-basic-offset: 2; indent-tabs-mode: nil; -*-
 *  vim:expandtab:shiftwidth=2:tabstop=2:smarttab:
 *
 *  Copyright (C) 2011 Brian Aker
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

#include <drizzled/statement/catalog.h>
#include <drizzled/plugin/client.h>
#include <drizzled/session.h>
#include <drizzled/plugin/catalog.h>

namespace drizzled {
namespace statement {
namespace catalog {

Create::Create(Session *in_session, drizzled::lex_string_t &arg) :
  Catalog(in_session, arg)
{
}

bool Create::authorized() const
{
  if (session().getClient()->isConsole())
  {
    return true;
  }

  my_error(ER_CATALOG_CANNOT_CREATE_PERMISSION, identifier());

  return false;
}

bool Create::perform() const
{
  return plugin::Catalog::create(identifier());
}

} /* namespace catalog */
} /* namespace statement */
} /* namespace drizzled */
