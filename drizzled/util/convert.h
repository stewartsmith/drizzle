/* - mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; -*-
 *  vim:expandtab:shiftwidth=2:tabstop=2:smarttab:
 *
 *  Copyright (C) 2008 MySQL
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



#pragma once

#include <boost/lexical_cast.hpp>
#include <string>
#include <iostream>
#include <sstream>

#include <drizzled/visibility.h>

namespace drizzled
{

template <class T>
std::string to_string(T t)
{
  return boost::lexical_cast<std::string>(t);
}

template <class T>
std::string& to_string(std::string &str, T t)
{
  std::ostringstream o(str);
  o << t;
  return str;
}

void bytesToHexdumpFormat(std::string &s, const unsigned char *from, size_t from_length);

DRIZZLED_API uint64_t drizzled_string_to_hex(char *to, const char *from,
                                             uint64_t from_size);
DRIZZLED_API void drizzled_hex_to_string(char *to, const char *from,
                                         uint64_t from_size);

} /* namespace drizzled */

