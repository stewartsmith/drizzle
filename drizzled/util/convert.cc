/* - mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; -*-
 *  vim:expandtab:shiftwidth=2:tabstop=2:smarttab:
 *
 *  Copyright (C) 2009 Sun Microsystems, Inc.
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

#include <config.h>

#include <drizzled/util/convert.h>
#include <string>
#include <iomanip>
#include <sstream>

using namespace std;

namespace drizzled
{

uint64_t drizzled_string_to_hex(char *to, const char *from, uint64_t from_size)
{
  static const char hex_map[]= "0123456789ABCDEF";
  const char *from_end;

  for (from_end= from + from_size; from != from_end; from++)
  {
    *to++= hex_map[((unsigned char) *from) >> 4];
    *to++= hex_map[((unsigned char) *from) & 0xF];
  }

  *to= 0;

  return from_size * 2;
}

static inline char hex_char_value(char c)
{
  if (c >= '0' && c <= '9')
    return c - '0';
  else if (c >= 'A' && c <= 'F')
    return c - 'A' + 10;
  else if (c >= 'a' && c <= 'f')
    return c - 'a' + 10;
  return 0;
}

void drizzled_hex_to_string(char *to, const char *from, uint64_t from_size)
{
  const char *from_end = from + from_size;

  while (from != from_end)
  {
    *to= hex_char_value(*from++) << 4;
    if (from != from_end)
      *to++|= hex_char_value(*from++);
  }
}

void bytesToHexdumpFormat(string &to, const unsigned char *from, size_t from_length)
{
  static const char hex_map[]= "0123456789abcdef";
  unsigned int x, y;
  ostringstream line_number;

  for (x= 0; x < from_length; x+= 16)
  {
    line_number << setfill('0') << setw(6);
    line_number << x;
    to.append(line_number.str());
    to.append(": ", 2);

    for (y= 0; y < 16; y++)
    {
      if ((x + y) < from_length)
      {
        to.push_back(hex_map[(from[x+y]) >> 4]);
        to.push_back(hex_map[(from[x+y]) & 0xF]);
        to.push_back(' ');
      }
      else
        to.append("   ");
    }
    to.push_back(' ');
    for (y= 0; y < 16; y++)
    {
      if ((x + y) < from_length)
        to.push_back(isprint(from[x + y]) ? from[x + y] : '.');
    }
    to.push_back('\n');
    line_number.str("");
  }
}

} /* namespace drizzled */
