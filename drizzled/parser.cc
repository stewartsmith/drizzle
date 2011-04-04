/* - mode: c; c-basic-offset: 2; indent-tabs-mode: nil; -*-
 * vim:expandtab:shiftwidth=2:tabstop=2:smarttab:
 *
 *  Copyright (C) 2010 Brian Aker
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
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
#include <drizzled/session.h>
#include <drizzled/parser.h>
#include <drizzled/alter_info.h>

#include <drizzled/message/alter_table.pb.h>
#include <drizzled/item/subselect.h>
#include <drizzled/sql_lex.h>

namespace drizzled {
namespace parser {

/**
  Helper to resolve the SQL:2003 Syntax exception 1) in <in predicate>.
  See SQL:2003, Part 2, section 8.4 <in predicate>, Note 184, page 383.
  This function returns the proper item for the SQL expression
  <code>left [NOT] IN ( expr )</code>
  @param session the current thread
  @param left the in predicand
  @param equal true for IN predicates, false for NOT IN predicates
  @param expr first and only expression of the in value list
  @return an expression representing the IN predicate.
*/

Item* handle_sql2003_note184_exception(Session *session, Item* left, bool equal, Item *expr)
{
  /*
    Relevant references for this issue:
    - SQL:2003, Part 2, section 8.4 <in predicate>, page 383,
    - SQL:2003, Part 2, section 7.2 <row value expression>, page 296,
    - SQL:2003, Part 2, section 6.3 <value expression primary>, page 174,
    - SQL:2003, Part 2, section 7.15 <subquery>, page 370,
    - SQL:2003 Feature F561, "Full value expressions".

    The exception in SQL:2003 Note 184 means:
    Item_singlerow_subselect, which corresponds to a <scalar subquery>,
    should be re-interpreted as an Item_in_subselect, which corresponds
    to a <table subquery> when used inside an <in predicate>.

    Our reading of Note 184 is reccursive, so that all:
    - IN (( <subquery> ))
    - IN ((( <subquery> )))
    - IN '('^N <subquery> ')'^N
    - etc
    should be interpreted as a <table subquery>, no matter how deep in the
    expression the <subquery> is.
  */

  Item *result;

  if (expr->type() == Item::SUBSELECT_ITEM)
  {
    Item_subselect *expr2 = (Item_subselect*) expr;

    if (expr2->substype() == Item_subselect::SINGLEROW_SUBS)
    {
      Item_singlerow_subselect *expr3 = (Item_singlerow_subselect*) expr2;
      Select_Lex *subselect;

      /*
        Implement the mandated change, by altering the semantic tree:
          left IN Item_singlerow_subselect(subselect)
        is modified to
          left IN (subselect)
        which is represented as
          Item_in_subselect(left, subselect)
      */
      subselect= expr3->invalidate_and_restore_select_lex();
      result= new (session->mem_root) Item_in_subselect(left, subselect);

      if (! equal)
        result = negate_expression(session, result);

      return(result);
    }
  }

  if (equal)
    result= new (session->mem_root) Item_func_eq(left, expr);
  else
    result= new (session->mem_root) Item_func_ne(left, expr);

  return(result);
}

/**
   @brief Creates a new Select_Lex for a UNION branch.

   Sets up and initializes a Select_Lex structure for a query once the parser
   discovers a UNION token. The current Select_Lex is pushed on the stack and
   the new Select_Lex becomes the current one..=

   @lex The parser state.

   @is_union_distinct True if the union preceding the new select statement
   uses UNION DISTINCT.

   @return <code>false</code> if successful, <code>true</code> if an error was
   reported. In the latter case parsing should stop.
 */
bool add_select_to_union_list(Session *session, LEX *lex, bool is_union_distinct)
{
  if (lex->result)
  {
    /* Only the last SELECT can have  INTO...... */
    my_error(ER_WRONG_USAGE, MYF(0), "UNION", "INTO");
    return true;
  }
  if (lex->current_select->linkage == GLOBAL_OPTIONS_TYPE)
  {
    my_parse_error(session->m_lip);
    return true;
  }
  /* This counter shouldn't be incremented for UNION parts */
  lex->nest_level--;
  if (new_select(lex, 0))
    return true;
  init_select(lex);
  lex->current_select->linkage=UNION_TYPE;
  if (is_union_distinct) /* UNION DISTINCT - remember position */
    lex->current_select->master_unit()->union_distinct=
      lex->current_select;
  return false;
}

