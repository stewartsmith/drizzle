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

#pragma once


class IndexPartsTool : public IndexesTool
{
public:
  IndexPartsTool();

  class Generator : public IndexesTool::Generator 
  {
    int32_t index_part_iterator;
    bool is_index_part_primed;
    drizzled::message::Table::Index::IndexPart index_part;

    void fill();

  public:
    Generator(drizzled::Field **arg);

    bool populate();

    bool nextIndexPartsCore();
    bool nextIndexParts();

  };

  Generator *generator(drizzled::Field **arg)
  {
    return new Generator(arg);
  }
};

