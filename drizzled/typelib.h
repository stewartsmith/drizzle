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


#pragma once

#include <drizzled/common_fwd.h>

namespace drizzled {

class TYPELIB
{
public:
  enum e_find_options
  {
    e_none = 0,
    e_match_full = 1,
    e_dont_complete = 2,

    e_default = 3
  };

  TYPELIB* copy_typelib(memory::Root&) const;
  int find_type_or_exit(const char*, const char* option) const;
  int find_type(const char*, e_find_options) const;
  const char *get_type(unsigned int nr) const;

  uint64_t find_set(const char *x, uint32_t length, const charset_info_st*,
                    char **err_pos, uint32_t *err_len, bool *set_warning) const;
  uint32_t find_type(const char *find, uint32_t length, bool part_match) const;
  uint32_t find_type2(const char *find, uint32_t length, const charset_info_st*) const;

  unsigned int count;
  const char *name;
  const char **type_names;
  unsigned int *type_lengths;
};

} /* namespace drizzled */

