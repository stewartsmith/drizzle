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

#include <stddef.h>

namespace drizzled
{

/*
  LEX_STRING -- a pair of a C-string and its length.
*/

/* This definition must match the one given in mysql/plugin.h */
typedef struct lex_string_t
{
  char *str;
  size_t length;
} LEX_STRING;

inline const LEX_STRING &null_lex_string()
{
  static LEX_STRING tmp= { NULL, 0 };
  return tmp;
}

#define NULL_LEX_STRING null_lex_string()

struct execute_string_t : public lex_string_t
{
private:
  bool is_variable;
public:

  bool isVariable() const
  {
    return is_variable;
  }

  void set(const lex_string_t& ptr, bool is_variable_arg= false)
  {
    is_variable= is_variable_arg;
    str= ptr.str;
    length= ptr.length;
  }

};


#define STRING_WITH_LEN(X) (X), (static_cast<size_t>((sizeof(X) - 1)))
#define C_STRING_WITH_LEN(X) (const_cast<char *>((X))), (static_cast<size_t>((sizeof(X) - 1)))

} /* namespace drizzled */

