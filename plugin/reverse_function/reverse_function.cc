/* -*- mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; -*-
 *  vim:expandtab:shiftwidth=2:tabstop=2:smarttab:
 *
 *  Copyright (C) 2008 Sun Microsystems, Inc.
 *  Copyright (C) 2010 Stewart Smith
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

#include <config.h>

#include <drizzled/plugin/function.h>
#include <drizzled/function/str/strfunc.h>
#include <drizzled/charset.h>

using namespace drizzled;

class ReverseFunction :public Item_str_func
{
  String tmp_value;
public:
  ReverseFunction() :Item_str_func() {}
  String *val_str(String *);
  void fix_length_and_dec();
  const char *func_name() const { return "reverse"; }

  bool check_argument_count(int n)
  {
    return n == 1;
  }
};

String *ReverseFunction::val_str(String *str)
{
  assert(fixed == 1);
  String *res = args[0]->val_str(str);
  char *ptr, *end, *tmp;

  if ((null_value=args[0]->null_value))
    return 0;
  /* An empty string is a special case as the string pointer may be null */
  if (!res->length())
    return &my_empty_string;
  if (tmp_value.alloced_length() < res->length())
      tmp_value.realloc(res->length());
  tmp_value.length(res->length());
  tmp_value.set_charset(res->charset());
  ptr= (char *) res->ptr();
  end= ptr + res->length();
  tmp= (char *) tmp_value.ptr() + tmp_value.length();
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
  {
    while (ptr < end)
      *--tmp= *ptr++;
  }
  return &tmp_value;
}

void ReverseFunction::fix_length_and_dec()
{
  collation.set(args[0]->collation);
  max_length = args[0]->max_length;
}

plugin::Create_function<ReverseFunction> *reverse_function= NULL;

static int initialize(drizzled::module::Context &context)
{
  reverse_function= new plugin::Create_function<ReverseFunction>("reverse");
  context.add(reverse_function);
  return 0;
}

DRIZZLE_DECLARE_PLUGIN
{
  DRIZZLE_VERSION_ID,
  "reverse_function",
  "1.0",
  "Stewart Smith",
  "reverses a string",
  PLUGIN_LICENSE_GPL,
  initialize, /* Plugin Init */
  NULL,   /* depends */
  NULL    /* config options */
}
DRIZZLE_DECLARE_PLUGIN_END;
