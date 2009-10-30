/* Copyright (C) 2009 Sun Microsystems

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


#ifndef PLUGIN_INFORMATION_ENGINE_INFORMATION_SHARE_H
#define PLUGIN_INFORMATION_ENGINE_INFORMATION_SHARE_H

#include <drizzled/server_includes.h>
#include <string>

/*
  Shared class for correct LOCK operation
  TODO -> Fix to never remove/etc. We could generate all of these in startup if we wanted to.
  Tracking count? I'm not sure that is needed at all. We could possibly make this a member of
  engine as well (should we just hide the share detail?)
*/

class InformationCursor;

class InformationShare {
  uint32_t count;
  std::string name;

public:
  InformationShare(const char *arg) :
    count(1),
    name(arg)
  {
    thr_lock_init(&lock);
  };
  ~InformationShare() 
  {
    thr_lock_delete(&lock);
  }

  void inc(void) { count++; }
  uint32_t dec(void) { return --count; }

  static InformationShare *get(const char *table_name);
  static void free(InformationShare *share);
  static void start(void);
  static void stop(void);
  THR_LOCK lock;
};

#endif /* STORAGE_INFORMATION_ENGINE_INFORMATION_SOURCE_H */
