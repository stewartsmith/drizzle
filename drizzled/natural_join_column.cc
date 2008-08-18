#include <drizzled/server_includes.h>
#include <drizzled/natural_join_column.h>

Natural_join_column::Natural_join_column(Field_translator *field_param,
                                         TableList *tab)
{
  assert(tab->field_translation);
  view_field= field_param;
  table_field= NULL;
  table_ref= tab;
  is_common= false;
}


Natural_join_column::Natural_join_column(Field *field_param,
                                         TableList *tab)
{
  assert(tab->table == field_param->table);
  table_field= field_param;
  view_field= NULL;
  table_ref= tab;
  is_common= false;
}


const char *Natural_join_column::name()
{
  if (view_field)
  {
    assert(table_field == NULL);
    return view_field->name;
  }

  return table_field->field_name;
}


Item *Natural_join_column::create_item(THD *thd)
{
  if (view_field)
  {
    assert(table_field == NULL);
    return create_view_field(thd, table_ref, &view_field->item,
                             view_field->name);
  }
  return new Item_field(thd, &thd->lex->current_select->context, table_field);
}


Field *Natural_join_column::field()
{
  if (view_field)
  {
    assert(table_field == NULL);
    return NULL;
  }
  return table_field;
}


const char *Natural_join_column::table_name()
{
  assert(table_ref);
  return table_ref->alias;
}


const char *Natural_join_column::db_name()
{
  /*
    Test that TableList::db is the same as st_table_share::db to
    ensure consistency. An exception are I_S schema tables, which
    are inconsistent in this respect.
  */
  assert(!strcmp(table_ref->db,
                      table_ref->table->s->db.str) ||
              (table_ref->schema_table &&
               table_ref->table->s->db.str[0] == 0));
  return table_ref->db;
}
