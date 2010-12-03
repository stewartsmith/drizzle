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


#ifndef DRIZZLED_PLUGIN_CATALOG_H
#define DRIZZLED_PLUGIN_CATALOG_H

#include <drizzled/plugin/plugin.h>
#include <drizzled/identifier/catalog.h>
#include <drizzled/catalog/instance.h>
#include <drizzled/catalog/engine.h>

namespace drizzled
{
namespace plugin
{

class Catalog : public Plugin {
  /* Disable default constructors */
  Catalog();
  Catalog(const Catalog &);
  Catalog& operator=(const Catalog &);

public:
  typedef std::vector<Catalog *> vector;

  explicit Catalog(std::string name_arg) :
    Plugin(name_arg, "Catalog")
  {}
  virtual ~Catalog();

  virtual catalog::Engine::shared_ptr engine()= 0;

  static bool create(const identifier::Catalog &);
  static bool create(const identifier::Catalog &, message::catalog::shared_ptr &);
  static bool drop(const identifier::Catalog &);

  static bool lock(const identifier::Catalog &);
  static bool unlock(const identifier::Catalog &);

  // Required for plugin interface
  static bool addPlugin(plugin::Catalog *plugin);
  static void removePlugin(plugin::Catalog *plugin);

  // Get Meta information
  static bool exist(const identifier::Catalog &identifier);
  static void getIdentifiers(identifier::Catalog::vector &identifiers);
  static void getMessages(message::catalog::vector &messages);
  static bool getMessage(const identifier::Catalog &identifier, message::catalog::shared_ptr &message);

  // Get Instance
  static bool getInstance(const identifier::Catalog &identifier, catalog::Instance::shared_ptr &instance);
};

} /* namespace plugin */
} /* namespace drizzled */

#endif /* DRIZZLED_PLUGIN_CATALOG_H */
