/* -*- mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; -*-
 *  vim:expandtab:shiftwidth=2:tabstop=2:smarttab:
 *
 *  Copyright (C) 2008 Mark Atwood
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

#ifndef DRIZZLED_LOGGING_H
#define DRIZZLED_LOGGING_H

#include <drizzled/plugin/logging_handler.h>

/* there are no parameters other than the session because logging can
 * pull everything it needs out of the session.  If need to add
 * parameters, look at how errmsg.h and errmsg.cc do it. */

bool logging_pre_do (Session *session);
bool logging_post_do (Session *session);
void add_logger(Logging_handler *handler);
void remove_logger(Logging_handler *handler);

#endif /* DRIZZLED_LOGGING_H */
