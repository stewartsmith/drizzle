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

#ifndef drizzle_innodb_plugin_extras_h
#define drizzle_innodb_plugin_extras_h

#include <drizzled/plugin.h>
#include <mystrings/m_string.h>

#if defined(__cplusplus)
extern "C" {
#endif

/**
  Create a LEX_STRING in this connection's local memory pool

  @param thd      user thread connection handle
  @param lex_str  pointer to LEX_STRING object to be initialized
  @param str      initializer to be copied into lex_str
  @param size     length of str, in bytes
  @param allocate_lex_string  flag: if TRUE, allocate new LEX_STRING object,
                              instead of using lex_str value
  @return  NULL on failure, or pointer to the LEX_STRING object

  @see thd_alloc()
*/
LEX_STRING *thd_make_lex_string(Session *thd, LEX_STRING *lex_str,
                                const char *str, unsigned int size,
                                int allocate_lex_string);

#if defined(__cplusplus)
}
#endif
#endif
