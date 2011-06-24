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

#include <config.h>
#include <drizzled/show.h>
#include <drizzled/table.h>
#include <drizzled/item/ident.h>

#include <cstdio>

using namespace std;

namespace drizzled
{

const uint32_t NO_CACHED_FIELD_INDEX= UINT32_MAX;

Item_ident::Item_ident(Name_resolution_context *context_arg,
                       const char *db_name_arg,const char *table_name_arg,
                       const char *field_name_arg)
  :orig_db_name(db_name_arg), orig_table_name(table_name_arg),
   orig_field_name(field_name_arg), context(context_arg),
   db_name(db_name_arg), table_name(table_name_arg),
   field_name(field_name_arg),
   alias_name_used(false), cached_field_index(NO_CACHED_FIELD_INDEX),
   cached_table(0), depended_from(0)
{
  name = (char*) field_name_arg;
}

/**
  Constructor used by Item_field & Item_*_ref (see Item comment)
*/

Item_ident::Item_ident(Session *session, Item_ident *item)
  :Item(session, item),
   orig_db_name(item->orig_db_name),
   orig_table_name(item->orig_table_name),
   orig_field_name(item->orig_field_name),
   context(item->context),
   db_name(item->db_name),
   table_name(item->table_name),
   field_name(item->field_name),
   alias_name_used(item->alias_name_used),
   cached_field_index(item->cached_field_index),
   cached_table(item->cached_table),
   depended_from(item->depended_from)
{}

void Item_ident::cleanup()
{
  Item::cleanup();
  db_name= orig_db_name;
  table_name= orig_table_name;
  field_name= orig_field_name;
  depended_from= 0;
  return;
}

bool Item_ident::remove_dependence_processor(unsigned char * arg)
{
  if (depended_from == (Select_Lex *) arg)
    depended_from= 0;
  return 0;
}

const char *Item_ident::full_name() const
{
  char *tmp;
	size_t tmp_len;
  if (!table_name || !field_name)
    return field_name ? field_name : name ? name : "tmp_field";
  if (db_name && db_name[0])
  {
    tmp_len= strlen(db_name)+strlen(table_name)+strlen(field_name)+3;
    tmp= (char*) memory::sql_alloc(tmp_len);
    snprintf(tmp, tmp_len, "%s.%s.%s",db_name,table_name,field_name);
  }
  else
  {
    if (table_name[0])
    {
      tmp_len=strlen(table_name)+strlen(field_name)+2;
      tmp= (char*) memory::sql_alloc(tmp_len);
      snprintf(tmp, tmp_len, "%s.%s", table_name, field_name);
    }
    else
      tmp= (char*) field_name;
  }
  return tmp;
}


void Item_ident::print(String *str)
{
  string d_name, t_name;

  if (table_name && table_name[0])
  {
    t_name.assign(table_name);
    std::transform(t_name.begin(), t_name.end(),
                   t_name.begin(), ::tolower);
  }
 
  if (db_name && db_name[0])
  {
    d_name.assign(db_name);
    // Keeping the std:: prefix here, since Item_ident has a transform
    // method
      std::transform(d_name.begin(), d_name.end(),
                     d_name.begin(), ::tolower);
  }

  if (!table_name || !field_name || !field_name[0])
  {
    const char *nm= (field_name && field_name[0]) ?
                      field_name : name ? name : "tmp_field";
    str->append_identifier(nm, (uint32_t) strlen(nm));

    return;
  }
  if (db_name && db_name[0] && !alias_name_used)
  {
    {
      str->append_identifier(d_name.c_str(), d_name.length());
      str->append('.');
    }
    str->append_identifier(t_name.c_str(), t_name.length());
    str->append('.');
    str->append_identifier(field_name, (uint32_t)strlen(field_name));
  }
  else
  {
    if (table_name[0])
    {
      str->append_identifier(t_name.c_str(), t_name.length());
      str->append('.');
      str->append_identifier(field_name, (uint32_t) strlen(field_name));
    }
    else
      str->append_identifier(field_name, (uint32_t) strlen(field_name));
  }
}

double Item_ident_for_show::val_real()
{
  return field->val_real();
}


int64_t Item_ident_for_show::val_int()
{
  return field->val_int();
}


String *Item_ident_for_show::val_str(String *str)
{
  return field->val_str_internal(str);
}


type::Decimal *Item_ident_for_show::val_decimal(type::Decimal *dec)
{
  return field->val_decimal(dec);
}

void Item_ident_for_show::make_field(SendField *tmp_field)
{
  tmp_field->table_name= tmp_field->org_table_name= table_name;
  tmp_field->db_name= db_name;
  tmp_field->col_name= tmp_field->org_col_name= field->field_name;
  tmp_field->charsetnr= field->charset()->number;
  tmp_field->length=field->field_length;
  tmp_field->type=field->type();
  tmp_field->flags= field->getTable()->maybe_null ?
    (field->flags & ~NOT_NULL_FLAG) : field->flags;
  tmp_field->decimals= field->decimals();
}

} /* namespace drizzled */
