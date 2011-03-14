/* - mode: c; c-basic-offset: 2; indent-tabs-mode: nil; -*-
 *  vim:expandtab:shiftwidth=2:tabstop=2:smarttab:
 *
 *  Copyright (C) 2010 Brian Aker
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

class ShowTableStatus : public  show_dictionary::Show
{
public:
  ShowTableStatus();

  class Generator : public show_dictionary::Show::Generator 
  {
    bool is_primed;
    drizzled::Table *table;
    std::string schema_predicate;
    std::vector<drizzled::Table *> table_list;
    std::vector<drizzled::Table *>::iterator table_list_iterator;
    boost::mutex::scoped_lock scopedLock;

    void fill();

    const char *schema_name();
    bool checkSchemaName();

    bool nextCore();
    bool next();

  public:
    bool populate();

    Generator(drizzled::Field **arg);
    ~Generator();
  };

  Generator *generator(drizzled::Field **arg)
  {
    return new Generator(arg);
  }
};

