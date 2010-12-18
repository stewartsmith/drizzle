/* -*- mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; -*-
 *  vim:expandtab:shiftwidth=2:tabstop=2:smarttab:
 *
 *  Copyright (C) 2008 Sun Microsystems, Inc.
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

#include "config.h"
#include <drizzled/current_session.h>

namespace drizzled
{

static MySessionVar THR_Session;
static MyMemoryRootVar THR_Mem_root;

MySessionVar &currentSession(void)
{
  return THR_Session;
}

MyMemoryRootVar &currentMemRoot(void)
{
  return THR_Mem_root;
}

Session *_current_session(void)
{
  return THR_Session.get();
}

memory::Root *current_mem_root(void)
{
  return *(currentMemRoot().get());
}

} /* namespace drizzled */
