/* -*- mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; -*-
 *  vim:expandtab:shiftwidth=2:tabstop=2:smarttab:
 *
 *  Copyright (C) 2009 Sun Microsystems, Inc.
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
#include <drizzled/show.h>
#include <drizzled/session.h>
#include <drizzled/lock.h>
#include <drizzled/statement/unlock_tables.h>

namespace drizzled
{

bool statement::UnlockTables::execute()
{
  /*
     It is critical for mysqldump --single-transaction --master-data that
     UNLOCK TABLES does not implicitely commit a connection which has only
     done FLUSH TABLES WITH READ LOCK + BEGIN. If this assumption becomes
     false, mysqldump will not work.
   */
  if (session().isGlobalReadLock())
  {
    session().unlockGlobalReadLock();
    session().my_ok();
  }
  else
  {
    my_error(ER_NO_LOCK_HELD, MYF(0));
  }

  return false;
}

} /* namespace drizzled */