/**
   @brief Initializes a Select_Lex for a query within parentheses (aka
   braces).

   @return false if successful, true if an error was reported. In the latter
   case parsing should stop.
 */
bool setup_select_in_parentheses(Session *session, LEX *lex)
{
  Select_Lex * sel= lex->current_select;
  if (sel->set_braces(1))
  {
    my_parse_error(session->m_lip);
    return true;
  }
  if (sel->linkage == UNION_TYPE &&
      !sel->master_unit()->first_select()->braces &&
      sel->master_unit()->first_select()->linkage ==
      UNION_TYPE)
  {
    my_parse_error(session->m_lip);
    return true;
  }
  if (sel->linkage == UNION_TYPE &&
      sel->olap != UNSPECIFIED_OLAP_TYPE &&
      sel->master_unit()->fake_select_lex)
  {
    my_error(ER_WRONG_USAGE, MYF(0), "CUBE/ROLLUP", "ORDER BY");
    return true;
  }
  /* select in braces, can't contain global parameters */
  if (sel->master_unit()->fake_select_lex)
    sel->master_unit()->global_parameters=
      sel->master_unit()->fake_select_lex;
  return false;
}

Item* reserved_keyword_function(Session *session, const std::string &name, List<Item> *item_list)
{
  const plugin::Function *udf= plugin::Function::get(name);

  if (udf)
  {
    return Create_udf_func::s_singleton.create(session, udf, item_list);
  }

  my_error(ER_SP_DOES_NOT_EXIST, MYF(0), "FUNCTION", name.c_str());

  return NULL;
}

/**
  @brief Push an error message into MySQL error stack with line
  and position information.

  This function provides semantic action implementers with a way
  to push the famous "You have a syntax error near..." error
  message into the error stack, which is normally produced only if
  a parse error is discovered internally by the Bison generated
  parser.
*/
void my_parse_error(Lex_input_stream *lip)
{
  assert(lip);

  const char *yytext= lip->get_tok_start();
  /* Push an error into the error stack */
  my_printf_error(ER_PARSE_ERROR,  ER(ER_PARSE_ERROR), MYF(0), ER(ER_SYNTAX_ERROR),
                  (yytext ? yytext : ""),
                  lip->yylineno);
}

void my_parse_error(const char *message)
{
  my_printf_error(ER_PARSE_ERROR_UNKNOWN, ER(ER_PARSE_ERROR_UNKNOWN), MYF(0), message);
}

bool check_reserved_words(LEX_STRING *name)
{
  if (!my_strcasecmp(system_charset_info, name->str, "GLOBAL") ||
      !my_strcasecmp(system_charset_info, name->str, "LOCAL") ||
      !my_strcasecmp(system_charset_info, name->str, "SESSION"))
    return true;

  return false;
}


/**
  @brief Bison callback to report a syntax/OOM error

  This function is invoked by the bison-generated parser
  when a syntax error, a parse error or an out-of-memory
  condition occurs. This function is not invoked when the
  parser is requested to abort by semantic action code
  by means of YYABORT or YYACCEPT macros. This is why these
  macros should not be used (use DRIZZLE_YYABORT/DRIZZLE_YYACCEPT
  instead).

  The parser will abort immediately after invoking this callback.

  This function is not for use in semantic actions and is internal to
  the parser, as it performs some pre-return cleanup.
  In semantic actions, please use parser::my_parse_error or my_error to
  push an error into the error stack and DRIZZLE_YYABORT
  to abort from the parser.
*/
void errorOn(drizzled::Session *session, const char *s)
{
  /* "parse error" changed into "syntax error" between bison 1.75 and 1.875 */
  if (strcmp(s,"parse error") == 0 || strcmp(s,"syntax error") == 0)
  {
    parser::my_parse_error(session->m_lip);
  }
  else
  {
    parser::my_parse_error(s);
  }
}

bool buildOrderBy(LEX *lex)
{
  Select_Lex *sel= lex->current_select;
  Select_Lex_Unit *unit= sel-> master_unit();

  if (sel->linkage != GLOBAL_OPTIONS_TYPE &&
      sel->olap != UNSPECIFIED_OLAP_TYPE &&
      (sel->linkage != UNION_TYPE || sel->braces))
  {
    my_error(ER_WRONG_USAGE, MYF(0),
             "CUBE/ROLLUP", "ORDER BY");
    return false;
  }

  if (lex->sql_command != SQLCOM_ALTER_TABLE && !unit->fake_select_lex)
  {
    /*
      A query of the of the form (SELECT ...) ORDER BY order_list is
      executed in the same way as the query
      SELECT ... ORDER BY order_list
      unless the SELECT construct contains ORDER BY or LIMIT clauses.
      Otherwise we create a fake Select_Lex if it has not been created
      yet.
    */
    Select_Lex *first_sl= unit->first_select();
    if (!unit->is_union() &&
        (first_sl->order_list.elements ||
         first_sl->select_limit) &&           
        unit->add_fake_select_lex(lex->session))
    {
      return false;
    }
  }

  return true;
}

