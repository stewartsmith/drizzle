/* - mode: c; c-basic-offset: 2; indent-tabs-mode: nil; -*-
 *  vim:expandtab:shiftwidth=2:tabstop=2:smarttab:
 *
 *  Copyright (C) 2010 Joseph Daly <skinny.moey@gmail.com> 
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

#include <drizzled/plugin/table_function.h>
#include <drizzled/field.h>

class Recorder {
  std::vector<std::string> string_list;
  std::vector<std::string>::iterator iterator;
public:

  void start()
  {
    iterator= string_list.begin();
  }

  void push(const char *arg)
  {
    string_list.push_back(arg);
  }

  bool next(std::string &arg)
  {
    if (iterator == string_list.end())
      return false;

    arg= *iterator;

    iterator++;

    return true;
  }

};

class InnodbInternalTables : public drizzled::plugin::TableFunction
{
public:
  InnodbInternalTables();

  class Generator : public drizzled::plugin::TableFunction::Generator
  {
    Recorder recorder;

  public:
    Generator(drizzled::Field **arg);
                        
    bool populate();
  private:
  };

  Generator *generator(drizzled::Field **arg)
  {
    return new Generator(arg);
  }
};

