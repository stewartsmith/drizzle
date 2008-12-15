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

#include <drizzled/server_includes.h>
#include CSTDINT_H
#include <drizzled/function/str/reverse.h>

String *Item_func_reverse::val_str(String *str)
{
  assert(fixed == 1);
  String *res = args[0]->val_str(str);
  char *ptr, *end, *tmp;

  if ((null_value=args[0]->null_value))
    return 0;
  /* An empty string is a special case as the string pointer may be null */
  if (!res->length())
    return &my_empty_string;
  if (tmp_value.alloced_length() < res->length() &&
      tmp_value.realloc(res->length()))
  {
    null_value= 1;
    return 0;
  }
  tmp_value.length(res->length());
  tmp_value.set_charset(res->charset());
  ptr= (char *) res->ptr();
  end= ptr + res->length();
  tmp= (char *) tmp_value.ptr() + tmp_value.length();
#ifdef USE_MB
  if (use_mb(res->charset()))
  {
    register uint32_t l;
    while (ptr < end)
    {
      if ((l= my_ismbchar(res->charset(),ptr,end)))
      {
        tmp-= l;
        memcpy(tmp,ptr,l);
        ptr+= l;
      }
      else
        *--tmp= *ptr++;
    }
  }
  else
#endif /* USE_MB */
  {
    while (ptr < end)
      *--tmp= *ptr++;
  }
  return &tmp_value;
}


void Item_func_reverse::fix_length_and_dec()
{
  collation.set(args[0]->collation);
  max_length = args[0]->max_length;
}