void buildEngineOption(LEX *lex, const char *key, const LEX_STRING &value)
{
  message::Engine::Option *opt= lex->table()->mutable_engine()->add_options();
  opt->set_name(key);
  opt->set_state(value.str, value.length);
}

void buildEngineOption(LEX *lex, const char *key, uint64_t value)
{
  drizzled::message::Engine::Option *opt= lex->table()->mutable_engine()->add_options();
  opt->set_name(key);
  opt->set_state(boost::lexical_cast<std::string>(value));
}

void buildSchemaOption(LEX *lex, const char *key, const LEX_STRING &value)
{
  statement::CreateSchema *statement= (statement::CreateSchema *)lex->statement;
  message::Engine::Option *opt= statement->schema_message.mutable_engine()->add_options();
  opt->set_name(key);
  opt->set_state(value.str, value.length);
}

void buildSchemaDefiner(LEX *lex, const LEX_STRING &value)
{
  statement::CreateSchema *statement= (statement::CreateSchema *)lex->statement;
  identifier::User user(value.str);
  message::set_definer(statement->schema_message, user);
}

void buildSchemaDefiner(LEX *lex, const identifier::User &user)
{
  statement::CreateSchema *statement= (statement::CreateSchema *)lex->statement;
  message::set_definer(statement->schema_message, user);
}

void buildSchemaOption(LEX *lex, const char *key, uint64_t value)
{
  statement::CreateSchema *statement= (statement::CreateSchema *)lex->statement;
  message::Engine::Option *opt= statement->schema_message.mutable_engine()->add_options();
  opt->set_name(key);
  opt->set_state(boost::lexical_cast<std::string>(value));
}

bool checkFieldIdent(LEX *lex, const LEX_STRING &schema_name, const LEX_STRING &table_name)
{
  TableList *table= reinterpret_cast<TableList*>(lex->current_select->table_list.first);

  if (schema_name.length)
  {
    if (my_strcasecmp(table_alias_charset, schema_name.str, table->getSchemaName()))
    {
      my_error(ER_WRONG_DB_NAME, MYF(0), schema_name.str);
      return false;
    }
  }

  if (my_strcasecmp(table_alias_charset, table_name.str,
                    table->getTableName()))
  {
    my_error(ER_WRONG_TABLE_NAME, MYF(0), table_name.str);
    return false;
  }

  return true;
}

Item *buildIdent(LEX *lex,
                 const LEX_STRING &schema_name,
                 const LEX_STRING &table_name,
                 const LEX_STRING &field_name)
{
  Select_Lex *sel= lex->current_select;

  if (table_name.length and sel->no_table_names_allowed)
  {
    my_error(ER_TABLENAME_NOT_ALLOWED_HERE,
             MYF(0), table_name.str, lex->session->where());
  }

  Item *item= (sel->parsing_place != IN_HAVING or
               sel->get_in_sum_expr() > 0) ?
    (Item*) new Item_field(lex->current_context(), schema_name.str, table_name.str, field_name.str) :
    (Item*) new Item_ref(lex->current_context(), schema_name.str, table_name.str, field_name.str);

  return item;
}

Item *buildTableWild(LEX *lex, const LEX_STRING &schema_name, const LEX_STRING &table_name)
{
  Select_Lex *sel= lex->current_select;
  Item *item= new Item_field(lex->current_context(), schema_name.str, table_name.str, "*");
  sel->with_wild++;

  return item;
}

void buildCreateFieldIdent(LEX *lex)
{
  statement::CreateTable *statement= (statement::CreateTable *)lex->statement;
  lex->length= lex->dec=0;
  lex->type=0;
  statement->default_value= statement->on_update_value= 0;
  statement->comment= null_lex_str;
  lex->charset= NULL;
  statement->column_format= COLUMN_FORMAT_TYPE_DEFAULT;

  message::AddedFields &added_fields_proto= ((statement::CreateTable *)lex->statement)->alter_info.added_fields_proto;
  lex->setField(added_fields_proto.add_added_field());
}

