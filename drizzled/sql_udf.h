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

#ifndef DRIZZLED_SQL_UDF_H
#define DRIZZLED_SQL_UDF_H

/* This file defines structures needed by udf functions */

#include <drizzled/function/func.h>
#include <drizzled/function/create.h>

#include <stdint.h>

enum Item_udftype {UDFTYPE_FUNCTION=1,UDFTYPE_AGGREGATE};

Function_builder *find_udf(const char *name, uint32_t len=0);
void free_udf(Function_builder *udf);
int mysql_create_function(Session *session,Function_builder *udf);
void add_udf(Function_builder *udf);
void remove_udf(Function_builder *udf);

#endif /* DRIZZLED_SQL_UDF_H */
