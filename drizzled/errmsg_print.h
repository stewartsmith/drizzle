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

/* This file and these functions are a stopgap until all the
   sql_print_foo() function calls are replaced with calls to
   errmsg_printf()
*/

#ifndef DRIZZLED_ERRMSG_PRINT_H
#define DRIZZLED_ERRMSG_PRINT_H

void sql_print_error (const char *format, ...);
void sql_print_warning (const char *format, ...);
void sql_print_information (const char *format, ...);
void sql_perror (const char *message);

#endif /* DRIZZLED_ERRMSG_PRINT_H */


