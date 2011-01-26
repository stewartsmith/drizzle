/* -*- mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; -*-
 *  vim:expandtab:shiftwidth=2:tabstop=2:smarttab:
 *
 *  Copyright (C) 2009 Sun Microsystems, Inc.
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

#ifndef DRIZZLED_PTHREAD_GLOBALS_H
#define DRIZZLED_PTHREAD_GLOBALS_H

#include <pthread.h>
#include <boost/thread/recursive_mutex.hpp>
#include <boost/thread/mutex.hpp>
#include <boost/thread/shared_mutex.hpp>
#include <boost/thread/condition_variable.hpp>
#include <boost/shared_ptr.hpp>

namespace drizzled
{

typedef boost::unique_lock<boost::mutex>  boost_unique_lock_t;

typedef boost::shared_ptr<boost::thread> boost_thread_shared_ptr;

extern boost::condition_variable_any COND_refresh;
extern boost::condition_variable COND_thread_count;
extern pthread_t signal_thread;

} /* namespace drizzled */

#endif /* DRIZZLED_PTHREAD_GLOBALS_H */
