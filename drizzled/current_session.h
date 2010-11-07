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


#ifndef DRIZZLED_CURRENT_SESSION_H
#define DRIZZLED_CURRENT_SESSION_H

#include <pthread.h>
#include <boost/thread/tss.hpp>

namespace drizzled
{

class Session;

namespace memory { class Root; }

Session *_current_session(void);
#define current_session ::drizzled::_current_session()
memory::Root *current_mem_root(void);

typedef boost::thread_specific_ptr<Session> MySessionVar;
typedef boost::thread_specific_ptr<memory::Root *> MyMemoryRootVar;

MySessionVar &currentSession(void);
MyMemoryRootVar &currentMemRoot(void);

} /* namespace drizzled */

#endif /* DRIZZLED_CURRENT_SESSION_H */
