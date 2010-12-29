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

#ifndef DRIZZLED_IDENTIFIER_CATALOG_H
#define DRIZZLED_IDENTIFIER_CATALOG_H

#include <drizzled/enum.h>
#include "drizzled/definitions.h"
#include "drizzled/message/table.pb.h"
#include <string.h>

#include <assert.h>

#include <ostream>
#include <list>
#include <algorithm>
#include <functional>
#include <iostream>

#include <boost/algorithm/string.hpp>

namespace drizzled {
namespace identifier {

class Catalog
{
  std::string name;
  std::string path;


  // @note this should be changed to protected once Session contains an
  // identifier for current db.
public:

public:
  typedef std::vector<Catalog> vector;

  Catalog(const std::string &name_arg);

  virtual ~Catalog()
  { }

  const std::string &getPath() const;

  const std::string &getName() const
  {
    return name;
  }

  bool isValid() const;
  bool compare(const std::string &arg) const;

  size_t getHashValue() const
  {
    return hash_value;
  }

  friend bool operator<(const Catalog &left, const Catalog &right)
  {
    return  boost::algorithm::to_upper_copy(left.getName()) < boost::algorithm::to_upper_copy(right.getName());
  }

  friend std::ostream& operator<<(std::ostream& output, const Catalog &identifier)
  {
    output << "Catalog:(";
    output <<  identifier.getName();
    output << ", ";
    output << identifier.getPath();
    output << ")";

    return output;  // for multiple << operators.
  }

  friend bool operator==(const Catalog &left,
                         const Catalog &right)
  {
    return boost::iequals(left.getName(), right.getName());
  }

private:
  size_t hash_value;

};

std::size_t hash_value(Catalog const& b);

} /* namespace identifier */
} /* namespace drizzled */

#endif /* DRIZZLED_IDENTIFIER_CATALOG_H */
