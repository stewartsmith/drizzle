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

#include "drizzled/schema_identifier.h"
#include "drizzled/session.h"
#include "drizzled/current_session.h"
#include "drizzled/internal/my_sys.h"
#include "drizzled/data_home.h"

#include <algorithm>
#include <sstream>
#include <cstdio>

using namespace std;

namespace drizzled
{

extern char *drizzle_tmpdir;
extern pid_t current_pid;

static const char hexchars[]= "0123456789abcdef";

static bool tablename_to_filename(const char *from, char *to, size_t to_length);

static size_t build_schema_filename(std::string &path, const char *db)
{
  char dbbuff[FN_REFLEN];
  bool conversion_error= false;

  memset(dbbuff, 0, sizeof(dbbuff));
  conversion_error= tablename_to_filename(db, dbbuff, sizeof(dbbuff));
  if (conversion_error)
  {
    errmsg_printf(ERRMSG_LVL_ERROR,
                  _("Schema name cannot be encoded and fit within filesystem "
                    "name length restrictions."));
    return 0;
  }
   

  int rootdir_len= strlen(FN_ROOTDIR);
  path.append(drizzle_data_home);
  ssize_t without_rootdir= path.length() - rootdir_len;

  /* Don't add FN_ROOTDIR if dirzzle_data_home already includes it */
  if (without_rootdir >= 0)
  {
    const char *tmp= path.c_str() + without_rootdir;

    if (memcmp(tmp, FN_ROOTDIR, rootdir_len) != 0)
      path.append(FN_ROOTDIR);
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
      to_length                 The size of the cursor name buffer.

  RETURN
    true if errors happen. false on success.
*/
static bool tablename_to_filename(const char *from, char *to, size_t to_length)
{
  
  size_t length= 0;
  for (; *from  && length < to_length; length++, from++)
  {
    if ((*from >= '0' && *from <= '9') ||
        (*from >= 'A' && *from <= 'Z') ||
        (*from >= 'a' && *from <= 'z') ||
/* OSX defines an extra set of high-bit and multi-byte characters
   that cannot be used on the filesystem. Instead of trying to sort
   those out, we'll just escape encode all high-bit-set chars on OSX.
   It won't really hurt anything - it'll just make some filenames ugly. */
#if !defined(TARGET_OS_OSX)
        ((unsigned char)*from >= 128) ||
#endif
        (*from == '_') ||
        (*from == ' ') ||
        (*from == '-'))
    {
      to[length]= *from;
      continue;
    }
   
    if (length + 3 >= to_length)
      return true;

    /* We need to escape this char in a way that can be reversed */
    to[length++]= '@';
    to[length++]= hexchars[(*from >> 4) & 15];
    to[length]= hexchars[(*from) & 15];
  }

  if (internal::check_if_legal_tablename(to) &&
      length + 4 < to_length)
  {
    memcpy(to + length, "@@@", 4);
    length+= 3;
  }
  return false;
}

const std::string &SchemaIdentifier::getSQLPath()
{
  return getSchemaName();
}

const std::string &SchemaIdentifier::getPath()
{
  if (db_path.empty())
  {
    drizzled::build_schema_filename(db_path, lower_db.c_str());
    assert(db_path.length()); // TODO throw exception, this is a possibility
  }

  return db_path;
}

bool SchemaIdentifier::compare(std::string arg)
{
  std::transform(arg.begin(), arg.end(),
                 arg.begin(), ::tolower);

  return arg == lower_db;
}

bool SchemaIdentifier::isValid()
{
  if (lower_db.empty())
    return false;

  if (lower_db.size() > NAME_LEN)
    return false;

  if (lower_db.at(lower_db.length() -1) == ' ')
    return false;

  const CHARSET_INFO * const cs= &my_charset_utf8mb4_general_ci;

  int well_formed_error;
  uint32_t res= cs->cset->well_formed_len(cs, lower_db.c_str(), lower_db.c_str() + lower_db.length(),
                                          NAME_CHAR_LEN, &well_formed_error);

  if (well_formed_error)
  {
    my_error(ER_INVALID_CHARACTER_STRING, MYF(0), "identifier", lower_db.c_str());
    return false;
  }

  if (lower_db.length() != res)
    return false;

  return true;
}

} /* namespace drizzled */
