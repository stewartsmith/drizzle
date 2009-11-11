/* Copyright (C) 2005 MySQL AB

  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation; version 2 of the License.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA */


#ifndef PLUGIN_INFORMATION_ENGINE_INFORMATION_CURSOR_H
#define PLUGIN_INFORMATION_ENGINE_INFORMATION_CURSOR_H

#include <drizzled/server_includes.h>
#include <drizzled/cursor.h>
#include <mysys/thr_lock.h>

#include "information_share.h"

class InformationCursor: public Cursor
{
  THR_LOCK_DATA lock;      /* MySQL lock */
  InformationShare *share;
  drizzled::plugin::InfoSchemaTable::Rows::iterator iter;

public:
  InformationCursor(drizzled::plugin::StorageEngine *engine, TableShare *table_arg);
  ~InformationCursor() {}

  /*
    The name of the index type that will be used for display
    don't implement this method unless you really have indexes
  */
  uint64_t table_flags() const
  {
    return 0;
  }
  uint32_t index_flags(uint32_t inx, uint32_t part, bool all_parts) const;
  /* The following defines can be increased if necessary */
  int open(const char *name, int mode, uint32_t test_if_locked);
  int close(void);
  int rnd_init(bool scan);
  int rnd_next(unsigned char *buf);
  int rnd_pos(unsigned char * buf, unsigned char *pos);
  void position(const unsigned char *record);
  int info(uint32_t flag);
  THR_LOCK_DATA **store_lock(Session *session,
                             THR_LOCK_DATA **to,
                             enum thr_lock_type lock_type);
};

#endif /* PLUGIN_INFORMATION_ENGINE_INFORMATION_CURSOR_H */
