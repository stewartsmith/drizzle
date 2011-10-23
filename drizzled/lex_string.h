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

#include <cstddef>
#include <drizzled/util/data_ref.h>

namespace drizzled {

/*
  lex_string_t -- a pair of a C-string and its length.
*/

/* This definition must match the one given in mysql/plugin.h */
struct lex_string_t
{
  const char* begin() const
  {
    return data();
  }

  const char* end() const
  {
    return data() + size();
  }

  const char* data() const
  {
    return str_;
  }

  size_t size() const
  {
    return length_;
  }

  void assign(const char* d, size_t s)
  {
    str_= const_cast<char*>(d);
    length_ = s;
  }

  void operator=(str_ref v)
  {
    assign(v.data(), v.size());
  }

  char* str_;
  size_t length_;
};

inline const lex_string_t &null_lex_string()
{
  static lex_string_t tmp= { NULL, 0 };
  return tmp;
}

struct execute_string_t : public lex_string_t
{
private:
  bool is_variable;
public:

  bool isVariable() const
  {
    return is_variable;
  }

  void set(const lex_string_t& s, bool is_variable_arg= false)
  {
    is_variable= is_variable_arg;
    static_cast<lex_string_t&>(*this) = s;
  }

};

#define STRING_WITH_LEN(X) (X), (sizeof(X) - 1)
#define C_STRING_WITH_LEN(X) const_cast<char*>(X), (sizeof(X) - 1)

} /* namespace drizzled */

