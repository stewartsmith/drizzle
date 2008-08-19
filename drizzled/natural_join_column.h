#ifndef DRIZZLED_NATURAL_JOIN_COLUMN_H
#define DRIZZLED_NATURAL_JOIN_COLUMN_H

class Field;
class Field_translator;

/*
  Column reference of a NATURAL/USING join. Since column references in
  joins can be both from views and stored tables, may point to either a
  Field (for tables), or a Field_translator (for views).
*/

class Natural_join_column: public Sql_alloc
{
public:
  Field            *table_field; /* Column reference of table or temp view. */
  TableList *table_ref; /* Original base table/view reference. */
  /*
    True if a common join column of two NATURAL/USING join operands. Notice
    that when we have a hierarchy of nested NATURAL/USING joins, a column can
    be common at some level of nesting but it may not be common at higher
    levels of nesting. Thus this flag may change depending on at which level
    we are looking at some column.
  */
  bool is_common;
public:
  Natural_join_column(Field_translator *field_param, TableList *tab);
  Natural_join_column(Field *field_param, TableList *tab);
  const char *name();
  Item *create_item(THD *thd);
  Field *field();
  const char *table_name();
  const char *db_name();
};



#endif /* DRIZZLED_NATURAL_JOIN_COLUMN_H */
