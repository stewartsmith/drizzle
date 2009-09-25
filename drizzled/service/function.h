/* -*- mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; -*-
 *  vim:expandtab:shiftwidth=2:tabstop=2:smarttab:
 *
 *  Copyright (C) 2008 Sun Microsystems
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

#ifndef DRIZZLED_SERVICE_FUNCTION_H
#define DRIZZLED_SERVICE_FUNCTION_H

/* This file defines structures needed by udf functions */

#include "drizzled/function/func.h"
#include "drizzled/plugin/function.h"
#include "drizzled/registry.h"

namespace drizzled
{
namespace service
{

/**
 * Class to handle all Function plugin objects.
 */
class Function
{
  Registry<const plugin::Function *> udf_registry;

public:
  Function() : udf_registry() {}
  ~Function() {}

  /**
   * Add a new Function factory to the list of factories we manage.
   */
  void add(plugin::Function *function_obj);

  /**
   * Remove a Function factory from the list of factory we manage.
   */
  void remove(const plugin::Function *function_obj);

  /**
   * Get the Function factory object corresponding to the given name.
   */
  const plugin::Function *get(const char *name, size_t len=0) const;

};

} /* end namespace service */
} /* end namespace drizzled */

#endif /* DRIZZLED_SERVICE_FUNCTION_H */
