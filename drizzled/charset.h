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

#ifndef DRIZZLED_CHARSET_H
#define DRIZZLED_CHARSET_H

#include <cstddef>
#include "drizzled/definitions.h"

namespace drizzled
{

typedef struct charset_info_st CHARSET_INFO;

extern CHARSET_INFO *all_charsets[256];
extern CHARSET_INFO compiled_charsets[];

/* character sets */
void *cs_alloc(size_t size);

extern uint32_t get_charset_number(const char *cs_name, uint32_t cs_flags);
extern uint32_t get_collation_number(const char *name);
extern const char *get_charset_name(uint32_t cs_number);

extern const CHARSET_INFO *get_charset(uint32_t cs_number);
extern const CHARSET_INFO *get_charset_by_name(const char *cs_name);
extern const CHARSET_INFO *get_charset_by_csname(const char *cs_name, uint32_t cs_flags);

extern bool resolve_charset(const char *cs_name,
			    const CHARSET_INFO *default_cs,
			    const CHARSET_INFO **cs);
extern bool resolve_collation(const char *cl_name,
			     const CHARSET_INFO *default_cl,
			     const CHARSET_INFO **cl);

extern void free_charsets(void);
extern char *get_charsets_dir(char *buf);
extern bool my_charset_same(const CHARSET_INFO *cs1, const CHARSET_INFO *cs2);
extern bool init_compiled_charsets(myf flags);
extern void add_compiled_collation(CHARSET_INFO *cs);
extern size_t escape_string_for_drizzle(const CHARSET_INFO *charset_info,
					char *to, size_t to_length,
					const char *from, size_t length);
extern size_t escape_quotes_for_drizzle(const CHARSET_INFO *charset_info,
					char *to, size_t to_length,
					const char *from, size_t length);
/* character sets */
void *cs_alloc(size_t size);

extern uint32_t get_charset_number(const char *cs_name, uint32_t cs_flags);
extern uint32_t get_collation_number(const char *name);
extern const char *get_charset_name(uint32_t cs_number);

extern const CHARSET_INFO *get_charset(uint32_t cs_number);
extern const CHARSET_INFO *get_charset_by_name(const char *cs_name);
extern const CHARSET_INFO *get_charset_by_csname(const char *cs_name, uint32_t cs_flags);

extern bool resolve_charset(const char *cs_name,
			    const CHARSET_INFO *default_cs,
			    const CHARSET_INFO **cs);
extern bool resolve_collation(const char *cl_name,
			     const CHARSET_INFO *default_cl,
			     const CHARSET_INFO **cl);

extern void free_charsets(void);
extern char *get_charsets_dir(char *buf);
extern bool my_charset_same(const CHARSET_INFO *cs1, const CHARSET_INFO *cs2);
extern bool init_compiled_charsets(myf flags);
extern void add_compiled_collation(CHARSET_INFO *cs);
extern size_t escape_string_for_drizzle(const CHARSET_INFO *charset_info,
					char *to, size_t to_length,
					const char *from, size_t length);
extern size_t escape_quotes_for_drizzle(const CHARSET_INFO *charset_info,
					char *to, size_t to_length,
					const char *from, size_t length);

} /* namespace drizzled */

#endif /* DRIZZLED_CHARSET_H */
