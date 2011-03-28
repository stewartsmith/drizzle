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

#pragma once

#include <cstddef>
#include <drizzled/definitions.h>

#include <drizzled/visibility.h>

namespace drizzled
{

struct charset_info_st;

extern DRIZZLED_API charset_info_st *all_charsets[256];
extern charset_info_st compiled_charsets[];

extern uint32_t get_charset_number(const char *cs_name, uint32_t cs_flags);
extern uint32_t get_collation_number(const char *name);
extern const char *get_charset_name(uint32_t cs_number);

DRIZZLED_API const charset_info_st *get_charset(uint32_t cs_number);
DRIZZLED_API const charset_info_st *get_charset_by_name(const char *cs_name);
DRIZZLED_API const charset_info_st *get_charset_by_csname(const char *cs_name, uint32_t cs_flags);

extern bool resolve_charset(const char *cs_name,
			    const charset_info_st *default_cs,
			    const charset_info_st **cs);
extern bool resolve_collation(const char *cl_name,
			     const charset_info_st *default_cl,
			     const charset_info_st **cl);

extern void free_charsets(void);
extern char *get_charsets_dir(char *buf);
extern bool my_charset_same(const charset_info_st *cs1, const charset_info_st *cs2);
extern bool init_compiled_charsets(myf flags);
extern void add_compiled_collation(charset_info_st *cs);
extern size_t escape_string_for_drizzle(const charset_info_st *charset_info,
					char *to, size_t to_length,
					const char *from, size_t length);
extern size_t escape_quotes_for_drizzle(const charset_info_st *charset_info,
					char *to, size_t to_length,
					const char *from, size_t length);

} /* namespace drizzled */

