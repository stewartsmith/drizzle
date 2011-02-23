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

#include <config.h>
#include <plugin/user_locks/module.h>

using namespace drizzled;

static int init(drizzled::module::Context &context)
{
  // We are single threaded at this point, so we can use this to initialize.
  user_locks::Locks::getInstance();
  user_locks::barriers::Barriers::getInstance();

  context.add(new plugin::Create_function<user_locks::barriers::CreateBarrier>("create_barrier"));
  context.add(new plugin::Create_function<user_locks::barriers::Release>("release_barrier"));
  context.add(new plugin::Create_function<user_locks::barriers::Wait>("wait"));
  context.add(new plugin::Create_function<user_locks::barriers::WaitUntil>("wait_until"));
  context.add(new plugin::Create_function<user_locks::barriers::Signal>("signal"));

  context.add(new plugin::Create_function<user_locks::GetLock>("get_lock"));
  context.add(new plugin::Create_function<user_locks::GetLocks>("get_locks"));
  context.add(new plugin::Create_function<user_locks::ReleaseLock>("release_lock"));
  context.add(new plugin::Create_function<user_locks::ReleaseLocks>("release_locks"));
  context.add(new plugin::Create_function<user_locks::IsFreeLock>("is_free_lock"));
  context.add(new plugin::Create_function<user_locks::IsUsedLock>("is_used_lock"));
  context.add(new plugin::Create_function<user_locks::locks::WaitFor>("wait_for_lock"));
  context.add(new plugin::Create_function<user_locks::locks::ReleaseAndWait>("release_lock_and_wait"));
  context.add(new user_locks::UserLocks);
  context.add(new user_locks::barriers::UserBarriers);

  return 0;
}

DRIZZLE_DECLARE_PLUGIN
{
  DRIZZLE_VERSION_ID,
  "User Level Locking Functions",
  "1.1",
  "Brian Aker",
  "User level locking and barrier functions",
  PLUGIN_LICENSE_GPL,
  init,
  NULL,
  NULL
}
DRIZZLE_DECLARE_PLUGIN_END;
