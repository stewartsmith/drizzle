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

#include <drizzled/select_to_file.h>

/*
 List of all possible characters of a numeric value text representation.
*/
#define NUMERIC_CHARS ".0123456789e+-"

namespace drizzled
{

class select_export :
  public select_to_file
{
  uint32_t field_term_length;
  int field_sep_char,escape_char,line_sep_char;
  int field_term_char; // first char of FIELDS TERMINATED BY or MAX_INT
  /*
    The is_ambiguous_field_sep field is true if a value of the field_sep_char
    field is one of the 'n', 't', 'r' etc characters
    (see the READ_INFO::unescape method and the ESCAPE_CHARS constant value).
  */
  bool is_ambiguous_field_sep;
  /*
     The is_ambiguous_field_term is true if field_sep_char contains the first
     char of the FIELDS TERMINATED BY (ENCLOSED BY is empty), and items can
     contain this character.
  */
  bool is_ambiguous_field_term;
  /*
    The is_unsafe_field_sep field is true if a value of the field_sep_char
    field is one of the '0'..'9', '+', '-', '.' and 'e' characters
    (see the NUMERIC_CHARS constant value).
  */
  bool is_unsafe_field_sep;
  bool fixed_row_size;
public:
  select_export(file_exchange *ex) :
    select_to_file(ex),
    field_term_length(0),
    field_sep_char(0),
    escape_char(0),
    line_sep_char(0),
    field_term_char(0),
    is_ambiguous_field_sep(0),
    is_ambiguous_field_term(0),
    is_unsafe_field_sep(0),
    fixed_row_size(0)
  {}
  ~select_export();
  int prepare(List<Item> &list, Select_Lex_Unit *u);
  bool send_data(List<Item> &items);
private:
  inline bool needs_escaping(char character, bool enclosed)
  {
    if ((character == escape_char) ||
        (enclosed ? character == field_sep_char : character == field_term_char) ||
        character == line_sep_char  ||
        (character == 0))
      return true;

    return false;

  }
};

} /* namespace drizzled */

