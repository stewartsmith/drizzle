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

#include <config.h>
#include <cassert>
#include <drizzled/errmsg_print.h>
#include <drizzled/gettext.h>
#include <drizzled/identifier.h>
#include <drizzled/session.h>
#include <drizzled/internal/my_sys.h>

#include <drizzled/util/tablename_to_filename.h>
#include <drizzled/util/backtrace.h>
#include <drizzled/charset.h>

#include <algorithm>
#include <sstream>
#include <cstdio>

#include <boost/algorithm/string/compare.hpp>

using namespace std;

namespace drizzled {
namespace identifier {

Catalog::Catalog(const std::string &name_arg) :
  _name(name_arg)
{ 
  init();
}

Catalog::Catalog(const drizzled::LEX_STRING &name_arg) :
  _name(name_arg.str, name_arg.length)
{
  init();
}

void Catalog::init()
{ 
  assert(not _name.empty());
  path.append("../");
  if (util::tablename_to_filename(_name, path))
    errmsg_printf(error::ERROR, _("Catalog name cannot be encoded and fit within filesystem name length restrictions."));
  assert(path.length()); // TODO throw exception, this is a possibility
  hash_value= util::insensitive_hash()(path);
}

bool Catalog::compare(const std::string &arg) const
{
  return boost::iequals(arg, _name);
}

bool Catalog::isValid() const
{
  if (_name.empty()
    || _name.size() > NAME_LEN
    || _name.at(_name.length() -1 ) == ' ')
    return false;
  const charset_info_st& cs= my_charset_utf8mb4_general_ci;
  int well_formed_error;
  uint32_t res= cs.cset->well_formed_len(&cs, _name.c_str(), _name.c_str() + _name.length(), NAME_CHAR_LEN, &well_formed_error);
  if (well_formed_error)
  {
    my_error(ER_INVALID_CHARACTER_STRING, MYF(0), "identifier", _name.c_str());
    return false;
  }
  if (_name.length() != res)
    return false;
  return true;
}

} /* namespace identifier */
} /* namespace drizzled */
