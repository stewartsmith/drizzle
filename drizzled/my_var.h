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


#ifndef DRIZZLED_MY_VAR_H
#define DRIZZLED_MY_VAR_H

class my_var : public Sql_alloc  {
public:
  LEX_STRING s;
  bool local;
  uint32_t offset;
  enum_field_types type;
  my_var (LEX_STRING& j, bool i, uint32_t o, enum_field_types t)
    :s(j), local(i), offset(o), type(t)
  {}
  ~my_var() {}
};

#endif /* DRIZZLED_MY_VAR_H */
