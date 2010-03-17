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

#include "drizzled/table_identifier.h"
#include "drizzled/session.h"
#include "drizzled/current_session.h"
#include "drizzled/internal/my_sys.h"
#include "drizzled/data_home.h"

#include <algorithm>
#include <sstream>

using namespace std;

namespace drizzled
{

extern char *drizzle_tmpdir;
extern pid_t current_pid;

static const char hexchars[]= "0123456789abcdef";

/*
  Translate a cursor name to a table name (WL #1324).

  SYNOPSIS
    filename_to_tablename()
      from                      The cursor name
      to                OUT     The table name
      to_length                 The size of the table name buffer.

  RETURN
    Table name length.
*/
uint32_t filename_to_tablename(const char *from, char *to, uint32_t to_length)
{
  uint32_t length= 0;

  if (!memcmp(from, TMP_FILE_PREFIX, TMP_FILE_PREFIX_LENGTH))
  {
    /* Temporary table name. */
    length= strlen(strncpy(to, from, to_length));
  }
  else
  {
    for (; *from  && length < to_length; length++, from++)
    {
      if (*from != '@')
      {
        to[length]= *from;
        continue;
      }
      /* We've found an escaped char - skip the @ */
      from++;
      to[length]= 0;
      /* There will be a two-position hex-char version of the char */
      for (int x=1; x >= 0; x--)
      {
        if (*from >= '0' && *from <= '9')
          to[length] += ((*from++ - '0') << (4 * x));
        else if (*from >= 'a' && *from <= 'f')
          to[length] += ((*from++ - 'a' + 10) << (4 * x));
      }
      /* Backup because we advanced extra in the inner loop */
      from--;
    } 
  }

  return length;
}

/*
  Creates path to a cursor: drizzle_tmpdir/#sql1234_12_1.ext

  SYNOPSIS
   build_tmptable_filename()
     session                    The thread handle.
     buff                       Where to write result
     bufflen                    buff size

  NOTES

    Uses current_pid, thread_id, and tmp_table counter to create
    a cursor name in drizzle_tmpdir.

  RETURN
    path length on success, 0 on failure
*/

size_t build_tmptable_filename(char *buff, size_t bufflen)
{
  size_t length;
  ostringstream path_str, post_tmpdir_str;
  string tmp;

  Session *session= current_session;

  path_str << drizzle_tmpdir;
  post_tmpdir_str << "/" << TMP_FILE_PREFIX << current_pid;
  post_tmpdir_str << session->thread_id << session->tmp_table++;
  tmp= post_tmpdir_str.str();

  transform(tmp.begin(), tmp.end(), tmp.begin(), ::tolower);

  path_str << tmp;

  if (bufflen < path_str.str().length())
    length= 0;
  else
    length= internal::unpack_filename(buff, path_str.str().c_str());

  return length;
}

/*
  Creates path to a cursor: drizzle_data_dir/db/table.ext

  SYNOPSIS
   build_table_filename()
     buff                       Where to write result
                                This may be the same as table_name.
     bufflen                    buff size
     db                         Database name
     table_name                 Table name
     ext                        File extension.
     flags                      FN_FROM_IS_TMP or FN_TO_IS_TMP
                                table_name is temporary, do not change.

  NOTES

    Uses database and table name, and extension to create
    a cursor name in drizzle_data_dir. Database and table
    names are converted from system_charset_info into "fscs".
    Unless flags indicate a temporary table name.
    'db' is always converted.
    'ext' is not converted.

    The conversion suppression is required for ALTER Table. This
    statement creates intermediate tables. These are regular
    (non-temporary) tables with a temporary name. Their path names must
    be derivable from the table name. So we cannot use
    build_tmptable_filename() for them.

  RETURN
    path length on success, 0 on failure
*/

size_t build_table_filename(char *buff, size_t bufflen, const char *db, const char *table_name, bool is_tmp)
{
  char dbbuff[FN_REFLEN];
  char tbbuff[FN_REFLEN];
  bool conversion_error= false;

  memset(tbbuff, 0, sizeof(tbbuff));
  if (is_tmp) // FN_FROM_IS_TMP | FN_TO_IS_TMP
    strncpy(tbbuff, table_name, sizeof(tbbuff));
  else
  {
    conversion_error= tablename_to_filename(table_name, tbbuff, sizeof(tbbuff));
    if (conversion_error)
    {
      errmsg_printf(ERRMSG_LVL_ERROR,
                    _("Table name cannot be encoded and fit within filesystem "
                      "name length restrictions."));
      return 0;
    }
  }
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
  string table_path(drizzle_data_home);
  int without_rootdir= table_path.length()-rootdir_len;

  /* Don't add FN_ROOTDIR if dirzzle_data_home already includes it */
  if (without_rootdir >= 0)
  {
    const char *tmp= table_path.c_str()+without_rootdir;
    if (memcmp(tmp, FN_ROOTDIR, rootdir_len) != 0)
      table_path.append(FN_ROOTDIR);
  }

  table_path.append(dbbuff);
  table_path.append(FN_ROOTDIR);
  table_path.append(tbbuff);

  if (bufflen < table_path.length())
    return 0;

  strcpy(buff, table_path.c_str());

  return table_path.length();
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
bool tablename_to_filename(const char *from, char *to, size_t to_length)
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



const char *TableIdentifier::getPath()
{
  if (not path_inited)
  {
    size_t path_length= 0;

    switch (type) {
    case STANDARD_TABLE:
      path_length= build_table_filename(path, sizeof(path),
                                        db.c_str(), table_name.c_str(),
                                        false);
      break;
    case INTERNAL_TMP_TABLE:
      path_length= build_table_filename(path, sizeof(path),
                                        db.c_str(), table_name.c_str(),
                                        true);
      break;
    case TEMP_TABLE:
      path_length= build_tmptable_filename(path, sizeof(path));
      break;
    case SYSTEM_TMP_TABLE:
      assert(0);
    }
    path_inited= true;
    assert(path_length); // TODO throw exception, this is a possibility
  }

  return path;
}

} /* namespace drizzled */
