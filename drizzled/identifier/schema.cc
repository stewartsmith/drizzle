/* -*- mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; -*-
 *  vim:expandtab:shiftwidth=2:tabstop=2:smarttab:
 *
 *  Copyright (C) 2009 Sun Microsystems
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

#include "config.h"

#include <assert.h>

#include "drizzled/identifier.h"
#include "drizzled/session.h"
#include "drizzled/current_session.h"
#include "drizzled/internal/my_sys.h"

#include "drizzled/util/backtrace.h"

#include <algorithm>
#include <sstream>
#include <cstdio>

#include <boost/algorithm/string/compare.hpp>

using namespace std;

namespace drizzled
{

extern string drizzle_tmpdir;
extern pid_t current_pid;

static const char hexchars[]= "0123456789abcdef";

static bool tablename_to_filename(const string &from, string &to);

static size_t build_schema_filename(string &path, const string &db)
{
  string dbbuff("");
  bool conversion_error= false;

  conversion_error= tablename_to_filename(db, dbbuff);
  if (conversion_error)
  {
    errmsg_printf(ERRMSG_LVL_ERROR,
                  _("Schema name cannot be encoded and fit within filesystem "
                    "name length restrictions."));
    return 0;
  }
   

  path.append(dbbuff);

  return path.length();
}


/*
  Translate a table name to a cursor name (WL #1324).

  SYNOPSIS
    tablename_to_filename()
      from                      The table name
      to                OUT     The cursor name

  RETURN
    true if errors happen. false on success.
*/
static bool tablename_to_filename(const string &from, string &to)
{
  
  string::const_iterator iter= from.begin();
  for (; iter != from.end(); ++iter)
  {
    if ((*iter >= '0' && *iter <= '9') ||
        (*iter >= 'a' && *iter <= 'z') ||
        /* OSX defines an extra set of high-bit and multi-byte characters
          that cannot be used on the filesystem. Instead of trying to sort
          those out, we'll just escape encode all high-bit-set chars on OSX.
          It won't really hurt anything - it'll just make some filenames ugly. */
#if !defined(TARGET_OS_OSX)
        ((unsigned char)*iter >= 128) ||
#endif
        (*iter == '_') ||
        (*iter == ' ') ||
        (*iter == '-'))
    {
      to.push_back(*iter);
      continue;
    }

    if ((*iter >= 'A' && *iter <= 'Z'))
    {
      to.push_back(tolower(*iter));
      continue;
    }
   
    /* We need to escape this char in a way that can be reversed */
    to.push_back('@');
    to.push_back(hexchars[(*iter >> 4) & 15]);
    to.push_back(hexchars[(*iter) & 15]);
  }

  if (internal::check_if_legal_tablename(to.c_str()))
  {
    to.append("@@@");
  }
  return false;
}

SchemaIdentifier::SchemaIdentifier(const std::string &db_arg) :
  db(db_arg),
  db_path(""),
  catalog("LOCAL")
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
    drizzled::build_schema_filename(db_path, db);
    assert(db_path.length()); // TODO throw exception, this is a possibility
  }
}

const std::string &SchemaIdentifier::getSQLPath()
{
  return getSchemaName();
}

const std::string &SchemaIdentifier::getPath() const
{
  return db_path;
}

bool SchemaIdentifier::compare(const std::string &arg) const
{
  return boost::iequals(arg, db);
}

bool SchemaIdentifier::isValid() const
{
  if (db.empty())
    return false;

  if (db.size() > NAME_LEN)
    return false;

  if (db.at(db.length() -1) == ' ')
    return false;

  const CHARSET_INFO * const cs= &my_charset_utf8mb4_general_ci;

  int well_formed_error;
  uint32_t res= cs->cset->well_formed_len(cs, db.c_str(), db.c_str() + db.length(),
                                          NAME_CHAR_LEN, &well_formed_error);

  if (well_formed_error)
  {
    my_error(ER_INVALID_CHARACTER_STRING, MYF(0), "identifier", db.c_str());
    return false;
  }

  if (db.length() != res)
    return false;

  return true;
}

} /* namespace drizzled */
