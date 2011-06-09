/* Copyright (C) 2000 MySQL AB

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA */

#include <config.h>

#include <drizzled/charset.h>
#include <drizzled/internal/my_sys.h>
#include <drizzled/internal/m_string.h>

namespace drizzled
{
namespace internal
{

/*
  List of file names that causes problem on windows

  NOTE that one can also not have file names of type CON.TXT

  NOTE: it is important to keep "CLOCK$" on the first place,
  we skip it in check_if_legal_tablename.
*/
static const char *reserved_names[]=
{
  "CLOCK$",
  "CON", "PRN", "AUX", "NUL",
  "COM1", "COM2", "COM3", "COM4", "COM5", "COM6", "COM7", "COM8", "COM9",
  "LPT1", "LPT2", "LPT3", "LPT4", "LPT5", "LPT6", "LPT7", "LPT8", "LPT9",
  NULL
};


/*
  Looks up a null-terminated string in a list,
  case insensitively.

  SYNOPSIS
    str_list_find()
    list        list of items
    str         item to find

  RETURN
    0  ok
    1  reserved file name
*/
static int str_list_find(const char **list, const char *str)
{
  const char **name;
  for (name= list; *name; name++)
  {
    if (!my_strcasecmp(&my_charset_utf8_general_ci, *name, str))
      return 1;
  }
  return 0;
}


/*
  A map for faster reserved_names lookup,
  helps to avoid loops in many cases.
  1 - can be the first letter
  2 - can be the second letter
  4 - can be the third letter
*/
static char reserved_map[256]=
{
  0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, /* ................ */
  0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, /* ................ */
  0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, /*  !"#$%&'()*+,-./ */
  0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, /* 0123456789:;<=>? */
  0,1,0,1,0,0,0,0,0,0,0,0,7,4,5,2, /* @ABCDEFGHIJKLMNO */
  3,0,2,0,4,2,0,0,4,0,0,0,0,0,0,0, /* PQRSTUVWXYZ[\]^_ */
  0,1,0,1,0,0,0,0,0,0,0,0,7,4,5,2, /* bcdefghijklmno */
  3,0,2,0,4,2,0,0,4,0,0,0,0,0,0,0, /* pqrstuvwxyz{|}~. */
  0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, /* ................ */
  0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, /* ................ */
  0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, /* ................ */
  0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, /* ................ */
  0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, /* ................ */
  0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, /* ................ */
  0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, /* ................ */
  0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0  /* ................ */
};


/*
  Check if a table name may cause problems

  SYNOPSIS
    check_if_legal_tablename
    name 	Table name (without any extensions)

  DESCRIPTION
    We don't check 'CLOCK$' because dollar sign is encoded as @0024,
    making table file name 'CLOCK@0024', which is safe.
    This is why we start lookup from the second element
    (i.e. &reserver_name[1])

  RETURN
    0  ok
    1  reserved file name
*/

int check_if_legal_tablename(const char *name)
{
  return((reserved_map[(unsigned char) name[0]] & 1) &&
              (reserved_map[(unsigned char) name[1]] & 2) &&
              (reserved_map[(unsigned char) name[2]] & 4) &&
              str_list_find(&reserved_names[1], name));
}


} /* namespace internal */
} /* namespace drizzled */
