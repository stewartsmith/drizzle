#include <drizzled/server_includes.h>
#include <drizzled/natural_join_column.h>
#include <drizzled/table_list.h>
#include <drizzled/session.h>

Natural_join_column::Natural_join_column(Field *field_param,
                                         TableList *tab)
{
  assert(tab->table == field_param->table);
  table_field= field_param;
  table_ref= tab;
  is_common= false;
}


const char *Natural_join_column::name()
{
  return table_field->field_name;
}


Item *Natural_join_column::create_item(Session *session)
{
  return new Item_field(session, &session->lex->current_select->context, table_field);
}


Field *Natural_join_column::field()
{
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
    Test that TableList::db is the same as TableShare::db to
    ensure consistency. An exception are I_S schema tables, which
    are inconsistent in this respect.
  */
  assert(!strcmp(table_ref->db,
                      table_ref->table->s->db.str) ||
              (table_ref->schema_table &&
               table_ref->table->s->db.str[0] == 0));
  return table_ref->db;
}
