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
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA */


/**
  @file

  @brief
  This file defines all string functions

  @warning
    Some string functions don't always put and end-null on a String.
    (This shouldn't be needed)
*/

#include <drizzled/server_includes.h>
#include <mysys/sha1.h>
#include <zlib.h>
#include <drizzled/query_id.h>
#include <drizzled/data_home.h>
#include <drizzled/error.h>

#include <mysys/my_static.h>
/*
#include CMATH_H */

#if defined(CMATH_NAMESPACE)
using namespace CMATH_NAMESPACE;
#endif

using namespace std;

String my_empty_string("",default_charset_info);

String *Item_func_conv_charset::val_str(String *str)
{
  assert(fixed == 1);
  if (use_cached_value)
    return null_value ? 0 : &str_value;
  String *arg= args[0]->val_str(str);
  uint32_t dummy_errors;
  if (!arg)
  {
    null_value=1;
    return 0;
  }
  null_value= str_value.copy(arg->ptr(),arg->length(),arg->charset(),
                             conv_charset, &dummy_errors);
  return null_value ? 0 : check_well_formed_result(&str_value);
}

void Item_func_conv_charset::fix_length_and_dec()
{
  collation.set(conv_charset, DERIVATION_IMPLICIT);
  max_length = args[0]->max_length*conv_charset->mbmaxlen;
}

void Item_func_conv_charset::print(String *str, enum_query_type query_type)
{
  str->append(STRING_WITH_LEN("convert("));
  args[0]->print(str, query_type);
  str->append(STRING_WITH_LEN(" using "));
  str->append(conv_charset->csname);
  str->append(')');
}

String *Item_func_set_collation::val_str(String *str)
{
  assert(fixed == 1);
  str=args[0]->val_str(str);
  if ((null_value=args[0]->null_value))
    return 0;
  str->set_charset(collation.collation);
  return str;
}

void Item_func_set_collation::fix_length_and_dec()
{
  const CHARSET_INFO *set_collation;
  const char *colname;
  String tmp, *str= args[1]->val_str(&tmp);
  colname= str->c_ptr();
  if (colname == binary_keyword)
    set_collation= get_charset_by_csname(args[0]->collation.collation->csname,
					 MY_CS_BINSORT,MYF(0));
  else
  {
    if (!(set_collation= get_charset_by_name(colname,MYF(0))))
    {
      my_error(ER_UNKNOWN_COLLATION, MYF(0), colname);
      return;
    }
  }

  if (!set_collation || 
      !my_charset_same(args[0]->collation.collation,set_collation))
  {
    my_error(ER_COLLATION_CHARSET_MISMATCH, MYF(0),
             colname, args[0]->collation.collation->csname);
    return;
  }
  collation.set(set_collation, DERIVATION_EXPLICIT,
                args[0]->collation.repertoire);
  max_length= args[0]->max_length;
}


bool Item_func_set_collation::eq(const Item *item, bool binary_cmp) const
{
  /* Assume we don't have rtti */
  if (this == item)
    return 1;
  if (item->type() != FUNC_ITEM)
    return 0;
  Item_func *item_func=(Item_func*) item;
  if (arg_count != item_func->arg_count ||
      functype() != item_func->functype())
    return 0;
  Item_func_set_collation *item_func_sc=(Item_func_set_collation*) item;
  if (collation.collation != item_func_sc->collation.collation)
    return 0;
  for (uint32_t i=0; i < arg_count ; i++)
    if (!args[i]->eq(item_func_sc->args[i], binary_cmp))
      return 0;
  return 1;
}


void Item_func_set_collation::print(String *str, enum_query_type query_type)
{
  str->append('(');
  args[0]->print(str, query_type);
  str->append(STRING_WITH_LEN(" collate "));
  assert(args[1]->basic_const_item() &&
              args[1]->type() == Item::STRING_ITEM);
  args[1]->str_value.print(str);
  str->append(')');
}

String *Item_func_collation::val_str(String *str)
{
  assert(fixed == 1);
  uint32_t dummy_errors;
  const CHARSET_INFO * const cs= args[0]->collation.collation; 

  null_value= 0;
  str->copy(cs->name, strlen(cs->name),
	    &my_charset_utf8_general_ci, collation.collation, &dummy_errors);
  return str;
}


void Item_func_weight_string::fix_length_and_dec()
{
  const CHARSET_INFO * const cs= args[0]->collation.collation;
  collation.set(&my_charset_bin, args[0]->collation.derivation);
  flags= my_strxfrm_flag_normalize(flags, cs->levels_for_order);
  max_length= cs->mbmaxlen * cmax(args[0]->max_length, nweights);
  maybe_null= 1;
}


/* Return a weight_string according to collation */
String *Item_func_weight_string::val_str(String *str)
{
  String *res;
  const CHARSET_INFO * const cs= args[0]->collation.collation;
  uint32_t tmp_length, frm_length;
  assert(fixed == 1);

  if (args[0]->result_type() != STRING_RESULT ||
      !(res= args[0]->val_str(str)))
    goto nl;
  
  tmp_length= cs->coll->strnxfrmlen(cs, cs->mbmaxlen *
                                        cmax(res->length(), nweights));

  if (tmp_value.alloc(tmp_length))
    goto nl;

  frm_length= cs->coll->strnxfrm(cs,
                                 (unsigned char*) tmp_value.ptr(), tmp_length,
                                 nweights ? nweights : tmp_length,
                                 (const unsigned char*) res->ptr(), res->length(),
                                 flags);
  tmp_value.length(frm_length);
  null_value= 0;
  return &tmp_value;

nl:
  null_value= 1;
  return 0;
}


String *Item_load_file::val_str(String *str)
{
  assert(fixed == 1);
  String *file_name;
  File file;
  struct stat stat_info;
  char path[FN_REFLEN];

  if (!(file_name= args[0]->val_str(str)))
    goto err;

  (void) fn_format(path, file_name->c_ptr(), drizzle_real_data_home, "",
		   MY_RELATIVE_PATH | MY_UNPACK_FILENAME);

  /* Read only allowed from within dir specified by secure_file_priv */
  if (opt_secure_file_priv &&
      strncmp(opt_secure_file_priv, path, strlen(opt_secure_file_priv)))
    goto err;

  if (stat(path, &stat_info))
    goto err;

  if (!(stat_info.st_mode & S_IROTH))
  {
    /* my_error(ER_TEXTFILE_NOT_READABLE, MYF(0), file_name->c_ptr()); */
    goto err;
  }
  if (stat_info.st_size > (long) current_session->variables.max_allowed_packet)
  {
    push_warning_printf(current_session, DRIZZLE_ERROR::WARN_LEVEL_WARN,
			ER_WARN_ALLOWED_PACKET_OVERFLOWED,
			ER(ER_WARN_ALLOWED_PACKET_OVERFLOWED),
			func_name(), current_session->variables.max_allowed_packet);
    goto err;
  }
  if (tmp_value.alloc(stat_info.st_size))
    goto err;
  if ((file = my_open(file_name->c_ptr(), O_RDONLY, MYF(0))) < 0)
    goto err;
  if (my_read(file, (unsigned char*) tmp_value.ptr(), stat_info.st_size, MYF(MY_NABP)))
  {
    my_close(file, MYF(0));
    goto err;
  }
  tmp_value.length(stat_info.st_size);
  my_close(file, MYF(0));
  null_value = 0;
  return(&tmp_value);

err:
  null_value = 1;
  return(0);
}

