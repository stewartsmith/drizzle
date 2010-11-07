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

#ifndef DRIZZLED_STRFUNC_H
#define DRIZZLED_STRFUNC_H

namespace drizzled
{

typedef struct charset_info_st CHARSET_INFO;
typedef struct st_typelib TYPELIB;

uint64_t find_set(TYPELIB *lib, const char *x, uint32_t length,
                  const CHARSET_INFO * const cs,
                  char **err_pos, uint32_t *err_len, bool *set_warning);
uint32_t find_type(const TYPELIB *lib, const char *find, uint32_t length,
                   bool part_match);
uint32_t find_type2(const TYPELIB *lib, const char *find, uint32_t length,
                    const CHARSET_INFO *cs);

} /* namespace drizzled */

#endif /* DRIZZLED_STRFUNC_H */
