/* - mode: c; c-basic-offset: 2; indent-tabs-mode: nil; -*-
   vim:expandtab:shiftwidth=2:tabstop=2:smarttab:

  Copyright (C) 2010 Monty Taylor

  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation; version 2 of the License.
 
  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA

  Copyright (C) 1995-2006 International Business Machines Corporation and others

  All rights reserved.

  Permission is hereby granted, free of charge, to any person obtaining a copy
  of this software and associated documentation files (the "Software"),
  to deal in the Software without restriction, including without limitation
  the rights to use, copy, modify, merge, publish, distribute, and/or sell
  copies of the Software, and to permit persons
  to whom the Software is furnished to do so, provided that the above
  copyright notice(s) and this permission notice appear in all copies
  of the Software and that both the above copyright notice(s) and this
  permission notice appear in supporting documentation.

  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT OF THIRD PARTY RIGHTS.
  IN NO EVENT SHALL THE COPYRIGHT HOLDER OR HOLDERS INCLUDED IN THIS NOTICE BE
  LIABLE FOR ANY CLAIM, OR ANY SPECIAL INDIRECT OR CONSEQUENTIAL DAMAGES, OR
  ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER
  IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
  OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.

  Except as contained in this notice, the name of a copyright holder shall not
  be used in advertising or otherwise to promote the sale, use or other dealings
  in this Software without prior written authorization of the copyright holder.
*/

#pragma once

#include <string>

#include <drizzled/utf8/checked.h>
#include <drizzled/utf8/unchecked.h>

namespace drizzled
{
namespace utf8
{

/**
 * The maximum number of UTF-8 code units (bytes) per Unicode code point (U+0000..U+10ffff).
 */
static const int MAX_LENGTH= 4;

/**
 * Does this code unit (byte) encode a code point by itself (US-ASCII 0..0x7f)?
 * @param c 8-bit code unit (byte)
 * @return TRUE or FALSE
 */
template <class T>
bool is_single(T c)
{
  return (static_cast<uint8_t>(c) & 0x80) == 0;
}

/**
 * How many code units (bytes) are used for the UTF-8 encoding
 * of this Unicode code point?
 * @param c 32-bit code point
 * @return 1..4, or 0 if c is a surrogate or not a Unicode code point
 */
static inline int codepoint_length(uint32_t c)
{
  return (c <= 0x7f ? 1 :
          (c <= 0x7ff ? 2 :
           (c <= 0xd7ff ? 3 :
            (c <= 0xdfff || c>0x10ffff ? 0 :
             (c <= 0xffff ? 3 : 4)))));
}

/**
 * How many bytes are used for the UTF-8 encoding of the codepoint of which
 * this is the first byte?
 * @param c 8-bit leading byte
 * @return 1..4, or 0 if c is a surrogate or not a Unicode code point
 */ 
template <class T>
int sequence_length(T c)
{
  return (static_cast<uint8_t>(c) < 0x80 ? 1 :
          ((static_cast<uint8_t>(c) >> 5) == 0x6 ? 2 :
           ((static_cast<uint8_t>(c) >> 4) == 0xe ? 3 :
            ((static_cast<uint8_t>(c) >> 3) == 0x1e ? 4 : 0))));
}


/**
 * How many logical characters does the given UTF-8 string occupy? Useful when
 * needing to calculate char length rather than byte length of a string
 * @param in_string string to measure
 * @return length in characters of given string
 */
static inline uint32_t char_length(const std::string &in_string)
{
  uint32_t length= 0;
  int seq_length= 0;
  std::string::const_iterator iter= in_string.begin();
  while (iter < in_string.end())
  {
    length++;
    seq_length= sequence_length(*iter);
    iter += (seq_length > 0) ? seq_length : 1;
  }
  return length;
}

/**
 * How many logical characters does the given UTF-8 string occupy? Useful when
 * needing to calculate char length rather than byte length of a string
 * @param in_string string to measure
 * @return length in characters of given string
 */
static inline uint32_t char_length(const char *in_string)
{
  const std::string process_string(in_string);
  return char_length(process_string);
}


} /* namespace utf8 */
} /* namespace drizzled */

