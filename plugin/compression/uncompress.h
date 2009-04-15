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

#ifndef DRIZZLED_PLUGIN_COMPRESSION_UNCOMPRESS_H
#define DRIZZLED_PLUGIN_COMPRESSION_UNCOMPRESS_H

#include <drizzled/server_includes.h>
#include <drizzled/session.h>
#include <drizzled/error.h>
#include <drizzled/function/str/strfunc.h>

class Item_func_uncompress: public Item_str_func
{
  String buffer;
public:
  Item_func_uncompress(): Item_str_func(){}
  void fix_length_and_dec(){ maybe_null= 1; max_length= MAX_BLOB_WIDTH; }
  const char *func_name() const{return "uncompress";}
  String *val_str(String *) ;
};


#endif /* DRIZZLED_PLUGIN_COMPRESSION_UNCOMPRESS_H */