void storeAlterColumnPosition(LEX *lex, const char *position)
{
  statement::AlterTable *statement= (statement::AlterTable *)lex->statement;

  lex->last_field->after=const_cast<char*> (position);
  statement->alter_info.flags.set(ALTER_COLUMN_ORDER);
}

bool buildCollation(LEX *lex, const charset_info_st *arg)
{
  statement::CreateTable *statement= (statement::CreateTable *)lex->statement;

  HA_CREATE_INFO *cinfo= &statement->create_info();
  if ((cinfo->used_fields & HA_CREATE_USED_DEFAULT_CHARSET) &&
      cinfo->default_table_charset && arg &&
      !my_charset_same(cinfo->default_table_charset, arg))
  {
    my_error(ER_COLLATION_CHARSET_MISMATCH, MYF(0),
             arg->name, cinfo->default_table_charset->csname);
    return false;
  }
  statement->create_info().default_table_charset= arg;
  statement->create_info().used_fields|= HA_CREATE_USED_DEFAULT_CHARSET;

  return true;
}

void buildKey(LEX *lex, Key::Keytype type_par, const lex_string_t &name_arg)
{
  statement::AlterTable *statement= (statement::AlterTable *)lex->statement;
  Key *key= new Key(type_par, name_arg, &statement->key_create_info, 0,
                    lex->col_list);
  statement->alter_info.key_list.push_back(key);
  lex->col_list.clear(); /* Alloced by memory::sql_alloc */
}

void buildForeignKey(LEX *lex, const lex_string_t &name_arg, drizzled::Table_ident *table)
{
  statement::AlterTable *statement= (statement::AlterTable *)lex->statement;
  Key *key= new Foreign_key(name_arg, lex->col_list,
                            table,
                            lex->ref_list,
                            statement->fk_delete_opt,
                            statement->fk_update_opt,
                            statement->fk_match_option);

  statement->alter_info.key_list.push_back(key);
  key= new Key(Key::MULTIPLE, name_arg,
               &default_key_create_info, 1,
               lex->col_list);
  statement->alter_info.key_list.push_back(key);
  lex->col_list.clear(); /* Alloced by memory::sql_alloc */
  /* Only used for ALTER TABLE. Ignored otherwise. */
  statement->alter_info.flags.set(ALTER_FOREIGN_KEY);
}

drizzled::enum_field_types buildIntegerColumn(LEX *lex, drizzled::enum_field_types final_type, const bool is_unsigned)
{ 
  lex->length=(char*) 0; /* use default length */

  if (is_unsigned)
  {
    final_type= DRIZZLE_TYPE_LONGLONG;
  }

  if (lex->field())
  {
    assert (final_type == DRIZZLE_TYPE_LONG or final_type == DRIZZLE_TYPE_LONGLONG);
    // We update the type for unsigned types
    if (is_unsigned)
    {
      lex->field()->set_type(message::Table::Field::BIGINT);
      lex->field()->mutable_constraints()->set_is_unsigned(true);
    }
    else if (final_type == DRIZZLE_TYPE_LONG)
    {
      lex->field()->set_type(message::Table::Field::INTEGER);
    }
    else if (final_type == DRIZZLE_TYPE_LONGLONG)
    {
      lex->field()->set_type(message::Table::Field::BIGINT);
    }
  }

  return final_type;
}

drizzled::enum_field_types buildSerialColumn(LEX *lex)
{
  statement::AlterTable *statement= (statement::AlterTable *)lex->statement;
  statement->alter_info.flags.set(ALTER_ADD_INDEX);

  lex->type|= (AUTO_INCREMENT_FLAG | NOT_NULL_FLAG | UNIQUE_FLAG | UNSIGNED_FLAG);

  if (lex->field())
  {
    lex->field()->mutable_constraints()->set_is_notnull(true);
    lex->field()->mutable_constraints()->set_is_unsigned(true);

    lex->field()->set_type(message::Table::Field::BIGINT);
  }

  return DRIZZLE_TYPE_LONGLONG;
}

drizzled::enum_field_types buildVarcharColumn(LEX *lex, const char *length)
{
  lex->length= const_cast<char *>(length);

  if (lex->field())
  {
    lex->field()->set_type(message::Table::Field::VARCHAR);

    message::Table::Field::StringFieldOptions *string_field_options;

    string_field_options= lex->field()->mutable_string_options();

    string_field_options->set_length(atoi(length));
  }

  return DRIZZLE_TYPE_VARCHAR;
}

