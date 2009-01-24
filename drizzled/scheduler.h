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

#ifndef DRIZZLE_SERVER_SCHEDULER_H
#define DRIZZLE_SERVER_SCHEDULER_H

/*
  Classes for the thread scheduler
*/

#include <mysys/my_list.h>

/* Forward declarations */
class Session;
struct event;

/* Functions used when manipulating threads */

class scheduler_functions
{
public:
  uint32_t max_threads;
  bool (*init_new_connection_thread)(void);
  void (*add_connection)(Session *session);
  void (*post_kill_notification)(Session *session);
  bool (*end_thread)(Session *session, bool cache_thread);
  scheduler_functions();
};

#endif /* DRIZZLE_SERVER_SCHEDULER_H */
