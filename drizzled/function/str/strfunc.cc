/* Copyright (C) 2000-2006 MySQL AB

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


/**
  @file

  @brief
  This file defines all string functions

  @warning
    Some string functions don't always put and end-null on a String.
    (This shouldn't be needed)
*/

#include <config.h>
#include <zlib.h>
#include <drizzled/query_id.h>
#include <drizzled/error.h>
#include <drizzled/function/str/strfunc.h>

using namespace std;

namespace drizzled
{

Item_str_func::~Item_str_func() {}

bool Item_str_func::fix_fields(Session *session, Item **ref)
{
  bool res= Item_func::fix_fields(session, ref);
  /*
    In Item_str_func::check_well_formed_result() we may set null_value
    flag on the same condition as in test() below.
  */
  maybe_null= (maybe_null || true);
  return res;
}


type::Decimal *Item_str_func::val_decimal(type::Decimal *decimal_value)
{
  assert(fixed == 1);
  char buff[64];
  String *res, tmp(buff,sizeof(buff), &my_charset_bin);
  res= val_str(&tmp);
  if (not res)
    return 0;

  (void)decimal_value->store(E_DEC_FATAL_ERROR, (char*) res->ptr(), res->length(), res->charset());

  return decimal_value;
}


double Item_str_func::val_real()
{
  assert(fixed == 1);
  int err_not_used;
  char *end_not_used, buff[64];
  String *res, tmp(buff,sizeof(buff), &my_charset_bin);
  res= val_str(&tmp);
  return res ? my_strntod(res->charset(), (char*) res->ptr(), res->length(),
			  &end_not_used, &err_not_used) : 0.0;
}


int64_t Item_str_func::val_int()
{
  assert(fixed == 1);
  int err;
  char buff[DECIMAL_LONGLONG_DIGITS];
  String *res, tmp(buff,sizeof(buff), &my_charset_bin);
  res= val_str(&tmp);
  return (res ?
	  my_strntoll(res->charset(), res->ptr(), res->length(), 10, NULL,
		      &err) :
	  (int64_t) 0);
}

void Item_str_func::left_right_max_length()
{
  max_length=args[0]->max_length;
  if (args[1]->const_item())
  {
    int length=(int) args[1]->val_int()*collation.collation->mbmaxlen;
    if (length <= 0)
      max_length=0;
    else
      set_if_smaller(max_length,(uint) length);
  }
}

DRIZZLED_API String my_empty_string("",default_charset_info);

} /* namespace drizzled */
