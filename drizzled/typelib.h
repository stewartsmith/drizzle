/* Copyright (C) 2000 MySQL AB

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA */


#ifndef DRIZZLED_TYPELIB_H
#define DRIZZLED_TYPELIB_H

#include <drizzled/global_charset_info.h>
#include <drizzled/memory/root.h>

namespace drizzled
{

typedef struct st_typelib 
{
public:
  st_typelib *copy_typelib(memory::Root *root) const;
  int find_type_or_exit(const char *x, const char *option) const;
  int find_type(const char *x, unsigned int full_name) const;
  int find_type(char *x, unsigned int full_name) const;
  uint64_t find_typeset(const char *x, int *error_position) const;
  const char *get_type(unsigned int nr) const;
  void make_type(char *to, unsigned int nr) const;

  uint64_t find_set(const char *x, uint32_t length, const CHARSET_INFO *cs,
                    char **err_pos, uint32_t *err_len, bool *set_warning) const;
  uint32_t find_type(const char *find, uint32_t length, bool part_match) const;
  uint32_t find_type2(const char *find, uint32_t length, const CHARSET_INFO *cs) const;

  unsigned int count;
  const char *name;
  const char **type_names;
  unsigned int *type_lengths;
} TYPELIB;

} /* namespace drizzled */

#endif /* DRIZZLED_TYPELIB_H */
