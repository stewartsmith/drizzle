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

#pragma once

#include <drizzled/catalog/engine.h>
#include <drizzled/catalog/local.h>

namespace plugin {
namespace catalog {

class Engine : public drizzled::catalog::Engine
{
public:
  bool create(const drizzled::identifier::Catalog &identifier, drizzled::message::catalog::shared_ptr &);
  bool drop(const drizzled::identifier::Catalog &identifier);

  bool exist(const drizzled::identifier::Catalog &identifier)
  {
    return drizzled::catalog::local_identifier() == identifier;
  }

  void getIdentifiers(drizzled::identifier::catalog::vector &identifiers)
  {
    identifiers.push_back(drizzled::catalog::local_identifier());
  }

  drizzled::message::catalog::shared_ptr getMessage(const drizzled::identifier::Catalog& identifier);

  void getMessages(drizzled::message::catalog::vector &messages);

private:
  drizzled::message::catalog::shared_ptr readFile(const drizzled::identifier::Catalog& identifier);
  bool writeFile(const drizzled::identifier::Catalog &identifier, drizzled::message::catalog::shared_ptr &message);
  void prime(drizzled::message::catalog::vector &messages);

};

} /* namespace catalog */
} /* namespace plugin */

