/* -*- mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; -*-
 *  vim:expandtab:shiftwidth=2:tabstop=2:smarttab:
 *
 *  Copyright (C) 2008 Sun Microsystems, Inc.
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


#pragma once

#include <drizzled/sql_string.h>
#include <drizzled/memory/sql_alloc.h>
#include <drizzled/enum.h>

namespace drizzled {

extern const charset_info_st *default_charset_info;

static String default_line_term("\n",default_charset_info);
static String default_escaped("\\",default_charset_info);
static String default_field_term("\t",default_charset_info);

/*
  Used to hold information about file and file structure in exchange
  via non-DB file (...INTO OUTFILE..., ...LOAD DATA...)
  XXX: We never call destructor for objects of this class.
*/

class file_exchange :
  public memory::SqlAlloc
{
public:
  enum enum_filetype filetype; /* load XML, Added by Arnold & Erik */
  char *file_name;
  String *field_term,*enclosed,*line_term,*line_start,*escaped;
  bool opt_enclosed;
  bool dumpfile;
  ulong skip_lines;
  const charset_info_st *cs;
  file_exchange(char *name, bool flag,
                enum_filetype filetype_arg= FILETYPE_CSV);
};


} /* namespace drizzled */

