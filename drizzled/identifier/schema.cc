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

extern string drizzle_tmpdir;

static size_t build_schema_filename(string &path, const string &db)
{
  path.append("");
  bool conversion_error= false;

  conversion_error= util::tablename_to_filename(db, path);
  if (conversion_error)
  {
    errmsg_printf(error::ERROR,
                  _("Schema name cannot be encoded and fit within filesystem "
                    "name length restrictions."));
    return 0;
  }

  return path.length();
}

Schema::Schema(const std::string &db_arg) :
  db(db_arg)
{ 
#if 0
  string::size_type lastPos= db.find_first_of('/', 0);

  if (lastPos != std::string::npos) 
  {
    catalog= db.substr(0, lastPos);
    db.erase(0, lastPos + 1);
  }
#endif

  if (not db_arg.empty())
  {
    build_schema_filename(db_path, db);
    assert(db_path.length()); // TODO throw exception, this is a possibility
  }
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
  bool error= false;

  do
  {
    if (db.empty())
    {
      error= true;
      break;
    }

    if (db.size() > NAME_LEN)
    {
      error= true;
      break;
    }

    if (db.at(db.length() -1) == ' ')
    {
      error= true;
      break;
    }

    if (db.at(0) == '.')
    {
      error= true;
      break;
    }

    {
      const charset_info_st * const cs= &my_charset_utf8mb4_general_ci;

      int well_formed_error;
      uint32_t res= cs->cset->well_formed_len(cs, db.c_str(), db.c_str() + db.length(),
                                              NAME_CHAR_LEN, &well_formed_error);
      if (well_formed_error or db.length() != res)
      {
        error= true;
        break;
      }
    }
  } while (0);

  if (error)
  {
    my_error(ER_WRONG_DB_NAME, *this);

    return false;
  }

  return true;
}

const std::string &Schema::getCatalogName() const
{
  return drizzled::catalog::local_identifier().name();
}

std::ostream& operator<<(std::ostream& output, const Schema&identifier)
{
  return output << "identifier::Schema:(" <<  drizzled::catalog::local_identifier() << ", " <<  identifier.getSchemaName() << ", " << identifier.getPath() << ")";
}

} /* namespace identifier */
} /* namespace drizzled */