drizzled::enum_field_types buildDecimalColumn(LEX *lex)
{
  if (lex->field())
    lex->field()->set_type(message::Table::Field::DECIMAL);

  return DRIZZLE_TYPE_DECIMAL;
}

drizzled::enum_field_types buildVarbinaryColumn(LEX *lex, const char *length)
{
  lex->length= const_cast<char *>(length);
  lex->charset= &my_charset_bin;

  if (lex->field())
  {
    lex->field()->set_type(message::Table::Field::VARCHAR);

    message::Table::Field::StringFieldOptions *string_field_options;

    string_field_options= lex->field()->mutable_string_options();

    string_field_options->set_length(atoi(length));
    string_field_options->set_collation_id(my_charset_bin.number);
    string_field_options->set_collation(my_charset_bin.name);
  }

  return DRIZZLE_TYPE_VARCHAR;
}

drizzled::enum_field_types buildBlobColumn(LEX *lex)
{
  lex->charset=&my_charset_bin;
  lex->length=(char*) 0; /* use default length */

  if (lex->field())
  {
    lex->field()->set_type(message::Table::Field::BLOB);
    message::Table::Field::StringFieldOptions *string_field_options;

    string_field_options= lex->field()->mutable_string_options();
    string_field_options->set_collation_id(my_charset_bin.number);
    string_field_options->set_collation(my_charset_bin.name);
  }

  return DRIZZLE_TYPE_BLOB;
}

drizzled::enum_field_types buildBooleanColumn(LEX *lex)
{
  if (lex->field())
    lex->field()->set_type(message::Table::Field::BOOLEAN);

  return DRIZZLE_TYPE_BOOLEAN;
}

drizzled::enum_field_types buildUuidColumn(LEX *lex)
{
  if (lex->field())
    lex->field()->set_type(message::Table::Field::UUID);

  return DRIZZLE_TYPE_UUID;
}

drizzled::enum_field_types buildDoubleColumn(LEX *lex)
{
  if (lex->field())
  {
    lex->field()->set_type(message::Table::Field::DOUBLE);
  }

  return DRIZZLE_TYPE_DOUBLE;
}

drizzled::enum_field_types buildTimestampColumn(LEX *lex, const char *length)
{
  if (lex->field())
  {
    lex->field()->set_type(message::Table::Field::EPOCH);
  }

  if (length)
  {
    lex->length= const_cast<char *>(length);
    return DRIZZLE_TYPE_MICROTIME;
  }

  lex->length= NULL;

  return DRIZZLE_TYPE_TIMESTAMP;
}

void buildKeyOnColumn(LEX *lex)
{
  statement::AlterTable *statement= (statement::AlterTable *)lex->statement;

  lex->type|= UNIQUE_KEY_FLAG;
  statement->alter_info.flags.set(ALTER_ADD_INDEX);

  if (lex->field())
  {
    lex->field()->mutable_constraints()->set_is_unique(true);
  }
}

void buildAutoOnColumn(LEX *lex)
{
  lex->type|= AUTO_INCREMENT_FLAG | NOT_NULL_FLAG;

  if (lex->field())
  {
    lex->field()->mutable_constraints()->set_is_notnull(true);
  }
}

void buildPrimaryOnColumn(LEX *lex)
{
  statement::AlterTable *statement= (statement::AlterTable *)lex->statement;

  lex->type|= PRI_KEY_FLAG | NOT_NULL_FLAG;
  statement->alter_info.flags.set(ALTER_ADD_INDEX);

  if (lex->field())
  {
    lex->field()->mutable_constraints()->set_is_notnull(true);
  }
}

void buildReplicationOption(LEX *lex, bool arg)
{
  statement::CreateSchema *statement= (statement::CreateSchema *)lex->statement;
  message::set_is_replicated(statement->schema_message, arg);
}

void buildAddAlterDropIndex(LEX *lex, const char *name, bool is_foreign_key)
{
  statement::AlterTable *statement= (statement::AlterTable *)lex->statement;

  message::AlterTable::AlterTableOperation *operation;
  operation= lex->alter_table()->add_operations();
  operation->set_drop_name(name);

  statement->alter_info.flags.set(ALTER_DROP_INDEX);
  if (is_foreign_key)
  {
    statement->alter_info.flags.set(ALTER_FOREIGN_KEY);
    operation->set_operation(message::AlterTable::AlterTableOperation::DROP_FOREIGN_KEY);
  }
  else
  {
    operation->set_operation(message::AlterTable::AlterTableOperation::DROP_KEY);
  }
}

} // namespace parser
} // namespace drizzled
