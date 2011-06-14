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

#include <drizzled/enum.h>
#include <drizzled/definitions.h>
#include <string.h>

#include <assert.h>

#include <ostream>
#include <vector>
#include <algorithm>
#include <functional>
#include <iostream>

#include <boost/algorithm/string.hpp>

namespace drizzled {
namespace identifier {

class Catalog : public Identifier
{
public:
  Catalog(const std::string &name_arg);
  Catalog(const drizzled::lex_string_t &name_arg);
  bool isValid() const;
  bool compare(const std::string &arg) const;

  const std::string &getPath() const
  {
    return path;
  }

  const std::string &getName() const
  {
    return _name;
  }

  const std::string &name() const
  {
    return _name;
  }

  virtual std::string getSQLPath() const
  {
    return _name;
  }

  size_t getHashValue() const
  {
    return hash_value;
  }

  friend bool operator<(const Catalog &left, const Catalog &right)
  {
    return boost::ilexicographical_compare(left.getName(), right.getName());
  }

  friend std::ostream& operator<<(std::ostream& output, const Catalog &identifier)
  {
    return output << "Catalog:(" <<  identifier.getName() << ", " << identifier.getPath() << ")";
  }

  friend bool operator==(const Catalog &left, const Catalog &right)
  {
    return boost::iequals(left.getName(), right.getName());
  }
private:
  void init();

  std::string _name;
  std::string path;
  size_t hash_value;
};

inline std::size_t hash_value(Catalog const& b)
{
  return b.getHashValue();
}

} /* namespace identifier */
} /* namespace drizzled */
