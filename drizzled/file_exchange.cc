/* -*- mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; -*-
 *  vim:expandtab:shiftwidth=2:tabstop=2:smarttab:
 *
 *  Copyright (C) 2008, 2009 Sun Microsystems, Inc.
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


#include <config.h>
#include <drizzled/file_exchange.h>

namespace drizzled
{

file_exchange::file_exchange(char *name, bool flag, enum_filetype filetype_arg)
  : filetype(filetype_arg),
    file_name(name),
    field_term(&default_field_term),
    enclosed(&my_empty_string),
    line_term(&default_line_term),
    line_start(&my_empty_string),
    escaped(&default_escaped),  
    opt_enclosed(0),
    dumpfile(flag),
    skip_lines(0),
    cs(NULL)
{ }

} /* namespace drizzled */
