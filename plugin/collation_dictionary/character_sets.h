/* - mode: c; c-basic-offset: 2; indent-tabs-mode: nil; -*-
 *  vim:expandtab:shiftwidth=2:tabstop=2:smarttab:
 *
 *  Copyright (C) 2010 Sun Microsystems, Inc.
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

#ifndef PLUGIN_COLLATION_DICTIONARY_CHARACTER_SETS_H
#define PLUGIN_COLLATION_DICTIONARY_CHARACTER_SETS_H

class CharacterSetsTool : public drizzled::plugin::TableFunction
{
public:

  CharacterSetsTool();

  CharacterSetsTool(const char *table_arg) :
    drizzled::plugin::TableFunction("DATA_DICTIONARY", table_arg)
  { }

  ~CharacterSetsTool() {}

  class Generator : public drizzled::plugin::TableFunction::Generator 
  {
    drizzled::CHARSET_INFO **character_set_iter;
    bool is_char_primed;
    bool nextCharacterSetCore();

  public:
    Generator(drizzled::Field **arg);

    bool populate();
    bool nextCharacterSet();
    bool checkCharacterSet();
    virtual void fill();

    drizzled::CHARSET_INFO const * character_set()
    {
      return character_set_iter[0];
    }

    bool isCharacterSetPrimed()
    {
      return is_char_primed;
    }

  };

  CharacterSetsTool::Generator *generator(drizzled::Field **arg)
  {
    return new Generator(arg);
  }
};

#endif /* PLUGIN_COLLATION_DICTIONARY_CHARACTER_SETS_H */
