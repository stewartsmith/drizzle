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

/**
  Item_empty_string -- is a utility class to put an item into List<Item>
  which is then used in protocol.send_fields() when sending SHOW output to
  the client.
*/

#include <drizzled/item/string.h>

namespace drizzled
{

class Item_empty_string :public Item_string
{
public:
  Item_empty_string(const char *header,uint32_t length, const charset_info_st * cs= NULL) :
    Item_string("",0, cs ? cs : &my_charset_utf8_general_ci)
    { name= const_cast<char*>(header); max_length= cs ? length * cs->mbmaxlen : length; }
  void make_field(SendField *field);
};

} /* namespace drizzled */
  
