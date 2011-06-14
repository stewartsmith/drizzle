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

#pragma once

#ifdef HAVE_DTRACE
#include <drizzled/generated_probes.h>
#else
#define DRIZZLE_COMMAND_DONE(arg0)
#define DRIZZLE_COMMAND_DONE_ENABLED() (0)
#define DRIZZLE_COMMAND_START(arg0, arg1)
#define DRIZZLE_COMMAND_START_ENABLED() (0)
#define DRIZZLE_CONNECTION_DONE(arg0)
#define DRIZZLE_CONNECTION_DONE_ENABLED() (0)
#define DRIZZLE_CONNECTION_START(arg0)
#define DRIZZLE_CONNECTION_START_ENABLED() (0)
#define DRIZZLE_DELETE_DONE(arg0, arg1)
#define DRIZZLE_DELETE_DONE_ENABLED() (0)
#define DRIZZLE_DELETE_ROW_DONE(arg0)
#define DRIZZLE_DELETE_ROW_DONE_ENABLED() (0)
#define DRIZZLE_DELETE_ROW_START(arg0, arg1)
#define DRIZZLE_DELETE_ROW_START_ENABLED() (0)
#define DRIZZLE_DELETE_START(arg0)
#define DRIZZLE_DELETE_START_ENABLED() (0)
#define DRIZZLE_FILESORT_DONE(arg0, arg1)
#define DRIZZLE_FILESORT_DONE_ENABLED() (0)
#define DRIZZLE_FILESORT_START(arg0, arg1)
#define DRIZZLE_FILESORT_START_ENABLED() (0)
#define DRIZZLE_CURSOR_RDLOCK_DONE(arg0)
#define DRIZZLE_CURSOR_RDLOCK_DONE_ENABLED() (0)
#define DRIZZLE_CURSOR_RDLOCK_START(arg0, arg1)
#define DRIZZLE_CURSOR_RDLOCK_START_ENABLED() (0)
#define DRIZZLE_CURSOR_UNLOCK_DONE(arg0)
#define DRIZZLE_CURSOR_UNLOCK_DONE_ENABLED() (0)
#define DRIZZLE_CURSOR_UNLOCK_START(arg0, arg1)
#define DRIZZLE_CURSOR_UNLOCK_START_ENABLED() (0)
#define DRIZZLE_CURSOR_WRLOCK_DONE(arg0)
#define DRIZZLE_CURSOR_WRLOCK_DONE_ENABLED() (0)
#define DRIZZLE_CURSOR_WRLOCK_START(arg0, arg1)
#define DRIZZLE_CURSOR_WRLOCK_START_ENABLED() (0)
#define DRIZZLE_INSERT_DONE(arg0, arg1)
#define DRIZZLE_INSERT_DONE_ENABLED() (0)
#define DRIZZLE_INSERT_ROW_DONE(arg0)
#define DRIZZLE_INSERT_ROW_DONE_ENABLED() (0)
#define DRIZZLE_INSERT_ROW_START(arg0, arg1)
#define DRIZZLE_INSERT_ROW_START_ENABLED() (0)
#define DRIZZLE_INSERT_SELECT_DONE(arg0, arg1)
#define DRIZZLE_INSERT_SELECT_DONE_ENABLED() (0)
#define DRIZZLE_INSERT_SELECT_START(arg0)
#define DRIZZLE_INSERT_SELECT_START_ENABLED() (0)
#define DRIZZLE_INSERT_START(arg0)
#define DRIZZLE_INSERT_START_ENABLED() (0)
#define DRIZZLE_QUERY_DONE(arg0)
#define DRIZZLE_QUERY_DONE_ENABLED() (0)
#define DRIZZLE_QUERY_EXEC_DONE(arg0)
#define DRIZZLE_QUERY_EXEC_DONE_ENABLED() (0)
#define DRIZZLE_QUERY_EXEC_START(arg0, arg1, arg2)
#define DRIZZLE_QUERY_EXEC_START_ENABLED() (0)
#define DRIZZLE_QUERY_OPT_START(arg0, arg1)
#define DRIZZLE_QUERY_OPT_START_ENABLED() (0)
#define DRIZZLE_QUERY_OPT_DONE(arg0)
#define DRIZZLE_QUERY_OPT_DONE_ENABLED() (0)
#define DRIZZLE_QUERY_OPT_CHOOSE_PLAN_START(arg0, arg1)
#define DRIZZLE_QUERY_OPT_CHOOSE_PLAN_START_ENABLED() (0)
#define DRIZZLE_QUERY_OPT_CHOOSE_PLAN_DONE(arg0)
#define DRIZZLE_QUERY_OPT_CHOOSE_PLAN_DONE_ENABLED() (0)
#define DRIZZLE_QUERY_PARSE_DONE(arg0)
#define DRIZZLE_QUERY_PARSE_DONE_ENABLED() (0)
#define DRIZZLE_QUERY_PARSE_START(arg0)
#define DRIZZLE_QUERY_PARSE_START_ENABLED() (0)
#define DRIZZLE_QUERY_START(arg0, arg1, arg2)
#define DRIZZLE_QUERY_START_ENABLED() (0)
#define DRIZZLE_SELECT_DONE(arg0, arg1)
#define DRIZZLE_SELECT_DONE_ENABLED() (0)
#define DRIZZLE_SELECT_START(arg0)
#define DRIZZLE_SELECT_START_ENABLED() (0)
#define DRIZZLE_UPDATE_DONE(arg0, arg1, arg2)
#define DRIZZLE_UPDATE_DONE_ENABLED() (0)
#define DRIZZLE_UPDATE_ROW_DONE(arg0)
#define DRIZZLE_UPDATE_ROW_DONE_ENABLED() (0)
#define DRIZZLE_UPDATE_ROW_START(arg0, arg1)
#define DRIZZLE_UPDATE_ROW_START_ENABLED() (0)
#define DRIZZLE_UPDATE_START(arg0)
#define DRIZZLE_UPDATE_START_ENABLED() (0)
#endif

