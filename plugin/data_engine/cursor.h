/* - mode: c; c-basic-offset: 2; indent-tabs-mode: nil; -*-
 *  vim:expandtab:shiftwidth=2:tabstop=2:smarttab:
 *
 *  Copyright (C) 2009 Sun Microsystems
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

#ifndef PLUGIN_DATA_ENGINE_CURSOR_H
#define PLUGIN_DATA_ENGINE_CURSOR_H

#include <drizzled/cursor.h>

#include <plugin/data_engine/function.h>

class FunctionCursor: public Cursor
{
  Tool *tool;
  Tool::Generator *generator;
  size_t record_id;
  std::vector<unsigned char *> row_cache;

public:
  FunctionCursor(drizzled::plugin::StorageEngine &engine, TableShare &table_arg);
  ~FunctionCursor() {}

  int open(const char *name, int mode, uint32_t test_if_locked);

  int close(void);

  int rnd_init(bool scan);

  /* get the next row and copy it into buf */
  int rnd_next(unsigned char *buf);

  /* locate row pointed to by pos, copy it into buf */
  int rnd_pos(unsigned char *buf, unsigned char *pos);

  int rnd_end();

  /* record position of a record for reordering */
  void position(const unsigned char *record);

  int info(uint32_t flag);
};

#endif /* PLUGIN_DATA_ENGINE_DATA_CURSOR_H */
