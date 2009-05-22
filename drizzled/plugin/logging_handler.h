/*
 -*- mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; -*-
 *  vim:expandtab:shiftwidth=2:tabstop=2:smarttab:

 *  Definitions required for Query Logging plugin

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

#ifndef DRIZZLED_PLUGIN_LOGGING_H
#define DRIZZLED_PLUGIN_LOGGING_H

#include <string>

class Logging_handler
{
  std::string name;
public:
  Logging_handler(std::string name_arg): name(name_arg)  {}
  Logging_handler(const char *name_arg): name(name_arg)  {}
  virtual ~Logging_handler() {}

  std::string getName() { return name; }
  /**
   * Make these no-op rather than pure-virtual so that it's easy for a plugin
   * to only 
   */
  virtual bool pre(Session *) {return false;}
  virtual bool post(Session *) {return false;}
};

#endif /* DRIZZLED_PLUGIN_LOGGING_H */
