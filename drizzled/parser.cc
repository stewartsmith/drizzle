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

#include "config.h"

#include <drizzled/parser.h>

namespace drizzled
{
namespace parser
{

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
    parser::error_t pass= { ER(ER_SYNTAX_ERROR), session };
    my_parse_error(pass);
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
    parser::error_t pass= { ER(ER_SYNTAX_ERROR), session };
    my_parse_error(pass);
    return true;
  }
  if (sel->linkage == UNION_TYPE &&
      !sel->master_unit()->first_select()->braces &&
      sel->master_unit()->first_select()->linkage ==
      UNION_TYPE)
  {
    parser::error_t pass= { ER(ER_SYNTAX_ERROR), session };
    my_parse_error(pass);
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
  const plugin::Function *udf= plugin::Function::get(name.c_str(), name.length());
  Item *item= NULL;

  if (udf)
  {
    item= Create_udf_func::s_singleton.create(session, udf, item_list);
  } else {
    my_error(ER_SP_DOES_NOT_EXIST, MYF(0), "FUNCTION", name.c_str());
  }

  return item;
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
void my_parse_error(parser::error_t &arg)
{
  Lex_input_stream *lip= arg.session->m_lip;

  const char *yytext= lip->get_tok_start();
  /* Push an error into the error stack */
  my_printf_error(ER_PARSE_ERROR,  ER(ER_PARSE_ERROR), MYF(0), arg.s,
                  (yytext ? yytext : ""),
                  lip->yylineno);
}

bool check_reserved_words(LEX_STRING *name)
{
  if (!my_strcasecmp(system_charset_info, name->str, "GLOBAL") ||
      !my_strcasecmp(system_charset_info, name->str, "LOCAL") ||
      !my_strcasecmp(system_charset_info, name->str, "SESSION"))
    return true;

  return false;
}



} // namespace parser
} // namespace drizzled
