/* -*- mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; -*-
 *  vim:expandtab:shiftwidth=2:tabstop=2:smarttab:
 *
 *  Copyright (C) 2009 Sun Microsystems, Inc.
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
#include <drizzled/catalog/local.h>
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

Schema::Schema(const drizzled::identifier::Catalog &catalog_arg,
                 str_ref db_arg) :
  db(db_arg.data(), db_arg.size()),
  _catalog(catalog_arg)
{ 
#if 0
  string::size_type lastPos= db.find_first_of('/', 0);

  if (lastPos != std::string::npos) 
  {
    catalog= db.substr(0, lastPos);
    db.erase(0, lastPos + 1);
  }
#endif

  if (db_arg.empty() == false)
  {
    db_path += _catalog.getPath();
    db_path += FN_LIBCHAR;
    db_path += util::tablename_to_filename(db);
    assert(db_path.length()); // TODO throw exception, this is a possibility
  }

  _corrected_db= boost::to_lower_copy(db);
}

const std::string &Schema::getPath() const
{
  return db_path;
}

bool Schema::compare(const std::string &arg) const
{
  return boost::iequals(arg, db);
}

bool Schema::compare(const Schema& arg) const
{
  return boost::iequals(arg.getSchemaName(), db);
}

bool Schema::isValid() const
{
  const charset_info_st& cs= my_charset_utf8mb4_general_ci;
  int well_formed_error;
  if (not db.empty()
    && db.size() <= NAME_LEN
    && db.at(0) != '.'
    && db.at(db.size() - 1) != ' '
    && db.size() == cs.cset->well_formed_len(cs, db, NAME_CHAR_LEN, &well_formed_error))
  {
    if (not well_formed_error)
      return true;
  }
  my_error(ER_WRONG_DB_NAME, *this);
  return false;
}

const std::string &Schema::getCatalogName() const
{
  return _catalog.name();
}

std::ostream& operator<<(std::ostream& output, const Schema&identifier)
{
  return output << "identifier::Schema:(" <<  identifier.getCatalogName() << ", " <<  identifier.getSchemaName() << ", " << identifier.getPath() << ")";
}

} /* namespace identifier */
} /* namespace drizzled */
