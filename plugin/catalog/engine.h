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

#include <boost/make_shared.hpp>
#include <drizzled/catalog/engine.h>

namespace plugin {
namespace catalog {

class Engine : public drizzled::catalog::Engine
{

public:
  Engine() :
    drizzled::catalog::Engine()
  {}

  bool create(const drizzled::identifier::Catalog &identifier, drizzled::message::catalog::shared_ptr &);
  bool drop(const drizzled::identifier::Catalog &identifier);

  bool exist(const drizzled::identifier::Catalog &identifier)
  {
    if (drizzled::catalog::local_identifier() == identifier)
      return true;

    return false;
  }

  void getIdentifiers(drizzled::identifier::Catalog::vector &identifiers)
  {
    identifiers.push_back(drizzled::catalog::local_identifier());
  }

  drizzled::message::catalog::shared_ptr getMessage(drizzled::identifier::Catalog::const_reference identifier);

  void getMessages(drizzled::message::catalog::vector &messages);

private:
  drizzled::message::catalog::shared_ptr readFile(drizzled::identifier::Catalog::const_reference identifier);
  bool writeFile(const drizzled::identifier::Catalog &identifier, drizzled::message::catalog::shared_ptr &message);
  void prime(drizzled::message::catalog::vector &messages);

};

} /* namespace catalog */
} /* namespace plugin */

