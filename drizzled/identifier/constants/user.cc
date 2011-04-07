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

#include <drizzled/identifier.h>

namespace drizzled {
namespace identifier {
namespace constants {

// Please note, UTF8 id's should not be used with this class
class User : public identifier::User
{
public:
  User(const std::string &name) :
    identifier::User(name)
  {
  }

  const std::string &getPath() const
  {
    return _path;
  }

private:
  std::string _path;
};

} /* namespace constants */

const identifier::User& system_user()
{
  static drizzled::identifier::User _tmp("SYSTEM");

  return _tmp;
}

} /* namespace identifier */
} /* namespace drizzled */
