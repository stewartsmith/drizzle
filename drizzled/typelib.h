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

#include "drizzled/memory/root.h"

namespace drizzled
{

typedef struct st_typelib 
{
public:
  int find_type_or_exit(const char *x, const char *option) const;
  int find_type(char *x, unsigned int full_name) const;

  unsigned int count;
  const char *name;
  const char **type_names;
  unsigned int *type_lengths;
} TYPELIB;

extern uint64_t find_typeset(char *x, TYPELIB *typelib,int *error_position);
extern void make_type(char *to,unsigned int nr,TYPELIB *typelib);
extern const char *get_type(TYPELIB *typelib,unsigned int nr);
extern TYPELIB *copy_typelib(memory::Root *root, TYPELIB *from);

} /* namespace drizzled */

#endif /* DRIZZLED_TYPELIB_H */
