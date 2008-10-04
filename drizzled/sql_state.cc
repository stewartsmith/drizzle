/* - mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; -*-
 *  vim:expandtab:shiftwidth=2:tabstop=2:smarttab:
 *
 *  Copyright (C) 2008 Sun Microsystems
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

/* Functions to map mysqld errno to sql_state */
#include <drizzled/global.h>
#include <drizzled/error.h>
#include <libdrizzle/libdrizzle.h>
#include "sql_state.h"

const char *drizzle_errno_to_sqlstate(uint mysql_errno)
{
  uint first=0, end= array_elements(sqlstate_map)-1;
  struct st_map_errno_to_sqlstate *map;

  /* Do binary search in the sorted array */
  while (first != end)
  {
    uint mid= (first+end)/2;
    map= sqlstate_map+mid;
    if (map->mysql_errno < mysql_errno)
      first= mid+1;
    else
      end= mid;
  }
  map= sqlstate_map+first;
  if (map->mysql_errno == mysql_errno)
    return map->odbc_state;
  return "HY000";				/* General error */
}
