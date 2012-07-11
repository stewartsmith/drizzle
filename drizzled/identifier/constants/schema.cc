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
#include <drizzled/catalog/local.h>

namespace drizzled {
namespace identifier {
namespace constants {

// Please note, UTF8 id's should not be used with this class
class Schema : public identifier::Schema
{
public:
  /* FIXME: use a global identifier for catalog */
  Schema(const char* name) :
    identifier::Schema(drizzled::catalog::local_identifier(), str_ref(name))
  {
  }

  bool isSystem() const
  {
    return true;
  }

private:
};

} /* namespace constants */


const Schema& data_dictionary()
{
  static constants::Schema g_dd= "DATA_DICTIONARY";
  return g_dd;
}

const Schema& information_schema()
{
  static constants::Schema g_is= "INFORMATION_SCHEMA";
  return g_is;
}

} /* namespace identifier */
} /* namespace drizzled */
