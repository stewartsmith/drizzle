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

#include <assert.h>

#include <drizzled/identifier.h>
#include <drizzled/session.h>
#include <drizzled/internal/my_sys.h>

#include <drizzled/util/tablename_to_filename.h>
#include <drizzled/util/backtrace.h>

#include <algorithm>
#include <sstream>
#include <cstdio>

#include <boost/algorithm/string/compare.hpp>

using namespace std;

namespace drizzled
{

namespace identifier
{

static void build_schema_filename(string &path, const string &name_arg)
{
  path.append("../");
  bool conversion_error= false;

  conversion_error= util::tablename_to_filename(name_arg, path);
  if (conversion_error)
  {
    errmsg_printf(error::ERROR,
                  _("Catalog name cannot be encoded and fit within filesystem "
                    "name length restrictions."));
  }
}

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

void  Catalog::init()
{ 
  assert(not _name.empty());

  build_schema_filename(path, _name);
  assert(path.length()); // TODO throw exception, this is a possibility

  util::insensitive_hash hasher;
  hash_value= hasher(path);
}

const std::string &Catalog::getPath() const
{
  return path;
}

bool Catalog::compare(const std::string &arg) const
{
  return boost::iequals(arg, _name);
}

bool Catalog::isValid() const
{
  if (_name.empty())
    return false;

  if (_name.size() > NAME_LEN)
    return false;

  if (_name.at(_name.length() -1) == ' ')
    return false;

  const CHARSET_INFO * const cs= &my_charset_utf8mb4_general_ci;

  int well_formed_error;
  uint32_t res= cs->cset->well_formed_len(cs, _name.c_str(), _name.c_str() + _name.length(),
                                          NAME_CHAR_LEN, &well_formed_error);

  if (well_formed_error)
  {
    my_error(ER_INVALID_CHARACTER_STRING, MYF(0), "identifier", _name.c_str());
    return false;
  }

  if (_name.length() != res)
    return false;

  return true;
}

std::size_t hash_value(Catalog const& b)
{
  return b.getHashValue();
}

void Catalog::getSQLPath(std::string &sql_path) const
{
  sql_path= _name;
}



} /* namespace identifier */
} /* namespace drizzled */
