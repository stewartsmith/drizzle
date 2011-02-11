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

#ifndef DRIZZLED_CATALOG_ENGINE_H
#define DRIZZLED_CATALOG_ENGINE_H

#include <boost/shared_ptr.hpp>
#include "drizzled/identifier/catalog.h"
#include "drizzled/message/catalog.h"

namespace drizzled {
namespace plugin {

class Catalog;

} // namespace plugin
namespace catalog {

class Engine
{
public:
  typedef boost::shared_ptr<Engine> shared_ptr;
  typedef std::vector<shared_ptr> vector;

protected:
  friend class drizzled::plugin::Catalog;

  Engine()
  { };

  virtual ~Engine()
  { };

  // DDL
  virtual bool create(identifier::Catalog::const_reference , message::catalog::shared_ptr &)= 0;
  virtual bool drop(identifier::Catalog::const_reference)= 0;

  // Get Meta information
  virtual bool exist(identifier::Catalog::const_reference identifier)= 0;
  virtual void getIdentifiers(identifier::Catalog::vector &identifiers)= 0;
  virtual message::catalog::shared_ptr getMessage(identifier::Catalog::const_reference)= 0;
  virtual void getMessages(message::catalog::vector &messages)= 0;
};

} /* namespace catalog */
} /* namespace drizzled */

#endif /* DRIZZLED_CATALOG_ENGINE_H */
