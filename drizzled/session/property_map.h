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

#ifndef DRIZZLED_SESSION_PROPERTY_MAP_H
#define DRIZZLED_SESSION_PROPERTY_MAP_H

#include <drizzled/util/string.h>
#include <boost/unordered_map.hpp>

namespace drizzled
{

class Session;

namespace session
{

class PropertyMap {
private:
  typedef boost::unordered_map<std::string, util::Storable *, util::insensitive_hash, util::insensitive_equal_to> Map;

public:
  typedef Map::iterator iterator;
  typedef Map::const_iterator const_iterator;

  drizzled::util::Storable *getProperty(const std::string &arg)
  {
    return _properties[arg];
  }

  template<class T>
  bool setProperty(const std::string &arg, T *value)
  {
    _properties[arg]= value;

    return true;
  }

  iterator begin()
  {
    return _properties.begin();
  }

  iterator end()
  {
    return _properties.end();
  }

  const_iterator begin() const
  {
    return _properties.begin();
  }

  const_iterator end() const
  {
    return _properties.end();
  }

  ~PropertyMap()
  {
    for (iterator iter= _properties.begin(); iter != _properties.end(); iter++)
    {
      boost::checked_delete((*iter).second);
    }
    _properties.clear();
  }

private:
  Map _properties;
};

} /* namespace session */
} /* namespace drizzled */

#endif /* DRIZZLED_SESSION_PROPERTY_MAP_H */
