/* -*- mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; -*-
 *  vim:expandtab:shiftwidth=2:tabstop=2:smarttab:
 *
 *  Copyright (C) 2008 Sun Microsystems
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; version 2 of the License.
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


#ifndef DRIZZLED_FILE_EXCHANGE_H
#define DRIZZLED_FILE_EXCHANGE_H

#include <drizzled/server_includes.h>

static String default_line_term("\n",default_charset_info);
static String default_escaped("\\",default_charset_info);
static String default_field_term("\t",default_charset_info);

/*
  Used to hold information about file and file structure in exchange
  via non-DB file (...INTO OUTFILE..., ...LOAD DATA...)
  XXX: We never call destructor for objects of this class.
*/

class file_exchange :public Sql_alloc
{
public:
  enum enum_filetype filetype; /* load XML, Added by Arnold & Erik */
  char *file_name;
  String *field_term,*enclosed,*line_term,*line_start,*escaped;
  bool opt_enclosed;
  bool dumpfile;
  ulong skip_lines;
  const CHARSET_INFO *cs;
  file_exchange(char *name, bool flag,
                enum_filetype filetype_arg= FILETYPE_CSV)
    :file_name(name), opt_enclosed(0), dumpfile(flag), skip_lines(0)
  {
    filetype= filetype_arg;
    field_term= &default_field_term;
    enclosed=   line_start= &my_empty_string;
    line_term=  &default_line_term;
    escaped=    &default_escaped;
    cs= NULL;
  }
};


#endif /* DRIZZLED_FILE_EXCHANGE_H */
