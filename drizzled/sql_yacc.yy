/* Copyright (C) 2000-2003 MySQL AB

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

/* sql_yacc.yy */

/**
  @defgroup Parser Parser
  @{
*/

%{
/* session is passed as an argument to yyparse(), and subsequently to yylex().
** The type will be void*, so it must be  cast to (Session*) when used.
** Use the YYSession macro for this.
*/
#define YYPARSE_PARAM yysession
#define YYLEX_PARAM yysession
#define YYSession (static_cast<Session *>(yysession))

#define YYENABLE_NLS 0
#define YYLTYPE_IS_TRIVIAL 0

#define DRIZZLE_YACC
#define YYINITDEPTH 100
#define YYMAXDEPTH 3200                        /* Because of 64K stack */
#define Lex (YYSession->lex)

#include "config.h"
#include <cstdio>
#include "drizzled/parser.h"

int yylex(void *yylval, void *yysession);

#define yyoverflow(A,B,C,D,E,F)               \
  {                                           \
    ulong val= *(F);                          \
    if (drizzled::my_yyoverflow((B), (D), &val)) \
    {                                         \
      yyerror((char*) (A));                   \
      return 2;                               \
    }                                         \
    else                                      \
    {                                         \
      *(F)= (YYSIZE_T)val;                    \
    }                                         \
  }

#define DRIZZLE_YYABORT                         \
  do                                          \
  {                                           \
    LEX::cleanup_lex_after_parse_error(YYSession);\
    YYABORT;                                  \
  } while (0)

#define DRIZZLE_YYABORT_UNLESS(A)         \
  if (!(A))                             \
  {                                     \
    struct my_parse_error_st pass= { ER(ER_SYNTAX_ERROR), YYSession };\
    my_parse_error(&pass);\
    DRIZZLE_YYABORT;                      \
  }


#define YYDEBUG 0

namespace drizzled
{

class Table_ident;
class Item;
class Item_num;

namespace item
{
class Boolean;
class True;
class False;
}


static bool check_reserved_words(LEX_STRING *name)
{
  if (!my_strcasecmp(system_charset_info, name->str, "GLOBAL") ||
      !my_strcasecmp(system_charset_info, name->str, "LOCAL") ||
      !my_strcasecmp(system_charset_info, name->str, "SESSION"))
    return true;
  return false;
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

struct my_parse_error_st {
  const char *s;
  Session *session;
};

static void my_parse_error(void *arg)
{
 struct my_parse_error_st *ptr= (struct my_parse_error_st *)arg;

  const char *s= ptr->s;
  Session *session= ptr->session;

  Lex_input_stream *lip= session->m_lip;

  const char *yytext= lip->get_tok_start();
  /* Push an error into the error stack */
  my_printf_error(ER_PARSE_ERROR,  ER(ER_PARSE_ERROR), MYF(0), s,
                  (yytext ? yytext : ""),
                  lip->yylineno);
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
  In semantic actions, please use my_parse_error or my_error to
  push an error into the error stack and DRIZZLE_YYABORT
  to abort from the parser.
*/

static void DRIZZLEerror(const char *s)
{
  Session *session= current_session;

  /*
    Restore the original LEX if it was replaced when parsing
    a stored procedure. We must ensure that a parsing error
    does not leave any side effects in the Session.
  */
  LEX::cleanup_lex_after_parse_error(session);

  /* "parse error" changed into "syntax error" between bison 1.75 and 1.875 */
  if (strcmp(s,"parse error") == 0 || strcmp(s,"syntax error") == 0)
    s= ER(ER_SYNTAX_ERROR);

  struct my_parse_error_st pass= { s, session };
  my_parse_error(&pass);
}

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
static Item* handle_sql2003_note184_exception(Session *session,
                                              Item* left, bool equal,
                                              Item *expr)
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
static bool add_select_to_union_list(Session *session, LEX *lex, bool is_union_distinct)
{
  if (lex->result)
  {
    /* Only the last SELECT can have  INTO...... */
    my_error(ER_WRONG_USAGE, MYF(0), "UNION", "INTO");
    return true;
  }
  if (lex->current_select->linkage == GLOBAL_OPTIONS_TYPE)
  {
    struct my_parse_error_st pass= { ER(ER_SYNTAX_ERROR), session };
    my_parse_error(&pass);
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
static bool setup_select_in_parentheses(Session *session, LEX *lex)
{
  Select_Lex * sel= lex->current_select;
  if (sel->set_braces(1))
  {
    struct my_parse_error_st pass= { ER(ER_SYNTAX_ERROR), session };
    my_parse_error(&pass);
    return true;
  }
  if (sel->linkage == UNION_TYPE &&
      !sel->master_unit()->first_select()->braces &&
      sel->master_unit()->first_select()->linkage ==
      UNION_TYPE)
  {
    struct my_parse_error_st pass= { ER(ER_SYNTAX_ERROR), session };
    my_parse_error(&pass);
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

static Item* reserved_keyword_function(Session *session, const std::string &name, List<Item> *item_list)
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

} /* namespace drizzled; */

using namespace drizzled;
%}
%union {
  bool boolean;
  int  num;
  ulong ulong_num;
  uint64_t ulonglong_number;
  int64_t longlong_number;
  drizzled::LEX_STRING lex_str;
  drizzled::LEX_STRING *lex_str_ptr;
  drizzled::LEX_SYMBOL symbol;
  drizzled::Table_ident *table;
  char *simple_string;
  drizzled::Item *item;
  drizzled::Item_num *item_num;
  drizzled::List<drizzled::Item> *item_list;
  drizzled::List<drizzled::String> *string_list;
  drizzled::String *string;
  drizzled::Key_part_spec *key_part;
  const drizzled::plugin::Function *udf;
  drizzled::TableList *table_list;
  enum drizzled::enum_field_types field_val;
  struct drizzled::sys_var_with_base variable;
  enum drizzled::sql_var_t var_type;
  drizzled::Key::Keytype key_type;
  enum drizzled::ha_key_alg key_alg;
  enum drizzled::ha_rkey_function ha_rkey_mode;
  enum drizzled::enum_tx_isolation tx_isolation;
  enum drizzled::Cast_target cast_type;
  const drizzled::CHARSET_INFO *charset;
  drizzled::thr_lock_type lock_type;
  drizzled::interval_type interval, interval_time_st;
  drizzled::type::timestamp_t date_time_type;
  drizzled::Select_Lex *select_lex;
  drizzled::chooser_compare_func_creator boolfunc2creator;
  struct drizzled::st_lex *lex;
  enum drizzled::index_hint_type index_hint;
  enum drizzled::enum_filetype filetype;
  enum drizzled::ha_build_method build_method;
  drizzled::message::Table::ForeignKeyConstraint::ForeignKeyOption m_fk_option;
  drizzled::execute_string_t execute_string;
}

%{
namespace drizzled
{
bool my_yyoverflow(short **a, YYSTYPE **b, ulong *yystacksize);
}
%}

%debug
%pure_parser                                    /* We have threads */

/*
  Currently there are 70 shift/reduce conflicts.
  We should not introduce new conflicts any more.
*/
%expect 70

/*
   Comments for TOKENS.
   For each token, please include in the same line a comment that contains
   the following tags:
   SQL-2003-R : Reserved keyword as per SQL-2003
   SQL-2003-N : Non Reserved keyword as per SQL-2003
   SQL-1999-R : Reserved keyword as per SQL-1999
   SQL-1999-N : Non Reserved keyword as per SQL-1999
   MYSQL      : MySQL extention (unspecified)
   MYSQL-FUNC : MySQL extention, function
   INTERNAL   : Not a real token, lex optimization
   OPERATOR   : SQL operator
   FUTURE-USE : Reserved for futur use

   This makes the code grep-able, and helps maintenance.
*/

%token  ABORT_SYM                     /* INTERNAL (used in lex) */
%token  ACTION                        /* SQL-2003-N */
%token  ADD_SYM                           /* SQL-2003-R */
%token  ADDDATE_SYM                   /* MYSQL-FUNC */
%token  AFTER_SYM                     /* SQL-2003-N */
%token  AGGREGATE_SYM
%token  ALL                           /* SQL-2003-R */
%token  ALTER_SYM                         /* SQL-2003-R */
%token  ANALYZE_SYM
%token  AND_SYM                       /* SQL-2003-R */
%token  ANY_SYM                       /* SQL-2003-R */
%token  AS                            /* SQL-2003-R */
%token  ASC                           /* SQL-2003-N */
%token  ASENSITIVE_SYM                /* FUTURE-USE */
%token  AT_SYM                        /* SQL-2003-R */
%token  AUTO_INC
%token  AVG_SYM                       /* SQL-2003-N */
%token  BEFORE_SYM                    /* SQL-2003-N */
%token  BEGIN_SYM                     /* SQL-2003-R */
%token  BETWEEN_SYM                   /* SQL-2003-R */
%token  BIGINT_SYM                    /* SQL-2003-R */
%token  BINARY                        /* SQL-2003-R */
%token  BIN_NUM
%token  BIT_SYM                       /* MYSQL-FUNC */
%token  BLOB_SYM                      /* SQL-2003-R */
%token  BOOLEAN_SYM                   /* SQL-2003-R */
%token  BOOL_SYM
%token  BOTH                          /* SQL-2003-R */
%token  BTREE_SYM
%token  BY                            /* SQL-2003-R */
%token  CALL_SYM                      /* SQL-2003-R */
%token  CASCADE                       /* SQL-2003-N */
%token  CASCADED                      /* SQL-2003-R */
%token  CASE_SYM                      /* SQL-2003-R */
%token  CAST_SYM                      /* SQL-2003-R */
%token  CATALOG_SYM
%token  CHAIN_SYM                     /* SQL-2003-N */
%token  CHANGE_SYM
%token  CHAR_SYM                      /* SQL-2003-R */
%token  CHECKSUM_SYM
%token  CHECK_SYM                     /* SQL-2003-R */
%token  CLOSE_SYM                     /* SQL-2003-R */
%token  COALESCE                      /* SQL-2003-N */
%token  COLLATE_SYM                   /* SQL-2003-R */
%token  COLLATION_SYM                 /* SQL-2003-N */
%token  COLUMNS
%token  COLUMN_SYM                    /* SQL-2003-R */
%token  COMMENT_SYM
%token  COMMITTED_SYM                 /* SQL-2003-N */
%token  COMMIT_SYM                    /* SQL-2003-R */
%token  COMPACT_SYM
%token  COMPRESSED_SYM
%token  CONCURRENT
%token  CONDITION_SYM                 /* SQL-2003-N */
%token  CONNECTION_SYM
%token  CONSISTENT_SYM
%token  CONSTRAINT                    /* SQL-2003-R */
%token  CONTAINS_SYM                  /* SQL-2003-N */
%token  CONVERT_SYM                   /* SQL-2003-N */
%token  COUNT_SYM                     /* SQL-2003-N */
%token  CREATE                        /* SQL-2003-R */
%token  CROSS                         /* SQL-2003-R */
%token  CUBE_SYM                      /* SQL-2003-R */
%token  CURDATE                       /* MYSQL-FUNC */
%token  CURRENT_USER                  /* SQL-2003-R */
%token  CURSOR_SYM                    /* SQL-2003-R */
%token  DATABASE
%token  DATABASES
%token  DATA_SYM                      /* SQL-2003-N */
%token  DATETIME_SYM
%token  DATE_ADD_INTERVAL             /* MYSQL-FUNC */
%token  DATE_SUB_INTERVAL             /* MYSQL-FUNC */
%token  DATE_SYM                      /* SQL-2003-R */
%token  DAY_HOUR_SYM
%token  DAY_MICROSECOND_SYM
%token  DAY_MINUTE_SYM
%token  DAY_SECOND_SYM
%token  DAY_SYM                       /* SQL-2003-R */
%token  DEALLOCATE_SYM                /* SQL-2003-R */
%token  DECIMAL_NUM
%token  DECIMAL_SYM                   /* SQL-2003-R */
%token  DECLARE_SYM                   /* SQL-2003-R */
%token  DEFAULT                       /* SQL-2003-R */
%token  DELETE_SYM                    /* SQL-2003-R */
%token  DESC                          /* SQL-2003-N */
%token  DESCRIBE                      /* SQL-2003-R */
%token  DETERMINISTIC_SYM             /* SQL-2003-R */
%token  DISABLE_SYM
%token  DISCARD
%token  DISTINCT                      /* SQL-2003-R */
%token  DIV_SYM
%token  DO_SYM
%token  DOUBLE_SYM                    /* SQL-2003-R */
%token  DROP                          /* SQL-2003-R */
%token  DUMPFILE
%token  DUPLICATE_SYM
%token  DYNAMIC_SYM                   /* SQL-2003-R */
%token  EACH_SYM                      /* SQL-2003-R */
%token  ELSE                          /* SQL-2003-R */
%token  ENABLE_SYM
%token  ENCLOSED
%token  END                           /* SQL-2003-R */
%token  ENDS_SYM
%token  END_OF_INPUT                  /* INTERNAL */
%token  ENGINE_SYM
%token  ENUM_SYM
%token  EQ                            /* OPERATOR */
%token  EQUAL_SYM                     /* OPERATOR */
%token  ERRORS
%token  ESCAPED
%token  ESCAPE_SYM                    /* SQL-2003-R */
%token  EXCLUSIVE_SYM
%token  EXECUTE_SYM                   /* SQL-2003-R */
%token  EXISTS                        /* SQL-2003-R */
%token  EXTENDED_SYM
%token  EXTRACT_SYM                   /* SQL-2003-N */
%token  FALSE_SYM                     /* SQL-2003-R */
%token  FILE_SYM
%token  FIRST_SYM                     /* SQL-2003-N */
%token  FIXED_SYM
%token  FLOAT_NUM
%token  FLUSH_SYM
%token  FORCE_SYM
%token  FOREIGN                       /* SQL-2003-R */
%token  FOR_SYM                       /* SQL-2003-R */
%token  FOUND_SYM                     /* SQL-2003-R */
%token  FRAC_SECOND_SYM
%token  FROM
%token  FULL                          /* SQL-2003-R */
%token  GE
%token  GLOBAL_SYM                    /* SQL-2003-R */
%token  GROUP_SYM                     /* SQL-2003-R */
%token  GROUP_CONCAT_SYM
%token  GT_SYM                        /* OPERATOR */
%token  HASH_SYM
%token  HAVING                        /* SQL-2003-R */
%token  HEX_NUM
%token  HOUR_MICROSECOND_SYM
%token  HOUR_MINUTE_SYM
%token  HOUR_SECOND_SYM
%token  HOUR_SYM                      /* SQL-2003-R */
%token  IDENT
%token  IDENTIFIED_SYM
%token  IDENTITY_SYM                  /* SQL-2003-R */
%token  IDENT_QUOTED
%token  IF
%token  IGNORE_SYM
%token  IMPORT
%token  INDEXES
%token  INDEX_SYM
%token  INFILE
%token  INNER_SYM                     /* SQL-2003-R */
%token  INOUT_SYM                     /* SQL-2003-R */
%token  INSENSITIVE_SYM               /* SQL-2003-R */
%token  INSERT                        /* SQL-2003-R */
%token  INTERVAL_SYM                  /* SQL-2003-R */
%token  INTO                          /* SQL-2003-R */
%token  INT_SYM                       /* SQL-2003-R */
%token  IN_SYM                        /* SQL-2003-R */
%token  IS                            /* SQL-2003-R */
%token  ISOLATION                     /* SQL-2003-R */
%token  ITERATE_SYM
%token  JOIN_SYM                      /* SQL-2003-R */
%token  KEYS
%token  KEY_BLOCK_SIZE
%token  KEY_SYM                       /* SQL-2003-N */
%token  KILL_SYM
%token  LAST_SYM                      /* SQL-2003-N */
%token  LE                            /* OPERATOR */
%token  LEADING                       /* SQL-2003-R */
%token  LEFT                          /* SQL-2003-R */
%token  LEVEL_SYM
%token  LEX_HOSTNAME
%token  LIKE                          /* SQL-2003-R */
%token  LIMIT
%token  LINES
%token  LOAD
%token  LOCAL_SYM                     /* SQL-2003-R */
%token  LOCKS_SYM
%token  LOCK_SYM
%token  LOGS_SYM
%token  LONG_NUM
%token  LONG_SYM
%token  LT                            /* OPERATOR */
%token  MATCH                         /* SQL-2003-R */
%token  MAX_SYM                       /* SQL-2003-N */
%token  MAX_VALUE_SYM                 /* SQL-2003-N */
%token  MEDIUM_SYM
%token  MERGE_SYM                     /* SQL-2003-R */
%token  MICROSECOND_SYM               /* MYSQL-FUNC */
%token  MINUTE_MICROSECOND_SYM
%token  MINUTE_SECOND_SYM
%token  MINUTE_SYM                    /* SQL-2003-R */
%token  MIN_SYM                       /* SQL-2003-N */
%token  MODE_SYM
%token  MODIFIES_SYM                  /* SQL-2003-R */
%token  MODIFY_SYM
%token  MOD_SYM                       /* SQL-2003-N */
%token  MONTH_SYM                     /* SQL-2003-R */
%token  NAMES_SYM                     /* SQL-2003-N */
%token  NAME_SYM                      /* SQL-2003-N */
%token  NATIONAL_SYM                  /* SQL-2003-R */
%token  NATURAL                       /* SQL-2003-R */
%token  NE                            /* OPERATOR */
%token  NEG
%token  NEW_SYM                       /* SQL-2003-R */
%token  NEXT_SYM                      /* SQL-2003-N */
%token  NONE_SYM                      /* SQL-2003-R */
%token  NOT_SYM                       /* SQL-2003-R */
%token  NOW_SYM
%token  NO_SYM                        /* SQL-2003-R */
%token  NULL_SYM                      /* SQL-2003-R */
%token  NUM
%token  NUMERIC_SYM                   /* SQL-2003-R */
%token  OFFLINE_SYM
%token  OFFSET_SYM
%token  ON                            /* SQL-2003-R */
%token  ONE_SHOT_SYM
%token  ONE_SYM
%token  ONLINE_SYM
%token  OPEN_SYM                      /* SQL-2003-R */
%token  OPTIMIZE                      /* Leave assuming we might add it back */
%token  OPTION                        /* SQL-2003-N */
%token  OPTIONALLY
%token  ORDER_SYM                     /* SQL-2003-R */
%token  OR_SYM                        /* SQL-2003-R */
%token  OUTER
%token  OUTFILE
%token  OUT_SYM                       /* SQL-2003-R */
%token  PARTIAL                       /* SQL-2003-N */
%token  POSITION_SYM                  /* SQL-2003-N */
%token  PRECISION                     /* SQL-2003-R */
%token  PREV_SYM
%token  PRIMARY_SYM                   /* SQL-2003-R */
%token  PROCESS
%token  PROCESSLIST_SYM
%token  QUARTER_SYM
%token  QUERY_SYM
%token  RANGE_SYM                     /* SQL-2003-R */
%token  READS_SYM                     /* SQL-2003-R */
%token  READ_SYM                      /* SQL-2003-N */
%token  READ_WRITE_SYM
%token  REAL                          /* SQL-2003-R */
%token  REDUNDANT_SYM
%token  REGEXP_SYM
%token  REFERENCES                    /* SQL-2003-R */
%token  RELEASE_SYM                   /* SQL-2003-R */
%token  RENAME
%token  REPEATABLE_SYM                /* SQL-2003-N */
%token  REPEAT_SYM                    /* MYSQL-FUNC */
%token  REPLACE                       /* MYSQL-FUNC */
%token  RESTRICT
%token  RETURNS_SYM                   /* SQL-2003-R */
%token  RETURN_SYM                    /* SQL-2003-R */
%token  REVOKE                        /* SQL-2003-R */
%token  RIGHT                         /* SQL-2003-R */
%token  ROLLBACK_SYM                  /* SQL-2003-R */
%token  ROLLUP_SYM                    /* SQL-2003-R */
%token  ROUTINE_SYM                   /* SQL-2003-N */
%token  ROWS_SYM                      /* SQL-2003-R */
%token  ROW_FORMAT_SYM
%token  ROW_SYM                       /* SQL-2003-R */
%token  SAVEPOINT_SYM                 /* SQL-2003-R */
%token  SECOND_MICROSECOND_SYM
%token  SECOND_SYM                    /* SQL-2003-R */
%token  SECURITY_SYM                  /* SQL-2003-N */
%token  SELECT_SYM                    /* SQL-2003-R */
%token  SENSITIVE_SYM                 /* FUTURE-USE */
%token  SEPARATOR_SYM
%token  SERIALIZABLE_SYM              /* SQL-2003-N */
%token  SERIAL_SYM
%token  SESSION_SYM                   /* SQL-2003-N */
%token  SERVER_SYM
%token  SET_SYM                           /* SQL-2003-R */
%token  SET_VAR
%token  SHARE_SYM
%token  SHOW
%token  SIGNED_SYM
%token  SIMPLE_SYM                    /* SQL-2003-N */
%token  SNAPSHOT_SYM
%token  SPECIFIC_SYM                  /* SQL-2003-R */
%token  SQLEXCEPTION_SYM              /* SQL-2003-R */
%token  SQLSTATE_SYM                  /* SQL-2003-R */
%token  SQLWARNING_SYM                /* SQL-2003-R */
%token  SQL_BIG_RESULT
%token  SQL_BUFFER_RESULT
%token  SQL_CALC_FOUND_ROWS
%token  SQL_SMALL_RESULT
%token  SQL_SYM                       /* SQL-2003-R */
%token  STARTING
%token  START_SYM                     /* SQL-2003-R */
%token  STATUS_SYM
%token  STDDEV_SAMP_SYM               /* SQL-2003-N */
%token  STD_SYM
%token  STOP_SYM
%token  STORED_SYM
%token  STRAIGHT_JOIN
%token  STRING_SYM
%token  SUBDATE_SYM
%token  SUBJECT_SYM
%token  SUBSTRING                     /* SQL-2003-N */
%token  SUM_SYM                       /* SQL-2003-N */
%token  SUSPEND_SYM
%token  SYSDATE
%token  TABLES
%token  TABLESPACE
%token  TABLE_REF_PRIORITY
%token  TABLE_SYM                     /* SQL-2003-R */
%token  TEMPORARY_SYM                 /* SQL-2003-N */
%token  TERMINATED
%token  TEXT_STRING
%token  TEXT_SYM
%token  THEN_SYM                      /* SQL-2003-R */
%token  TIME_SYM                 /* SQL-2003-R */
%token  TIMESTAMP_SYM                 /* SQL-2003-R */
%token  TIMESTAMP_ADD
%token  TIMESTAMP_DIFF
%token  TO_SYM                        /* SQL-2003-R */
%token  TRAILING                      /* SQL-2003-R */
%token  TRANSACTION_SYM
%token  TRIM                          /* SQL-2003-N */
%token  TRUE_SYM                      /* SQL-2003-R */
%token  TRUNCATE_SYM
%token  TYPE_SYM                      /* SQL-2003-N */
%token  ULONGLONG_NUM
%token  UNCOMMITTED_SYM               /* SQL-2003-N */
%token  UNDOFILE_SYM
%token  UNDO_SYM                      /* FUTURE-USE */
%token  UNION_SYM                     /* SQL-2003-R */
%token  UNIQUE_SYM
%token  UNKNOWN_SYM                   /* SQL-2003-R */
%token  UNLOCK_SYM
%token  UNSIGNED_SYM
%token  UPDATE_SYM                    /* SQL-2003-R */
%token  USAGE                         /* SQL-2003-N */
%token  USER                          /* SQL-2003-R */
%token  USE_SYM
%token  USING                         /* SQL-2003-R */
%token  UTC_DATE_SYM
%token  UTC_TIMESTAMP_SYM
%token  UUID_SYM
%token  VALUES                        /* SQL-2003-R */
%token  VALUE_SYM                     /* SQL-2003-R */
%token  VARBINARY
%token  VARCHAR_SYM                   /* SQL-2003-R */
%token  VARIABLES
%token  VARIANCE_SYM
%token  VARYING                       /* SQL-2003-R */
%token  VAR_SAMP_SYM
%token  WAIT_SYM
%token  WARNINGS
%token  WEEK_SYM
%token  WHEN_SYM                      /* SQL-2003-R */
%token  WHERE                         /* SQL-2003-R */
%token  WITH                          /* SQL-2003-R */
%token  WITH_ROLLUP_SYM               /* INTERNAL */
%token  WORK_SYM                      /* SQL-2003-N */
%token  WRITE_SYM                     /* SQL-2003-N */
%token  XOR
%token  YEAR_MONTH_SYM
%token  YEAR_SYM                      /* SQL-2003-R */
%token  ZEROFILL_SYM

%left   JOIN_SYM INNER_SYM STRAIGHT_JOIN CROSS LEFT RIGHT
/* A dummy token to force the priority of table_ref production in a join. */
%left   TABLE_REF_PRIORITY
%left   SET_VAR
%left   OR_SYM
%left   XOR
%left   AND_SYM
%left   BETWEEN_SYM CASE_SYM WHEN_SYM THEN_SYM ELSE
%left   EQ EQUAL_SYM GE GT_SYM LE LT NE IS LIKE REGEXP_SYM IN_SYM
%left   '-' '+'
%left   '*' '/' '%' DIV_SYM MOD_SYM
%left   NEG
%right  NOT_SYM
%right  BINARY COLLATE_SYM
%left  INTERVAL_SYM

%type <lex_str>
        IDENT IDENT_QUOTED TEXT_STRING DECIMAL_NUM FLOAT_NUM NUM LONG_NUM HEX_NUM
        LEX_HOSTNAME ULONGLONG_NUM field_ident select_alias
        ident
        ident_or_text
        internal_variable_ident
        user_variable_ident
        row_format_or_text
        IDENT_sys TEXT_STRING_sys TEXT_STRING_literal
        schema_name
	catalog_name
        opt_component
        engine_option_value
        savepoint_ident
        BIN_NUM TEXT_STRING_filesystem
        opt_constraint constraint opt_ident

%type <execute_string>
        execute_var_or_string

%type <lex_str_ptr>
        opt_table_alias

%type <table>
        table_ident references

%type <simple_string>
        remember_name remember_end opt_db

%type <string>
        text_string opt_gconcat_separator

%type <field_val>
      field_definition
      int_type
      real_type

%type <boolean>
        opt_wait
        opt_concurrent
        opt_status
        opt_zerofill
        opt_if_not_exists
        if_exists 
        opt_temporary 
        opt_field_number_signed

%type <num>
        order_dir
        field_def
        opt_table_options
        all_or_any opt_distinct
        union_option
        start_transaction_opts opt_chain opt_release
        union_opt select_derived_init option_type2
        kill_option

%type <m_fk_option>
        delete_option

%type <ulong_num>
        ulong_num

%type <ulonglong_number>
        ulonglong_num

%type <lock_type>
        load_data_lock

%type <item>
        literal text_literal insert_ident order_ident
        simple_ident expr opt_expr opt_else sum_expr in_sum_expr
        variable variable_aux bool_pri
        predicate bit_expr
        table_wild simple_expr udf_expr
        expr_or_default set_expr_or_default
        signed_literal now_or_signed_literal opt_escape
        simple_ident_nospvar simple_ident_q
        field_or_var limit_option
        function_call_keyword
        function_call_nonkeyword
        function_call_generic
        function_call_conflict

%type <item_num>
        NUM_literal

%type <item_list>
        expr_list opt_udf_expr_list udf_expr_list when_list

%type <var_type>
        option_type opt_var_type opt_var_ident_type

%type <key_type>
        key_type opt_unique constraint_key_type

%type <key_alg>
        btree_or_rtree

%type <string_list>
        using_list

%type <key_part>
        key_part

%type <table_list>
        join_table_list  join_table
        table_factor table_ref esc_table_ref
        select_derived derived_table_list
        select_derived_union

%type <interval> interval

%type <interval_time_st> interval_time_st

%type <interval_time_st> interval_time_stamp

%type <tx_isolation> isolation_types

%type <cast_type> cast_type

%type <symbol>
        keyword
        keyword_sp
        keyword_exception_for_variable
        row_format

%type <charset>
        collation_name
        collation_name_or_default

%type <variable> internal_variable_name

%type <select_lex> subselect
        get_select_lex query_specification
        query_expression_body

%type <boolfunc2creator> comp_op

%type <build_method> build_method

%type <NONE>
        query verb_clause create select drop insert replace insert2
        insert_values update delete truncate rename
        show describe load alter flush
        begin commit rollback savepoint release
        analyze check start
        field_list field_list_item field_spec kill column_def key_def
        select_item_list select_item values_list no_braces
        opt_limit_clause delete_limit_clause fields opt_values values
        opt_precision opt_ignore opt_column
        set unlock string_list
        ref_list opt_match_clause opt_on_update_delete use
        opt_delete_options opt_delete_option varchar
        opt_outer table_list table_name
        opt_option opt_place
        opt_attribute opt_attribute_list attribute
        flush_options flush_option
        equal optional_braces
        normal_join
        table_to_table_list table_to_table opt_table_list
        single_multi
        union_clause union_list
        precision subselect_start
        subselect_end select_var_list select_var_list_init opt_len
        opt_extended_describe
        statement
        execute
        opt_field_or_var_spec fields_or_vars opt_load_data_set_spec
        init_key_options key_options key_opts key_opt key_using_alg
END_OF_INPUT

%type <index_hint> index_hint_type
%type <num> index_hint_clause
%type <filetype> data_file

%type <NONE>
        '-' '+' '*' '/' '%' '(' ')'
        ',' '!' '{' '}' AND_SYM OR_SYM BETWEEN_SYM CASE_SYM
        THEN_SYM WHEN_SYM DIV_SYM MOD_SYM DELETE_SYM
%%

/*
  Indentation of grammar rules:

rule: <-- starts at col 1
          rule1a rule1b rule1c <-- starts at col 11
          { <-- starts at col 11
            code <-- starts at col 13, indentation is 2 spaces
          }
        | rule2a rule2b
          {
            code
          }
        ; <-- on a line by itself, starts at col 9

  Also, please do not use any <TAB>, but spaces.
  Having a uniform indentation in this file helps
  code reviews, patches, merges, and make maintenance easier.
  Tip: grep [[:cntrl:]] sql_yacc.yy
  Thanks.
*/

query:
          END_OF_INPUT
          {
            Session *session= YYSession;
            if (!(session->lex->select_lex.options & OPTION_FOUND_COMMENT))
            {
              my_message(ER_EMPTY_QUERY, ER(ER_EMPTY_QUERY), MYF(0));
              DRIZZLE_YYABORT;
            }
            else
            {
              session->lex->sql_command= SQLCOM_EMPTY_QUERY;
              session->lex->statement= new statement::EmptyQuery(YYSession);
            }
          }
        | verb_clause END_OF_INPUT {}
        ;

verb_clause:
          statement
        | begin
        ;

/* Verb clauses, except begin */
statement:
          alter
        | analyze
        | check
        | commit
        | create
        | delete
        | describe
        | drop
        | execute
        | flush
        | insert
        | kill
        | load
        | release
        | rename
        | replace
        | rollback
        | savepoint
        | select
        | set
        | show
        | start
        | truncate
        | unlock
        | update
        | use
        ;

/* create a table */

create:
          CREATE CATALOG_SYM catalog_name
          {
            Lex->statement= new statement::catalog::Create(YYSession, $3);
          }
        | CREATE opt_table_options TABLE_SYM opt_if_not_exists table_ident
          {
            Lex->sql_command= SQLCOM_CREATE_TABLE;
            Lex->statement= new statement::CreateTable(YYSession);

            if (not Lex->select_lex.add_table_to_list(YYSession, $5, NULL,
                                                     TL_OPTION_UPDATING,
                                                     TL_WRITE))
              DRIZZLE_YYABORT;
            Lex->col_list.empty();

            Lex->table()->set_name($5->table.str);
	    if ($2)
	      Lex->table()->set_type(message::Table::TEMPORARY);
	    else
	      Lex->table()->set_type(message::Table::STANDARD);
          }
          create_table_definition
          {
            Lex->current_select= &Lex->select_lex;
          }
        | CREATE build_method
          {
            Lex->sql_command= SQLCOM_CREATE_INDEX;
            statement::CreateIndex *statement= new statement::CreateIndex(YYSession);
            Lex->statement= statement;

            statement->alter_info.flags.set(ALTER_ADD_INDEX);
            statement->alter_info.build_method= $2;
            Lex->col_list.empty();
            statement->change=NULL;
          }
          opt_unique INDEX_SYM ident key_alg ON table_ident '(' key_list ')' key_options
          {
            statement::CreateIndex *statement= (statement::CreateIndex *)Lex->statement;

            if (not Lex->current_select->add_table_to_list(Lex->session, $9,
                                                            NULL,
                                                            TL_OPTION_UPDATING))
              DRIZZLE_YYABORT;
            Key *key;
            key= new Key($4, $6, &statement->key_create_info, 0, Lex->col_list);
            statement->alter_info.key_list.push_back(key);
            Lex->col_list.empty();
          }
        | CREATE DATABASE opt_if_not_exists schema_name
          {
            Lex->sql_command=SQLCOM_CREATE_DB;
            Lex->statement= new statement::CreateSchema(YYSession);
          }
          opt_create_database_options
          {
            Lex->name= $4;
          }
        ;

create_table_definition:
          '(' field_list ')' opt_create_table_options  create_select_as
          { }
        | '(' create_select ')'
           {
             Lex->current_select->set_braces(1);
           }
           union_opt {}
        |  '(' create_like ')' opt_create_table_options
          { }
        | create_like opt_create_table_options
          { }
        | opt_create_table_options create_select_as 
          { }
        ;

create_select_as:
          /* empty */ {}
        | opt_duplicate_as create_select
          {
            Lex->current_select->set_braces(0);
          }
          union_clause {}
        | opt_duplicate_as '(' create_select ')'
          {
            Lex->current_select->set_braces(1);
          }
          union_opt {}
        ;

create_like:
          LIKE table_ident
          {
            ((statement::CreateTable *)(YYSession->getLex()->statement))->is_create_table_like= true;

            if (not YYSession->getLex()->select_lex.add_table_to_list(YYSession, $2, NULL, 0, TL_READ))
              DRIZZLE_YYABORT;
          }
        ;

create_select:
          stored_select
          {
          }
        ;

/*
  This rule is used for both CREATE TABLE .. SELECT,  AND INSERT ... SELECT
*/
stored_select:
          SELECT_SYM
          {
            Lex->lock_option= TL_READ;
            if (Lex->sql_command == SQLCOM_INSERT)
            {
              Lex->sql_command= SQLCOM_INSERT_SELECT;
              delete Lex->statement;
              Lex->statement= new statement::InsertSelect(YYSession);
            }
            else if (Lex->sql_command == SQLCOM_REPLACE)
            {
              Lex->sql_command= SQLCOM_REPLACE_SELECT;
              delete Lex->statement;
              Lex->statement= new statement::ReplaceSelect(YYSession);
            }
            /*
              The following work only with the local list, the global list
              is created correctly in this case
            */
            Lex->current_select->table_list.save_and_clear(&Lex->save_list);
            init_select(Lex);
            Lex->current_select->parsing_place= SELECT_LIST;
          }
          select_options select_item_list
          {
            Lex->current_select->parsing_place= NO_MATTER;
          }
          opt_select_from
          {
            /*
              The following work only with the local list, the global list
              is created correctly in this case
            */
            Lex->current_select->table_list.push_front(&Lex->save_list);
          }
        ;

opt_create_database_options:
          /* empty */ {}
        | default_collation_schema {}
        | opt_database_custom_options {}
        ;

opt_database_custom_options:
        custom_database_option
        | custom_database_option ',' opt_database_custom_options
        ;

custom_database_option:
          ident_or_text
        {
          statement::CreateSchema *statement= (statement::CreateSchema *)Lex->statement;
          drizzled::message::Engine::Option *opt= statement->schema_message.mutable_engine()->add_options();

          opt->set_name($1.str);
        }
        | ident_or_text equal ident_or_text
        {
          statement::CreateSchema *statement= (statement::CreateSchema *)Lex->statement;
          drizzled::message::Engine::Option *opt= statement->schema_message.mutable_engine()->add_options();

          opt->set_name($1.str);
          opt->set_state($3.str);
        }
        | ident_or_text equal ulonglong_num
        {
          statement::CreateSchema *statement= (statement::CreateSchema *)Lex->statement;
          char number_as_string[22];

          snprintf(number_as_string, sizeof(number_as_string), "%"PRIu64, $3);

          drizzled::message::Engine::Option *opt= statement->schema_message.mutable_engine()->add_options();

          opt->set_name($1.str);
          opt->set_state(number_as_string);
        }
        ;

opt_table_options:
          /* empty */ { $$= false; }
        | TEMPORARY_SYM { $$= true; }
        ;

opt_if_not_exists:
          /* empty */ { $$= false; }
        | IF not EXISTS { $$= true; YYSession->getLex()->setExists(); }
        ;

opt_create_table_options:
          /* empty */
        | create_table_options
        ;

create_table_options_space_separated:
          create_table_option
        | create_table_option create_table_options_space_separated
        ;

create_table_options:
          create_table_option
        | create_table_option     create_table_options
        | create_table_option ',' create_table_options

create_table_option:
          custom_engine_option;

custom_engine_option:
        ENGINE_SYM equal ident_or_text
          {
            Lex->table()->mutable_engine()->set_name($3.str);
          }
        | COMMENT_SYM opt_equal TEXT_STRING_sys
          {
            Lex->table()->mutable_options()->set_comment($3.str);
          }
        | AUTO_INC opt_equal ulonglong_num
          {
            Lex->table()->mutable_options()->set_auto_increment_value($3);
          }
        |  ROW_FORMAT_SYM equal row_format_or_text
          {
	    drizzled::message::Engine::Option *opt= Lex->table()->mutable_engine()->add_options();

            opt->set_name("ROW_FORMAT");
            opt->set_state($3.str);
          }
        |  FILE_SYM equal TEXT_STRING_sys
          {
	    drizzled::message::Engine::Option *opt= Lex->table()->mutable_engine()->add_options();

            opt->set_name("FILE");
            opt->set_state($3.str);
          }
        |  ident_or_text equal engine_option_value
          {
	    drizzled::message::Engine::Option *opt= Lex->table()->mutable_engine()->add_options();

            opt->set_name($1.str);
            opt->set_state($3.str);
          }
        | ident_or_text equal ulonglong_num
          {
            char number_as_string[22];
            snprintf(number_as_string, sizeof(number_as_string), "%"PRIu64, $3);

	    drizzled::message::Engine::Option *opt= Lex->table()->mutable_engine()->add_options();
            opt->set_name($1.str);
            opt->set_state(number_as_string);
          }
        | default_collation
        ;

default_collation:
          opt_default COLLATE_SYM opt_equal collation_name_or_default
          {
            statement::CreateTable *statement= (statement::CreateTable *)Lex->statement;

            HA_CREATE_INFO *cinfo= &statement->create_info();
            if ((cinfo->used_fields & HA_CREATE_USED_DEFAULT_CHARSET) &&
                 cinfo->default_table_charset && $4 &&
                 !my_charset_same(cinfo->default_table_charset,$4))
              {
                my_error(ER_COLLATION_CHARSET_MISMATCH, MYF(0),
                         $4->name, cinfo->default_table_charset->csname);
                DRIZZLE_YYABORT;
              }
              statement->create_info().default_table_charset= $4;
              statement->create_info().used_fields|= HA_CREATE_USED_DEFAULT_CHARSET;
          }
        ;

default_collation_schema:
          opt_default COLLATE_SYM opt_equal collation_name_or_default
          {
            statement::CreateSchema *statement= (statement::CreateSchema *)Lex->statement;

            message::Schema &schema_message= statement->schema_message;
            schema_message.set_collation($4->name);
          }
        ;

row_format:
          COMPACT_SYM  {}
        | COMPRESSED_SYM  {}
        | DEFAULT  {}
        | DYNAMIC_SYM  {}
        | FIXED_SYM  {}
        | REDUNDANT_SYM  {}
        ;

row_format_or_text:
          row_format
          {
            $$.str= YYSession->strmake($1.str, $1.length);
            $$.length= $1.length;
          }
        ;

opt_select_from:
          opt_limit_clause {}
        | select_from select_lock_type
        ;

field_list:
          field_list_item
        | field_list ',' field_list_item
        ;

field_list_item:
          column_def
        | key_def
        ;

column_def:
          field_spec opt_check_constraint
        | field_spec references
          {
            Lex->col_list.empty(); /* Alloced by memory::sql_alloc */
          }
        ;

key_def:
          key_type opt_ident key_alg '(' key_list ')' key_options
          {
            statement::AlterTable *statement= (statement::AlterTable *)Lex->statement;
            Key *key= new Key($1, $2, &statement->key_create_info, 0,
                              Lex->col_list);
            statement->alter_info.key_list.push_back(key);
            Lex->col_list.empty(); /* Alloced by memory::sql_alloc */
          }
        | opt_constraint constraint_key_type opt_ident key_alg
          '(' key_list ')' key_options
          {
            statement::AlterTable *statement= (statement::AlterTable *)Lex->statement;
            Key *key= new Key($2, $3.str ? $3 : $1, &statement->key_create_info, 0,
                              Lex->col_list);
            statement->alter_info.key_list.push_back(key);
            Lex->col_list.empty(); /* Alloced by memory::sql_alloc */
          }
        | opt_constraint FOREIGN KEY_SYM opt_ident '(' key_list ')' references
          {
            statement::AlterTable *statement= (statement::AlterTable *)Lex->statement;
            Key *key= new Foreign_key($1.str ? $1 : $4, Lex->col_list,
                                      $8,
                                      Lex->ref_list,
                                      statement->fk_delete_opt,
                                      statement->fk_update_opt,
                                      statement->fk_match_option);

            statement->alter_info.key_list.push_back(key);
            key= new Key(Key::MULTIPLE, $1.str ? $1 : $4,
                         &default_key_create_info, 1,
                         Lex->col_list);
            statement->alter_info.key_list.push_back(key);
            Lex->col_list.empty(); /* Alloced by memory::sql_alloc */
            /* Only used for ALTER TABLE. Ignored otherwise. */
            statement->alter_info.flags.set(ALTER_FOREIGN_KEY);
          }
        | constraint opt_check_constraint
          {
            Lex->col_list.empty(); /* Alloced by memory::sql_alloc */
          }
        | opt_constraint check_constraint
          {
            Lex->col_list.empty(); /* Alloced by memory::sql_alloc */
          }
        ;

opt_check_constraint:
          /* empty */
        | check_constraint
        ;

check_constraint:
          CHECK_SYM expr
        ;

opt_constraint:
          /* empty */ { $$= null_lex_str; }
        | constraint { $$= $1; }
        ;

constraint:
          CONSTRAINT opt_ident { $$=$2; }
        ;

field_spec:
          field_ident
          {
            statement::CreateTable *statement= (statement::CreateTable *)Lex->statement;
            Lex->length= Lex->dec=0;
            Lex->type=0;
            statement->default_value= statement->on_update_value= 0;
            statement->comment= null_lex_str;
            Lex->charset= NULL;
            statement->column_format= COLUMN_FORMAT_TYPE_DEFAULT;

            message::AlterTable &alter_proto= ((statement::CreateTable *)Lex->statement)->alter_info.alter_proto;
            Lex->setField(alter_proto.add_added_field());
          }
          field_def
          {
            statement::CreateTable *statement= (statement::CreateTable *)Lex->statement;

            if (Lex->field())
            {
              Lex->field()->set_name($1.str);
            }

            if (add_field_to_list(Lex->session, &$1, (enum enum_field_types) $3,
                                  Lex->length, Lex->dec, Lex->type,
                                  statement->column_format,
                                  statement->default_value, statement->on_update_value,
                                  &statement->comment,
                                  statement->change, &Lex->interval_list, Lex->charset))
              DRIZZLE_YYABORT;

            Lex->setField(NULL);
          }
        ;
field_def:
          field_definition opt_attribute {}
        ;

field_definition:
          int_type ignored_field_number_length opt_field_number_signed opt_zerofill
          { 
            $$= $1;
            Lex->length=(char*) 0; /* use default length */

            if ($3 or $4)
            {
              $1= DRIZZLE_TYPE_LONGLONG;
            }

            if (Lex->field())
            {
              assert ($1 == DRIZZLE_TYPE_LONG or $1 == DRIZZLE_TYPE_LONGLONG);
              // We update the type for unsigned types
              if ($3 or $4)
              {
                Lex->field()->set_type(message::Table::Field::BIGINT);
                Lex->field()->mutable_constraints()->set_is_unsigned(true);
              }
              if ($1 == DRIZZLE_TYPE_LONG)
              {
                Lex->field()->set_type(message::Table::Field::INTEGER);
              }
              else if ($1 == DRIZZLE_TYPE_LONGLONG)
              {
                Lex->field()->set_type(message::Table::Field::BIGINT);
              }
            }
          }
        | real_type opt_precision
          {
            $$=$1;

            if (Lex->field())
            {
              assert ($1 == DRIZZLE_TYPE_DOUBLE);
              Lex->field()->set_type(message::Table::Field::DOUBLE);
            }
          }
          | char '(' NUM ')'
            {
              Lex->length=$3.str;
              $$=DRIZZLE_TYPE_VARCHAR;

            if (Lex->field())
            {
              Lex->field()->set_type(message::Table::Field::VARCHAR);
              message::Table::Field::StringFieldOptions *string_field_options;

              string_field_options= Lex->field()->mutable_string_options();

              string_field_options->set_length(atoi($3.str));
            }
            }
          | char
            {
              Lex->length=(char*) "1";
              $$=DRIZZLE_TYPE_VARCHAR;

            if (Lex->field())
              Lex->field()->set_type(message::Table::Field::VARCHAR);
            }
          | varchar '(' NUM ')'
            {
              Lex->length=$3.str;
              $$= DRIZZLE_TYPE_VARCHAR;

            if (Lex->field())
	    {
              Lex->field()->set_type(message::Table::Field::VARCHAR);

              message::Table::Field::StringFieldOptions *string_field_options;

              string_field_options= Lex->field()->mutable_string_options();

              string_field_options->set_length(atoi($3.str));
            }
            }
          | VARBINARY '(' NUM ')'
            {
              Lex->length=$3.str;
              Lex->charset=&my_charset_bin;
              $$= DRIZZLE_TYPE_VARCHAR;

            if (Lex->field())
	    {
              Lex->field()->set_type(message::Table::Field::VARCHAR);
              message::Table::Field::StringFieldOptions *string_field_options;

              string_field_options= Lex->field()->mutable_string_options();

              string_field_options->set_length(atoi($3.str));
              string_field_options->set_collation_id(my_charset_bin.number);
              string_field_options->set_collation(my_charset_bin.name);
            }
            }
          | DATE_SYM
            {
              $$=DRIZZLE_TYPE_DATE;

              if (Lex->field())
                Lex->field()->set_type(message::Table::Field::DATE);
            }
          | TIME_SYM
            {
              $$=DRIZZLE_TYPE_TIME;

              if (Lex->field())
                Lex->field()->set_type(message::Table::Field::TIME);
            }
          | TIMESTAMP_SYM
            {
              $$=DRIZZLE_TYPE_TIMESTAMP;
              Lex->length= 0;

              if (Lex->field())
                Lex->field()->set_type(message::Table::Field::EPOCH);
            }
          | TIMESTAMP_SYM '(' NUM ')'
            {
              $$=DRIZZLE_TYPE_MICROTIME;
              Lex->length= $3.str;

              if (Lex->field())
                Lex->field()->set_type(message::Table::Field::EPOCH);
            }
          | DATETIME_SYM
            {
              $$=DRIZZLE_TYPE_DATETIME;

              if (Lex->field())
                Lex->field()->set_type(message::Table::Field::DATETIME);
            }
          | BLOB_SYM
            {
              Lex->charset=&my_charset_bin;
              $$=DRIZZLE_TYPE_BLOB;
              Lex->length=(char*) 0; /* use default length */

              if (Lex->field())
              {
                Lex->field()->set_type(message::Table::Field::BLOB);
                message::Table::Field::StringFieldOptions *string_field_options;

                string_field_options= Lex->field()->mutable_string_options();
                string_field_options->set_collation_id(my_charset_bin.number);
                string_field_options->set_collation(my_charset_bin.name);
              }
            }
          | TEXT_SYM
            {
              $$=DRIZZLE_TYPE_BLOB;
              Lex->length=(char*) 0; /* use default length */

            if (Lex->field())
              Lex->field()->set_type(message::Table::Field::BLOB);
            }
          | DECIMAL_SYM float_options
          {
            $$=DRIZZLE_TYPE_DECIMAL;

            if (Lex->field())
              Lex->field()->set_type(message::Table::Field::DECIMAL);
          }
          | NUMERIC_SYM float_options
          {
            $$=DRIZZLE_TYPE_DECIMAL;

            if (Lex->field())
              Lex->field()->set_type(message::Table::Field::DECIMAL);
          }
          | FIXED_SYM float_options
          {
            $$=DRIZZLE_TYPE_DECIMAL;

            if (Lex->field())
              Lex->field()->set_type(message::Table::Field::DECIMAL);
          }
          | ENUM_SYM
            {Lex->interval_list.empty();}
            '(' string_list ')'
          {
            $$=DRIZZLE_TYPE_ENUM;

            if (Lex->field())
              Lex->field()->set_type(message::Table::Field::ENUM);
          }
        | UUID_SYM
          {
            $$=DRIZZLE_TYPE_UUID;

            if (Lex->field())
              Lex->field()->set_type(message::Table::Field::UUID);
          }
        | BOOLEAN_SYM
          {
            $$=DRIZZLE_TYPE_BOOLEAN;

            if (Lex->field())
              Lex->field()->set_type(message::Table::Field::BOOLEAN);
          }
        | SERIAL_SYM
          {
            $$=DRIZZLE_TYPE_LONGLONG;
            Lex->type|= (AUTO_INCREMENT_FLAG | NOT_NULL_FLAG | UNIQUE_FLAG);

            if (Lex->field())
            {
              message::Table::Field::FieldConstraints *constraints;
              constraints= Lex->field()->mutable_constraints();
              constraints->set_is_notnull(true);

              Lex->field()->set_type(message::Table::Field::BIGINT);
            }
          }
        ;

char:
          CHAR_SYM {}
        ;

varchar:
          char VARYING {}
        | VARCHAR_SYM {}
        ;

int_type:
          INT_SYM    { $$=DRIZZLE_TYPE_LONG; }
        | BIGINT_SYM { $$=DRIZZLE_TYPE_LONGLONG; }
        ;

real_type:
          REAL
          {
            $$= DRIZZLE_TYPE_DOUBLE;
          }
        | DOUBLE_SYM
          { $$=DRIZZLE_TYPE_DOUBLE; }
        | DOUBLE_SYM PRECISION
          { $$=DRIZZLE_TYPE_DOUBLE; }
        ;

float_options:
          /* empty */
          { Lex->dec=Lex->length= (char*)0; }
        | '(' NUM ')'
          { Lex->length=$2.str; Lex->dec= (char*)0; }
        | precision
          {}
        ;

precision:
          '(' NUM ',' NUM ')'
          {
            Lex->length= $2.str;
            Lex->dec= $4.str;
          }
        ;

opt_len:
          /* empty */ { Lex->length=(char*) 0; /* use default length */ }
        | '(' NUM ')' { Lex->length= $2.str; }
        ;

opt_field_number_signed:
          /* empty */ { $$= false; }
        | SIGNED_SYM { $$= false; }
        | UNSIGNED_SYM { $$= true; Lex->type|= UNSIGNED_FLAG; }
        ;

ignored_field_number_length:
          /* empty */ { }
        | '(' NUM ')' { }
        ;

opt_zerofill:
          /* empty */ { $$= false; }
        | ZEROFILL_SYM { $$= true; Lex->type|= UNSIGNED_FLAG; }
        ;

opt_precision:
          /* empty */ {}
        | precision {}
        ;

opt_attribute:
          /* empty */ {}
        | opt_attribute_list {}
        ;

opt_attribute_list:
          opt_attribute_list attribute {}
        | attribute
        ;

attribute:
          NULL_SYM
          {
            Lex->type&= ~ NOT_NULL_FLAG;

            if (Lex->field())
            {
              message::Table::Field::FieldConstraints *constraints;
              constraints= Lex->field()->mutable_constraints();
              constraints->set_is_notnull(false);
            }
          }
        | not NULL_SYM
          {
            Lex->type|= NOT_NULL_FLAG;

            if (Lex->field())
            {
              message::Table::Field::FieldConstraints *constraints;
              constraints= Lex->field()->mutable_constraints();
              constraints->set_is_notnull(true);
            }
          }
        | DEFAULT now_or_signed_literal
          {
            statement::AlterTable *statement= (statement::AlterTable *)Lex->statement;

            statement->default_value=$2;
            statement->alter_info.flags.set(ALTER_COLUMN_DEFAULT);
          }
        | ON UPDATE_SYM NOW_SYM optional_braces
          {
            ((statement::AlterTable *)Lex->statement)->on_update_value= new Item_func_now_local();
          }
        | AUTO_INC
          {
            Lex->type|= AUTO_INCREMENT_FLAG | NOT_NULL_FLAG;

            if (Lex->field())
            {
              message::Table::Field::FieldConstraints *constraints;

              constraints= Lex->field()->mutable_constraints();
              constraints->set_is_notnull(true);
            }
          }
        | SERIAL_SYM DEFAULT VALUE_SYM
          {
            statement::AlterTable *statement= (statement::AlterTable *)Lex->statement;

            Lex->type|= AUTO_INCREMENT_FLAG | NOT_NULL_FLAG | UNIQUE_FLAG;
            statement->alter_info.flags.set(ALTER_ADD_INDEX);

            if (Lex->field())
            {
              message::Table::Field::FieldConstraints *constraints;
              constraints= Lex->field()->mutable_constraints();
              constraints->set_is_notnull(true);
            }
          }
        | opt_primary KEY_SYM
          {
            statement::AlterTable *statement= (statement::AlterTable *)Lex->statement;

            Lex->type|= PRI_KEY_FLAG | NOT_NULL_FLAG;
            statement->alter_info.flags.set(ALTER_ADD_INDEX);

            if (Lex->field())
            {
              message::Table::Field::FieldConstraints *constraints;
              constraints= Lex->field()->mutable_constraints();
              constraints->set_is_notnull(true);
            }
          }
        | UNIQUE_SYM
          {
            statement::AlterTable *statement= (statement::AlterTable *)Lex->statement;

            Lex->type|= UNIQUE_FLAG;
            statement->alter_info.flags.set(ALTER_ADD_INDEX);
          }
        | UNIQUE_SYM KEY_SYM
          {
            statement::AlterTable *statement= (statement::AlterTable *)Lex->statement;

            Lex->type|= UNIQUE_KEY_FLAG;
            statement->alter_info.flags.set(ALTER_ADD_INDEX);
          }
        | COMMENT_SYM TEXT_STRING_sys
          {
            statement::AlterTable *statement= (statement::AlterTable *)Lex->statement;
            statement->comment= $2;

            if (Lex->field())
              Lex->field()->set_comment($2.str);
          }
        | COLLATE_SYM collation_name
          {
            if (Lex->charset && !my_charset_same(Lex->charset,$2))
            {
              my_error(ER_COLLATION_CHARSET_MISMATCH, MYF(0),
                       $2->name,Lex->charset->csname);
              DRIZZLE_YYABORT;
            }
            else
            {
              Lex->charset=$2;
            }
          }
        ;

now_or_signed_literal:
          NOW_SYM optional_braces
          { $$= new Item_func_now_local(); }
        | signed_literal
          { $$=$1; }
        ;

collation_name:
          ident_or_text
          {
            if (!($$=get_charset_by_name($1.str)))
            {
              my_error(ER_UNKNOWN_COLLATION, MYF(0), $1.str);
              DRIZZLE_YYABORT;
            }
          }
        ;

collation_name_or_default:
          collation_name { $$=$1; }
        | DEFAULT    { $$=NULL; }
        ;

opt_default:
          /* empty */ {}
        | DEFAULT {}
        ;

opt_primary:
          /* empty */
        | PRIMARY_SYM
        ;

references:
          REFERENCES
          table_ident
          opt_ref_list
          opt_match_clause
          opt_on_update_delete
          {
            $$=$2;
          }
        ;

opt_ref_list:
          /* empty */
          { Lex->ref_list.empty(); }
        | '(' ref_list ')'
        ;

ref_list:
          ref_list ',' ident
          { Lex->ref_list.push_back(new Key_part_spec($3, 0)); }
        | ident
          {
            Lex->ref_list.empty();
            Lex->ref_list.push_back(new Key_part_spec($1, 0));
          }
        ;

opt_match_clause:
          /* empty */
          { ((statement::CreateTable *)Lex->statement)->fk_match_option= drizzled::message::Table::ForeignKeyConstraint::MATCH_UNDEFINED; }
        | MATCH FULL
          { ((statement::CreateTable *)Lex->statement)->fk_match_option= drizzled::message::Table::ForeignKeyConstraint::MATCH_FULL; }
        | MATCH PARTIAL
          { ((statement::CreateTable *)Lex->statement)->fk_match_option= drizzled::message::Table::ForeignKeyConstraint::MATCH_PARTIAL; }
        | MATCH SIMPLE_SYM
          { ((statement::CreateTable *)Lex->statement)->fk_match_option= drizzled::message::Table::ForeignKeyConstraint::MATCH_SIMPLE; }
        ;

opt_on_update_delete:
          /* empty */
          {
            ((statement::CreateTable *)Lex->statement)->fk_update_opt= drizzled::message::Table::ForeignKeyConstraint::OPTION_UNDEF;
            ((statement::CreateTable *)Lex->statement)->fk_delete_opt= drizzled::message::Table::ForeignKeyConstraint::OPTION_UNDEF;
          }
        | ON UPDATE_SYM delete_option
          {
            ((statement::CreateTable *)Lex->statement)->fk_update_opt= $3;
            ((statement::CreateTable *)Lex->statement)->fk_delete_opt= drizzled::message::Table::ForeignKeyConstraint::OPTION_UNDEF;
          }
        | ON DELETE_SYM delete_option
          {
            ((statement::CreateTable *)Lex->statement)->fk_update_opt= drizzled::message::Table::ForeignKeyConstraint::OPTION_UNDEF;
            ((statement::CreateTable *)Lex->statement)->fk_delete_opt= $3;
          }
        | ON UPDATE_SYM delete_option
          ON DELETE_SYM delete_option
          {
            ((statement::CreateTable *)Lex->statement)->fk_update_opt= $3;
            ((statement::CreateTable *)Lex->statement)->fk_delete_opt= $6;
          }
        | ON DELETE_SYM delete_option
          ON UPDATE_SYM delete_option
          {
            ((statement::CreateTable *)Lex->statement)->fk_update_opt= $6;
            ((statement::CreateTable *)Lex->statement)->fk_delete_opt= $3;
          }
        ;

delete_option:
          RESTRICT      { $$= drizzled::message::Table::ForeignKeyConstraint::OPTION_RESTRICT; }
        | CASCADE       { $$= drizzled::message::Table::ForeignKeyConstraint::OPTION_CASCADE; }
        | SET_SYM NULL_SYM  { $$= drizzled::message::Table::ForeignKeyConstraint::OPTION_SET_NULL; }
        | NO_SYM ACTION { $$= drizzled::message::Table::ForeignKeyConstraint::OPTION_NO_ACTION; }
        | SET_SYM DEFAULT   { $$= drizzled::message::Table::ForeignKeyConstraint::OPTION_SET_DEFAULT;  }
        ;

key_type:
          key_or_index { $$= Key::MULTIPLE; }
        ;

constraint_key_type:
          PRIMARY_SYM KEY_SYM { $$= Key::PRIMARY; }
        | UNIQUE_SYM opt_key_or_index { $$= Key::UNIQUE; }
        ;

key_or_index:
          KEY_SYM {}
        | INDEX_SYM {}
        ;

opt_key_or_index:
          /* empty */ {}
        | key_or_index
        ;

keys_or_index:
          KEYS {}
        | INDEX_SYM {}
        | INDEXES {}
        ;

opt_unique:
          /* empty */  { $$= Key::MULTIPLE; }
        | UNIQUE_SYM   { $$= Key::UNIQUE; }
        ;

init_key_options:
          {
            ((statement::CreateTable *)Lex->statement)->key_create_info= default_key_create_info;
          }
        ;

/*
  For now, key_alg initializies Lex->key_create_info.
  In the future, when all key options are after key definition,
  we can remove key_alg and move init_key_options to key_options
*/

key_alg:
          init_key_options
        | init_key_options key_using_alg
        ;

key_options:
          /* empty */ {}
        | key_opts
        ;

key_opts:
          key_opt
        | key_opts key_opt
        ;

key_using_alg:
          USING btree_or_rtree     { ((statement::CreateTable *)Lex->statement)->key_create_info.algorithm= $2; }
        ;

key_opt:
          key_using_alg
        | KEY_BLOCK_SIZE opt_equal ulong_num
          { ((statement::CreateTable *)Lex->statement)->key_create_info.block_size= $3; }
        | COMMENT_SYM TEXT_STRING_sys
          { ((statement::CreateTable *)Lex->statement)->key_create_info.comment= $2; }
        ;

btree_or_rtree:
          BTREE_SYM { $$= HA_KEY_ALG_BTREE; }
        | HASH_SYM  { $$= HA_KEY_ALG_HASH; }
        ;

key_list:
          key_list ',' key_part order_dir { Lex->col_list.push_back($3); }
        | key_part order_dir { Lex->col_list.push_back($1); }
        ;

key_part:
          ident { $$=new Key_part_spec($1, 0); }
        | ident '(' NUM ')'
          {
            int key_part_len= atoi($3.str);
            if (!key_part_len)
            {
              my_error(ER_KEY_PART_0, MYF(0), $1.str);
            }
            $$=new Key_part_spec($1, (uint) key_part_len);
          }
        ;

opt_ident:
          /* empty */ { $$= null_lex_str; }
        | field_ident { $$= $1; }
        ;

opt_component:
          /* empty */    { $$= null_lex_str; }
        | '.' ident      { $$= $2; }
        ;

string_list:
          text_string { Lex->interval_list.push_back($1); }
        | string_list ',' text_string { Lex->interval_list.push_back($3); };

/*
** Alter table
*/

alter:
          ALTER_SYM build_method opt_ignore TABLE_SYM table_ident
          {
            Lex->sql_command= SQLCOM_ALTER_TABLE;
            statement::AlterTable *statement= new statement::AlterTable(YYSession);
            Lex->statement= statement;
            Lex->duplicates= DUP_ERROR;
            if (not Lex->select_lex.add_table_to_list(YYSession, $5, NULL,
                                                     TL_OPTION_UPDATING))
              DRIZZLE_YYABORT;

            Lex->col_list.empty();
            Lex->select_lex.init_order();
            Lex->select_lex.db= const_cast<char *>(((TableList*) Lex->select_lex.table_list.first)->getSchemaName());
            statement->alter_info.build_method= $2;
          }
          alter_commands
          {}
        | ALTER_SYM DATABASE schema_name
          {
            Lex->sql_command=SQLCOM_ALTER_DB;
            Lex->statement= new statement::AlterSchema(YYSession);
          }
          default_collation_schema
          {
            Lex->name= $3;
            if (Lex->name.str == NULL && Lex->copy_db_to(&Lex->name.str, &Lex->name.length))
              DRIZZLE_YYABORT;
          }
        ;

alter_commands:
          /* empty */
        | DISCARD TABLESPACE
          {
            statement::AlterTable *statement= (statement::AlterTable *)Lex->statement;
            statement->alter_info.tablespace_op= DISCARD_TABLESPACE;
          }
        | IMPORT TABLESPACE
          {
            statement::AlterTable *statement= (statement::AlterTable *)Lex->statement;
            statement->alter_info.tablespace_op= IMPORT_TABLESPACE;
          }
        | alter_list
        ;

build_method:
        /* empty */
          {
            $$= HA_BUILD_DEFAULT;
          }
        | ONLINE_SYM
          {
            $$= HA_BUILD_ONLINE;
          }
        | OFFLINE_SYM
          {
            $$= HA_BUILD_OFFLINE;
          }
        ;

alter_list:
          alter_list_item
        | alter_list ',' alter_list_item
        ;

add_column:
          ADD_SYM opt_column
          {
            statement::AlterTable *statement= (statement::AlterTable *)Lex->statement;

            statement->change=0;
            statement->alter_info.flags.set(ALTER_ADD_COLUMN);
          }
        ;

alter_list_item:
          add_column column_def opt_place { }
        | ADD_SYM key_def
          {
            statement::AlterTable *statement= (statement::AlterTable *)Lex->statement;

            statement->alter_info.flags.set(ALTER_ADD_INDEX);
          }
        | add_column '(' field_list ')'
          {
            statement::AlterTable *statement= (statement::AlterTable *)Lex->statement;

            statement->alter_info.flags.set(ALTER_ADD_COLUMN);
            statement->alter_info.flags.set(ALTER_ADD_INDEX);
          }
        | CHANGE_SYM opt_column field_ident
          {
            statement::AlterTable *statement= (statement::AlterTable *)Lex->statement;
            statement->change= $3.str;
            statement->alter_info.flags.set(ALTER_CHANGE_COLUMN);
          }
          field_spec opt_place
        | MODIFY_SYM opt_column field_ident
          {
            statement::AlterTable *statement= (statement::AlterTable *)Lex->statement;
            Lex->length= Lex->dec=0;
            Lex->type= 0;
            statement->default_value= statement->on_update_value= 0;
            statement->comment= null_lex_str;
            Lex->charset= NULL;
            statement->alter_info.flags.set(ALTER_CHANGE_COLUMN);
            statement->column_format= COLUMN_FORMAT_TYPE_DEFAULT;

            Lex->setField(NULL);
          }
          field_def
          {
            statement::AlterTable *statement= (statement::AlterTable *)Lex->statement;

            if (add_field_to_list(Lex->session,&$3,
                                  (enum enum_field_types) $5,
                                  Lex->length, Lex->dec, Lex->type,
                                  statement->column_format,
                                  statement->default_value,
                                  statement->on_update_value,
                                  &statement->comment,
                                  $3.str, &Lex->interval_list, Lex->charset))
              DRIZZLE_YYABORT;
          }
          opt_place
        | DROP opt_column field_ident
          {
            statement::AlterTable *statement= (statement::AlterTable *)Lex->statement;

            statement->alter_info.drop_list.push_back(new AlterDrop(AlterDrop::COLUMN, $3.str));
            statement->alter_info.flags.set(ALTER_DROP_COLUMN);
          }
        | DROP FOREIGN KEY_SYM opt_ident
          {
            statement::AlterTable *statement= (statement::AlterTable *)Lex->statement;
            statement->alter_info.drop_list.push_back(new AlterDrop(AlterDrop::FOREIGN_KEY,
                                                                    $4.str));
            statement->alter_info.flags.set(ALTER_DROP_INDEX);
            statement->alter_info.flags.set(ALTER_FOREIGN_KEY);
          }
        | DROP PRIMARY_SYM KEY_SYM
          {
            statement::AlterTable *statement= (statement::AlterTable *)Lex->statement;

            statement->alter_info.drop_list.push_back(new AlterDrop(AlterDrop::KEY,
                                                               "PRIMARY"));
            statement->alter_info.flags.set(ALTER_DROP_INDEX);
          }
        | DROP key_or_index field_ident
          {
            statement::AlterTable *statement= (statement::AlterTable *)Lex->statement;

            statement->alter_info.drop_list.push_back(new AlterDrop(AlterDrop::KEY,
                                                                    $3.str));
            statement->alter_info.flags.set(ALTER_DROP_INDEX);
          }
        | DISABLE_SYM KEYS
          {
            statement::AlterTable *statement= (statement::AlterTable *)Lex->statement;

            statement->alter_info.keys_onoff= DISABLE;
            statement->alter_info.flags.set(ALTER_KEYS_ONOFF);
          }
        | ENABLE_SYM KEYS
          {
            statement::AlterTable *statement= (statement::AlterTable *)Lex->statement;

            statement->alter_info.keys_onoff= ENABLE;
            statement->alter_info.flags.set(ALTER_KEYS_ONOFF);
          }
        | ALTER_SYM opt_column field_ident SET_SYM DEFAULT signed_literal
          {
            statement::AlterTable *statement= (statement::AlterTable *)Lex->statement;

            statement->alter_info.alter_list.push_back(new AlterColumn($3.str,$6));
            statement->alter_info.flags.set(ALTER_COLUMN_DEFAULT);
          }
        | ALTER_SYM opt_column field_ident DROP DEFAULT
          {
            statement::AlterTable *statement= (statement::AlterTable *)Lex->statement;

            statement->alter_info.alter_list.push_back(new AlterColumn($3.str, (Item*) 0));
            statement->alter_info.flags.set(ALTER_COLUMN_DEFAULT);
          }
        | RENAME opt_to table_ident
          {
            statement::AlterTable *statement= (statement::AlterTable *)Lex->statement;
            size_t dummy;

            Lex->select_lex.db=$3->db.str;
            if (Lex->select_lex.db == NULL &&
                Lex->copy_db_to(&Lex->select_lex.db, &dummy))
            {
              DRIZZLE_YYABORT;
            }

            if (check_table_name($3->table.str,$3->table.length))
            {
              my_error(ER_WRONG_TABLE_NAME, MYF(0), $3->table.str);
              DRIZZLE_YYABORT;
            }

            Lex->name= $3->table;
            statement->alter_info.flags.set(ALTER_RENAME);
          }
        | CONVERT_SYM TO_SYM collation_name_or_default
          {
            statement::AlterTable *statement= (statement::AlterTable *)Lex->statement;

            statement->create_info().table_charset=
            statement->create_info().default_table_charset= $3;
            statement->create_info().used_fields|= (HA_CREATE_USED_CHARSET |
              HA_CREATE_USED_DEFAULT_CHARSET);
            statement->alter_info.flags.set(ALTER_CONVERT);
          }
        | create_table_options_space_separated
          {
            statement::AlterTable *statement= (statement::AlterTable *)Lex->statement;

            statement->alter_info.flags.set(ALTER_OPTIONS);
          }
        | FORCE_SYM
          {
            statement::AlterTable *statement= (statement::AlterTable *)Lex->statement;

            statement->alter_info.flags.set(ALTER_FORCE);
          }
        | alter_order_clause
          {
            statement::AlterTable *statement= (statement::AlterTable *)Lex->statement;

            statement->alter_info.flags.set(ALTER_ORDER);
          }
        ;

opt_column:
          /* empty */ {}
        | COLUMN_SYM {}
        ;

opt_ignore:
          /* empty */ { Lex->ignore= 0;}
        | IGNORE_SYM { Lex->ignore= 1;}
        ;

opt_place:
          /* empty */ {}
        | AFTER_SYM ident
          {
            statement::AlterTable *statement= (statement::AlterTable *)Lex->statement;

            store_position_for_column($2.str);
            statement->alter_info.flags.set(ALTER_COLUMN_ORDER);
          }
        | FIRST_SYM
          {
            statement::AlterTable *statement= (statement::AlterTable *)Lex->statement;

            store_position_for_column(first_keyword);
            statement->alter_info.flags.set(ALTER_COLUMN_ORDER);
          }
        ;

opt_to:
          /* empty */ {}
        | TO_SYM {}
        | EQ {}
        | AS {}
        ;

start:
          START_SYM TRANSACTION_SYM start_transaction_opts
          {
            Lex->sql_command= SQLCOM_BEGIN;
            Lex->statement= new statement::StartTransaction(YYSession, (start_transaction_option_t)$3);
          }
        ;

start_transaction_opts:
          /*empty*/ { $$ = START_TRANS_NO_OPTIONS; }
        | WITH CONSISTENT_SYM SNAPSHOT_SYM
          {
            $$= START_TRANS_OPT_WITH_CONS_SNAPSHOT;
          }
        ;

analyze:
          ANALYZE_SYM table_or_tables
          {
            Lex->sql_command = SQLCOM_ANALYZE;
            Lex->statement= new statement::Analyze(YYSession);
          }
          table_list
          {}
        ;

check:
          CHECK_SYM table_or_tables
          {
            Lex->sql_command = SQLCOM_CHECK;
            Lex->statement= new statement::Check(YYSession);
          }
          table_list
          {}
        ;

rename:
          RENAME table_or_tables
          {
            Lex->sql_command= SQLCOM_RENAME_TABLE;
            Lex->statement= new statement::RenameTable(YYSession);
          }
          table_to_table_list
          {}
        ;

table_to_table_list:
          table_to_table
        | table_to_table_list ',' table_to_table
        ;

table_to_table:
          table_ident TO_SYM table_ident
          {
            Select_Lex *sl= Lex->current_select;
            if (!sl->add_table_to_list(Lex->session, $1,NULL,TL_OPTION_UPDATING,
                                       TL_IGNORE) ||
                !sl->add_table_to_list(Lex->session, $3,NULL,TL_OPTION_UPDATING,
                                       TL_IGNORE))
              DRIZZLE_YYABORT;
          }
        ;

/*
  Select : retrieve data from table
*/


select:
          select_init
          {
            Lex->sql_command= SQLCOM_SELECT;
            Lex->statement= new statement::Select(YYSession);
          }
        ;

/* Need select_init2 for subselects. */
select_init:
          SELECT_SYM select_init2
        | '(' select_paren ')' union_opt
        ;

select_paren:
          SELECT_SYM select_part2
          {
            if (setup_select_in_parentheses(YYSession, Lex))
              DRIZZLE_YYABORT;
          }
        | '(' select_paren ')'
        ;

/* The equivalent of select_paren for nested queries. */
select_paren_derived:
          SELECT_SYM select_part2_derived
          {
            if (setup_select_in_parentheses(YYSession, Lex))
              DRIZZLE_YYABORT;
          }
        | '(' select_paren_derived ')'
        ;

select_init2:
          select_part2
          {
            Select_Lex * sel= Lex->current_select;
            if (Lex->current_select->set_braces(0))
            {
              struct my_parse_error_st pass= { ER(ER_SYNTAX_ERROR), YYSession };
              my_parse_error(&pass);
              DRIZZLE_YYABORT;
            }
            if (sel->linkage == UNION_TYPE &&
                sel->master_unit()->first_select()->braces)
            {
              struct my_parse_error_st pass= { ER(ER_SYNTAX_ERROR), YYSession };
              my_parse_error(&pass);
              DRIZZLE_YYABORT;
            }
          }
          union_clause
        ;

select_part2:
          {
            Select_Lex *sel= Lex->current_select;
            if (sel->linkage != UNION_TYPE)
              init_select(Lex);
            Lex->current_select->parsing_place= SELECT_LIST;
          }
          select_options select_item_list
          {
            Lex->current_select->parsing_place= NO_MATTER;
          }
          select_into select_lock_type
        ;

select_into:
          opt_order_clause opt_limit_clause {}
        | into
        | select_from
        | into select_from
        | select_from into
        ;

select_from:
          FROM join_table_list where_clause group_clause having_clause
          opt_order_clause opt_limit_clause
          {
            Lex->current_select->context.table_list=
              Lex->current_select->context.first_name_resolution_table=
                reinterpret_cast<TableList *>(Lex->current_select->table_list.first);
          }
        ;

select_options:
          /* empty*/
        | select_option_list
          { }
        ;

select_option_list:
          select_option_list select_option
        | select_option
        ;

select_option_distinct_or_all:
          DISTINCT
          {
            Lex->current_select->options|= SELECT_DISTINCT; 

            if (Lex->current_select->options & SELECT_DISTINCT && Lex->current_select->options & SELECT_ALL)
            {
              my_error(ER_WRONG_USAGE, MYF(0), "ALL", "DISTINCT");
              DRIZZLE_YYABORT;
            }
          }
        | ALL
          {
            Lex->current_select->options|= SELECT_ALL; 

            if (Lex->current_select->options & SELECT_DISTINCT && Lex->current_select->options & SELECT_ALL)
            {
              my_error(ER_WRONG_USAGE, MYF(0), "ALL", "DISTINCT");
              DRIZZLE_YYABORT;
            }
          }
        ;

select_option_small_or_big:
          SQL_SMALL_RESULT
          {
            Lex->current_select->options|= SELECT_SMALL_RESULT;

            if (Lex->current_select->options & SELECT_SMALL_RESULT && Lex->current_select->options & SELECT_BIG_RESULT)
            {
              my_error(ER_WRONG_USAGE, MYF(0), "SELECT_SMALL_RESULT", "SELECT_SMALL_RESULT");
              DRIZZLE_YYABORT;
            }
          }
        | SQL_BIG_RESULT
          {
            Lex->current_select->options|= SELECT_BIG_RESULT;

            if (Lex->current_select->options & SELECT_SMALL_RESULT && Lex->current_select->options & SELECT_BIG_RESULT)
            {
              my_error(ER_WRONG_USAGE, MYF(0), "SELECT_SMALL_RESULT", "SELECT_SMALL_RESULT");
              DRIZZLE_YYABORT;
            }
          }
        ;


select_option:
          STRAIGHT_JOIN { Lex->current_select->options|= SELECT_STRAIGHT_JOIN; }
        | SQL_BUFFER_RESULT
          {
            if (check_simple_select(YYSession))
              DRIZZLE_YYABORT;
            Lex->current_select->options|= OPTION_BUFFER_RESULT;
          }
        | select_option_small_or_big
          { }
        | select_option_distinct_or_all
          { }
        | SQL_CALC_FOUND_ROWS
          {
            if (check_simple_select(YYSession))
              DRIZZLE_YYABORT;
            Lex->current_select->options|= OPTION_FOUND_ROWS;
          }
        ;

select_lock_type:
          /* empty */
        | FOR_SYM UPDATE_SYM
          {
            Lex->current_select->set_lock_for_tables(TL_WRITE);
          }
        | LOCK_SYM IN_SYM SHARE_SYM MODE_SYM
          {
            Lex->current_select->
              set_lock_for_tables(TL_READ_WITH_SHARED_LOCKS);
          }
        ;

select_item_list:
          select_item_list ',' select_item
        | select_item
        | '*'
          {
            if (YYSession->add_item_to_list( new Item_field(&YYSession->lex->current_select->
                                                          context,
                                                          NULL, NULL, "*")))
              DRIZZLE_YYABORT;
            (YYSession->lex->current_select->with_wild)++;
          }
        ;

select_item:
          remember_name table_wild remember_end
          {
            if (YYSession->add_item_to_list($2))
              DRIZZLE_YYABORT;
          }
        | remember_name expr remember_end select_alias
          {
            assert($1 < $3);

            if (YYSession->add_item_to_list($2))
              DRIZZLE_YYABORT;

            if ($4.str)
            {
              $2->is_autogenerated_name= false;
              $2->set_name($4.str, $4.length, system_charset_info);
            }
            else if (!$2->name)
            {
              $2->set_name($1, (uint) ($3 - $1), YYSession->charset());
            }
          }
        ;

remember_name:
          {
            Lex_input_stream *lip= YYSession->m_lip;
            $$= (char*) lip->get_cpp_tok_start();
          }
        ;

remember_end:
          {
            Lex_input_stream *lip= YYSession->m_lip;
            $$= (char*) lip->get_cpp_tok_end();
          }
        ;

select_alias:
          /* empty */ { $$=null_lex_str;}
        | AS ident { $$=$2; }
        | AS TEXT_STRING_sys { $$=$2; }
        | ident { $$=$1; }
        | TEXT_STRING_sys { $$=$1; }
        ;

optional_braces:
          /* empty */ {}
        | '(' ')' {}
        ;

/* all possible expressions */
expr:
          expr or expr %prec OR_SYM
          {
            /*
              Design notes:
              Do not use a manually maintained stack like session->lex->xxx_list,
              but use the internal bison stack ($$, $1 and $3) instead.
              Using the bison stack is:
              - more robust to changes in the grammar,
              - guaranteed to be in sync with the parser state,
              - better for performances (no memory allocation).
            */
            Item_cond_or *item1;
            Item_cond_or *item3;
            if (is_cond_or($1))
            {
              item1= (Item_cond_or*) $1;
              if (is_cond_or($3))
              {
                item3= (Item_cond_or*) $3;
                /*
                  (X1 OR X2) OR (Y1 OR Y2) ==> OR (X1, X2, Y1, Y2)
                */
                item3->add_at_head(item1->argument_list());
                $$ = $3;
              }
              else
              {
                /*
                  (X1 OR X2) OR Y ==> OR (X1, X2, Y)
                */
                item1->add($3);
                $$ = $1;
              }
            }
            else if (is_cond_or($3))
            {
              item3= (Item_cond_or*) $3;
              /*
                X OR (Y1 OR Y2) ==> OR (X, Y1, Y2)
              */
              item3->add_at_head($1);
              $$ = $3;
            }
            else
            {
              /* X OR Y */
              $$ = new (YYSession->mem_root) Item_cond_or($1, $3);
            }
          }
        | expr XOR expr %prec XOR
          {
            /* XOR is a proprietary extension */
            $$ = new (YYSession->mem_root) Item_cond_xor($1, $3);
          }
        | expr and expr %prec AND_SYM
          {
            /* See comments in rule expr: expr or expr */
            Item_cond_and *item1;
            Item_cond_and *item3;
            if (is_cond_and($1))
            {
              item1= (Item_cond_and*) $1;
              if (is_cond_and($3))
              {
                item3= (Item_cond_and*) $3;
                /*
                  (X1 AND X2) AND (Y1 AND Y2) ==> AND (X1, X2, Y1, Y2)
                */
                item3->add_at_head(item1->argument_list());
                $$ = $3;
              }
              else
              {
                /*
                  (X1 AND X2) AND Y ==> AND (X1, X2, Y)
                */
                item1->add($3);
                $$ = $1;
              }
            }
            else if (is_cond_and($3))
            {
              item3= (Item_cond_and*) $3;
              /*
                X AND (Y1 AND Y2) ==> AND (X, Y1, Y2)
              */
              item3->add_at_head($1);
              $$ = $3;
            }
            else
            {
              /* X AND Y */
              $$ = new (YYSession->mem_root) Item_cond_and($1, $3);
            }
          }
        | NOT_SYM expr %prec NOT_SYM
          { $$= negate_expression(YYSession, $2); }
        | bool_pri IS TRUE_SYM %prec IS
          { $$= new (YYSession->mem_root) Item_func_istrue($1); }
        | bool_pri IS not TRUE_SYM %prec IS
          { $$= new (YYSession->mem_root) Item_func_isnottrue($1); }
        | bool_pri IS FALSE_SYM %prec IS
          { $$= new (YYSession->mem_root) Item_func_isfalse($1); }
        | bool_pri IS not FALSE_SYM %prec IS
          { $$= new (YYSession->mem_root) Item_func_isnotfalse($1); }
        | bool_pri IS UNKNOWN_SYM %prec IS
          { $$= new Item_func_isnull($1); }
        | bool_pri IS not UNKNOWN_SYM %prec IS
          { $$= new Item_func_isnotnull($1); }
        | bool_pri
        ;

bool_pri:
          bool_pri IS NULL_SYM %prec IS
          { $$= new Item_func_isnull($1); }
        | bool_pri IS not NULL_SYM %prec IS
          { $$= new Item_func_isnotnull($1); }
        | bool_pri EQUAL_SYM predicate %prec EQUAL_SYM
          { $$= new Item_func_equal($1,$3); }
        | bool_pri comp_op predicate %prec EQ
          { $$= (*$2)(0)->create($1,$3); }
        | bool_pri comp_op all_or_any '(' subselect ')' %prec EQ
          { $$= all_any_subquery_creator($1, $2, $3, $5); }
        | predicate
        ;

predicate:
          bit_expr IN_SYM '(' subselect ')'
          {
            $$= new (YYSession->mem_root) Item_in_subselect($1, $4);
          }
        | bit_expr not IN_SYM '(' subselect ')'
          {
            Item *item= new (YYSession->mem_root) Item_in_subselect($1, $5);
            $$= negate_expression(YYSession, item);
          }
        | bit_expr IN_SYM '(' expr ')'
          {
            $$= handle_sql2003_note184_exception(YYSession, $1, true, $4);
          }
        | bit_expr IN_SYM '(' expr ',' expr_list ')'
          {
            $6->push_front($4);
            $6->push_front($1);
            $$= new (YYSession->mem_root) Item_func_in(*$6);
          }
        | bit_expr not IN_SYM '(' expr ')'
          {
            $$= handle_sql2003_note184_exception(YYSession, $1, false, $5);
          }
        | bit_expr not IN_SYM '(' expr ',' expr_list ')'
          {
            $7->push_front($5);
            $7->push_front($1);
            Item_func_in *item = new (YYSession->mem_root) Item_func_in(*$7);
            item->negate();
            $$= item;
          }
        | bit_expr BETWEEN_SYM bit_expr AND_SYM predicate
          {
            $$= new Item_func_between($1,$3,$5);
          }
        | bit_expr not BETWEEN_SYM bit_expr AND_SYM predicate
          {
            Item_func_between *item= new Item_func_between($1,$4,$6);
            item->negate();
            $$= item;
          }
        | bit_expr LIKE simple_expr opt_escape
          { 
            $$= new Item_func_like($1,$3,$4,Lex->escape_used);
          }
        | bit_expr not LIKE simple_expr opt_escape
          { 
            $$= new Item_func_not(new Item_func_like($1,$4,$5, Lex->escape_used));
          }
        | bit_expr REGEXP_SYM bit_expr
          { 
            List<Item> *args= new (YYSession->mem_root) List<Item>;
            args->push_back($1);
            args->push_back($3);
            if (! ($$= reserved_keyword_function(YYSession, "regex", args)))
            {
              DRIZZLE_YYABORT;
            }
          }
        | bit_expr not REGEXP_SYM bit_expr
          { 
            List<Item> *args= new (YYSession->mem_root) List<Item>;
            args->push_back($1);
            args->push_back($4);
            args->push_back(new (YYSession->mem_root) Item_int(1));
            if (! ($$= reserved_keyword_function(YYSession, "regex", args)))
            {
              DRIZZLE_YYABORT;
            }
          }
        | bit_expr
        ;

bit_expr:
          bit_expr '+' bit_expr %prec '+'
          { $$= new Item_func_plus($1,$3); }
        | bit_expr '-' bit_expr %prec '-'
          { $$= new Item_func_minus($1,$3); }
        | bit_expr '+' INTERVAL_SYM expr interval %prec '+'
          { $$= new Item_date_add_interval($1,$4,$5,0); }
        | bit_expr '-' INTERVAL_SYM expr interval %prec '-'
          { $$= new Item_date_add_interval($1,$4,$5,1); }
        | bit_expr '*' bit_expr %prec '*'
          { $$= new Item_func_mul($1,$3); }
        | bit_expr '/' bit_expr %prec '/'
          { $$= new Item_func_div(YYSession,$1,$3); }
        | bit_expr '%' bit_expr %prec '%'
          { $$= new Item_func_mod($1,$3); }
        | bit_expr DIV_SYM bit_expr %prec DIV_SYM
          { $$= new Item_func_int_div($1,$3); }
        | bit_expr MOD_SYM bit_expr %prec MOD_SYM
          { $$= new Item_func_mod($1,$3); }
        | simple_expr
        ;

or:
          OR_SYM
       ;

and:
          AND_SYM
       ;

not:
          NOT_SYM
        ;

comp_op:
          EQ     { $$ = &comp_eq_creator; }
        | GE     { $$ = &comp_ge_creator; }
        | GT_SYM { $$ = &comp_gt_creator; }
        | LE     { $$ = &comp_le_creator; }
        | LT     { $$ = &comp_lt_creator; }
        | NE     { $$ = &comp_ne_creator; }
        ;

all_or_any:
          ALL     { $$ = 1; }
        | ANY_SYM { $$ = 0; }
        ;

simple_expr:
          simple_ident
        | function_call_keyword
        | function_call_nonkeyword
        | function_call_generic
        | function_call_conflict
        | simple_expr COLLATE_SYM ident_or_text %prec NEG
          {
            Item *i1= new (YYSession->mem_root) Item_string($3.str,
                                                      $3.length,
                                                      YYSession->charset());
            $$= new (YYSession->mem_root) Item_func_set_collation($1, i1);
          }
        | literal
        | variable
        | sum_expr
          {
            Lex->setSumExprUsed();
          }
        | '+' simple_expr %prec NEG { $$= $2; }
        | '-' simple_expr %prec NEG
          { $$= new (YYSession->mem_root) Item_func_neg($2); }
        | '(' subselect ')'
          {
            $$= new (YYSession->mem_root) Item_singlerow_subselect($2);
          }
        | '(' expr ')' { $$= $2; }
        | '(' expr ',' expr_list ')'
          {
            $4->push_front($2);
            $$= new (YYSession->mem_root) Item_row(*$4);
          }
        | ROW_SYM '(' expr ',' expr_list ')'
          {
            $5->push_front($3);
            $$= new (YYSession->mem_root) Item_row(*$5);
          }
        | EXISTS '(' subselect ')'
          {
            $$= new (YYSession->mem_root) Item_exists_subselect($3);
          }
        | '{' ident expr '}' { $$= $3; }
        | BINARY simple_expr %prec NEG
          {
            $$= create_func_cast(YYSession, $2, ITEM_CAST_CHAR, NULL, NULL,
                                 &my_charset_bin);
          }
        | CAST_SYM '(' expr AS cast_type ')'
          {
            $$= create_func_cast(YYSession, $3, $5, Lex->length, Lex->dec,
                                 Lex->charset);
            if (!$$)
              DRIZZLE_YYABORT;
          }
        | CASE_SYM opt_expr when_list opt_else END
          { $$= new (YYSession->mem_root) Item_func_case(* $3, $2, $4 ); }
        | CONVERT_SYM '(' expr ',' cast_type ')'
          {
            $$= create_func_cast(YYSession, $3, $5, Lex->length, Lex->dec,
                                 Lex->charset);
            if (!$$)
              DRIZZLE_YYABORT;
          }
        | DEFAULT '(' simple_ident ')'
          {
            $$= new (YYSession->mem_root) Item_default_value(Lex->current_context(),
                                                         $3);
          }
        | VALUES '(' simple_ident_nospvar ')'
          {
            $$= new (YYSession->mem_root) Item_insert_value(Lex->current_context(),
                                                        $3);
          }
        | INTERVAL_SYM expr interval '+' expr %prec INTERVAL_SYM
          /* we cannot put interval before - */
          { $$= new (YYSession->mem_root) Item_date_add_interval($5,$2,$3,0); }
        ;

/*
  Function call syntax using official SQL 2003 keywords.
  Because the function name is an official token,
  a dedicated grammar rule is needed in the parser.
  There is no potential for conflicts
*/
function_call_keyword:
          CHAR_SYM '(' expr_list ')'
          { $$= new (YYSession->mem_root) Item_func_char(*$3); }
        | CURRENT_USER optional_braces
          {
            std::string user_str("user");
            if (! ($$= reserved_keyword_function(YYSession, user_str, NULL)))
            {
              DRIZZLE_YYABORT;
            }
            Lex->setCacheable(false);
          }
        | DATE_SYM '(' expr ')'
          { $$= new (YYSession->mem_root) Item_date_typecast($3); }
        | DAY_SYM '(' expr ')'
          { $$= new (YYSession->mem_root) Item_func_dayofmonth($3); }
        | HOUR_SYM '(' expr ')'
          { $$= new (YYSession->mem_root) Item_func_hour($3); }
        | INSERT '(' expr ',' expr ',' expr ',' expr ')'
          { $$= new (YYSession->mem_root) Item_func_insert(*YYSession, $3, $5, $7, $9); }
        | INTERVAL_SYM '(' expr ',' expr ')' %prec INTERVAL_SYM
          {
            List<Item> *list= new (YYSession->mem_root) List<Item>;
            list->push_front($5);
            list->push_front($3);
            Item_row *item= new (YYSession->mem_root) Item_row(*list);
            $$= new (YYSession->mem_root) Item_func_interval(item);
          }
        | INTERVAL_SYM '(' expr ',' expr ',' expr_list ')' %prec INTERVAL_SYM
          {
            $7->push_front($5);
            $7->push_front($3);
            Item_row *item= new (YYSession->mem_root) Item_row(*$7);
            $$= new (YYSession->mem_root) Item_func_interval(item);
          }
        | LEFT '(' expr ',' expr ')'
          { $$= new (YYSession->mem_root) Item_func_left($3,$5); }
        | MINUTE_SYM '(' expr ')'
          { $$= new (YYSession->mem_root) Item_func_minute($3); }
        | MONTH_SYM '(' expr ')'
          { $$= new (YYSession->mem_root) Item_func_month($3); }
        | RIGHT '(' expr ',' expr ')'
          { $$= new (YYSession->mem_root) Item_func_right($3,$5); }
        | SECOND_SYM '(' expr ')'
          { $$= new (YYSession->mem_root) Item_func_second($3); }
        | TIMESTAMP_SYM '(' expr ')'
          { $$= new (YYSession->mem_root) Item_datetime_typecast($3); }
        | TRIM '(' expr ')'
          { $$= new (YYSession->mem_root) Item_func_trim($3); }
        | TRIM '(' LEADING expr FROM expr ')'
          { $$= new (YYSession->mem_root) Item_func_ltrim($6,$4); }
        | TRIM '(' TRAILING expr FROM expr ')'
          { $$= new (YYSession->mem_root) Item_func_rtrim($6,$4); }
        | TRIM '(' BOTH expr FROM expr ')'
          { $$= new (YYSession->mem_root) Item_func_trim($6,$4); }
        | TRIM '(' LEADING FROM expr ')'
          { $$= new (YYSession->mem_root) Item_func_ltrim($5); }
        | TRIM '(' TRAILING FROM expr ')'
          { $$= new (YYSession->mem_root) Item_func_rtrim($5); }
        | TRIM '(' BOTH FROM expr ')'
          { $$= new (YYSession->mem_root) Item_func_trim($5); }
        | TRIM '(' expr FROM expr ')'
          { $$= new (YYSession->mem_root) Item_func_trim($5,$3); }
        | USER '(' ')'
          {
            if (! ($$= reserved_keyword_function(YYSession, "user", NULL)))
            {
              DRIZZLE_YYABORT;
            }
            Lex->setCacheable(false);
          }
        | YEAR_SYM '(' expr ')'
          { $$= new (YYSession->mem_root) Item_func_year($3); }
        ;

/*
  Function calls using non reserved keywords, with special syntaxic forms.
  Dedicated grammar rules are needed because of the syntax,
  but also have the potential to cause incompatibilities with other
  parts of the language.
  MAINTAINER:
  The only reasons a function should be added here are:
  - for compatibility reasons with another SQL syntax (CURDATE),
  Any other 'Syntaxic sugar' enhancements should be *STRONGLY*
  discouraged.
*/
function_call_nonkeyword:
          ADDDATE_SYM '(' expr ',' expr ')'
          {
            $$= new (YYSession->mem_root) Item_date_add_interval($3, $5,
                                                             INTERVAL_DAY, 0);
          }
        | ADDDATE_SYM '(' expr ',' INTERVAL_SYM expr interval ')'
          { $$= new (YYSession->mem_root) Item_date_add_interval($3, $6, $7, 0); }
        | CURDATE optional_braces
          {
            $$= new (YYSession->mem_root) Item_func_curdate_local();
            Lex->setCacheable(false);
          }
        | DATE_ADD_INTERVAL '(' expr ',' INTERVAL_SYM expr interval ')' %prec INTERVAL_SYM
          { $$= new (YYSession->mem_root) Item_date_add_interval($3,$6,$7,0); }
        | DATE_SUB_INTERVAL '(' expr ',' INTERVAL_SYM expr interval ')' %prec INTERVAL_SYM
          { $$= new (YYSession->mem_root) Item_date_add_interval($3,$6,$7,1); }
        | EXTRACT_SYM '(' interval FROM expr ')'
          { $$=new (YYSession->mem_root) Item_extract( $3, $5); }
        | NOW_SYM optional_braces
          {
            $$= new (YYSession->mem_root) Item_func_now_local();
            Lex->setCacheable(false);
          }
        | NOW_SYM '(' expr ')'
          {
            $$= new (YYSession->mem_root) Item_func_now_local($3);
            Lex->setCacheable(false);
          }
        | POSITION_SYM '(' bit_expr IN_SYM expr ')'
          { $$ = new (YYSession->mem_root) Item_func_locate($5,$3); }
        | SUBDATE_SYM '(' expr ',' expr ')'
          {
            $$= new (YYSession->mem_root) Item_date_add_interval($3, $5,
                                                             INTERVAL_DAY, 1);
          }
        | SUBDATE_SYM '(' expr ',' INTERVAL_SYM expr interval ')'
          { $$= new (YYSession->mem_root) Item_date_add_interval($3, $6, $7, 1); }
        | SUBSTRING '(' expr ',' expr ',' expr ')'
          {
            std::string reverse_str("substr");
            List<Item> *args= new (YYSession->mem_root) List<Item>;
            args->push_back($3);
            args->push_back($5);
            args->push_back($7);
            if (! ($$= reserved_keyword_function(YYSession, reverse_str, args)))
            {
              DRIZZLE_YYABORT;
            }
          }
        | SUBSTRING '(' expr ',' expr ')'
          {
            std::string reverse_str("substr");
            List<Item> *args= new (YYSession->mem_root) List<Item>;
            args->push_back($3);
            args->push_back($5);
            if (! ($$= reserved_keyword_function(YYSession, reverse_str, args)))
            {
              DRIZZLE_YYABORT;
            }
          }
        | SUBSTRING '(' expr FROM expr FOR_SYM expr ')'
          {
            std::string reverse_str("substr");
            List<Item> *args= new (YYSession->mem_root) List<Item>;
            args->push_back($3);
            args->push_back($5);
            args->push_back($7);
            if (! ($$= reserved_keyword_function(YYSession, reverse_str, args)))
            {
              DRIZZLE_YYABORT;
            }
          }
        | SUBSTRING '(' expr FROM expr ')'
          {
            std::string reverse_str("substr");
            List<Item> *args= new (YYSession->mem_root) List<Item>;
            args->push_back($3);
            args->push_back($5);
            if (! ($$= reserved_keyword_function(YYSession, reverse_str, args)))
            {
              DRIZZLE_YYABORT;
            }
          }
        | SYSDATE optional_braces
          { 
            $$= new (YYSession->mem_root) Item_func_sysdate_local(); 
            Lex->setCacheable(false);
          }
        | SYSDATE '(' expr ')'
          { 
            $$= new (YYSession->mem_root) Item_func_sysdate_local($3); 
            Lex->setCacheable(false);
          }
        | TIMESTAMP_ADD '(' interval_time_stamp ',' expr ',' expr ')'
          { $$= new (YYSession->mem_root) Item_date_add_interval($7,$5,$3,0); }
        | TIMESTAMP_DIFF '(' interval_time_stamp ',' expr ',' expr ')'
          { $$= new (YYSession->mem_root) Item_func_timestamp_diff($5,$7,$3); }
        | UTC_DATE_SYM optional_braces
          {
            $$= new (YYSession->mem_root) Item_func_curdate_utc();
            Lex->setCacheable(false);
          }
        | UTC_TIMESTAMP_SYM optional_braces
          {
            $$= new (YYSession->mem_root) Item_func_now_utc();
            Lex->setCacheable(false);
          }
        ;

/*
  Functions calls using a non reserved keyword, and using a regular syntax.
  Because the non reserved keyword is used in another part of the grammar,
  a dedicated rule is needed here.
*/
function_call_conflict:
        COALESCE '(' expr_list ')'
          { $$= new (YYSession->mem_root) Item_func_coalesce(* $3); }
        | COLLATION_SYM '(' expr ')'
          { $$= new (YYSession->mem_root) Item_func_collation($3); }
        | DATABASE '(' ')'
          {
            if (! ($$= reserved_keyword_function(YYSession, "database", NULL)))
            {
              DRIZZLE_YYABORT;
            }
            Lex->setCacheable(false);
	  }
        | CATALOG_SYM '(' ')'
          {
            if (! ($$= reserved_keyword_function(YYSession, "catalog", NULL)))
            {
              DRIZZLE_YYABORT;
            }
            Lex->setCacheable(false);
	  }
        | EXECUTE_SYM '(' expr ')' opt_wait
          {
            List<Item> *args= new (YYSession->mem_root) List<Item>;
            args->push_back($3);

            if ($5)
            {
              args->push_back(new (YYSession->mem_root) Item_int(1));
            }

            if (! ($$= reserved_keyword_function(YYSession, "execute", args)))
            {
              DRIZZLE_YYABORT;
            }
          }
        | IF '(' expr ',' expr ',' expr ')'
          { $$= new (YYSession->mem_root) Item_func_if($3,$5,$7); }
        | KILL_SYM kill_option '(' expr ')'
          {
            std::string kill_str("kill");
            List<Item> *args= new (YYSession->mem_root) List<Item>;
            args->push_back($4);

            if ($2)
            {
              args->push_back(new (YYSession->mem_root) Item_uint(1));
            }

            if (! ($$= reserved_keyword_function(YYSession, kill_str, args)))
            {
              DRIZZLE_YYABORT;
            }
          }
        | MICROSECOND_SYM '(' expr ')'
          { $$= new (YYSession->mem_root) Item_func_microsecond($3); }
        | MOD_SYM '(' expr ',' expr ')'
          { $$ = new (YYSession->mem_root) Item_func_mod( $3, $5); }
        | QUARTER_SYM '(' expr ')'
          { $$ = new (YYSession->mem_root) Item_func_quarter($3); }
        | REPEAT_SYM '(' expr ',' expr ')'
          { $$= new (YYSession->mem_root) Item_func_repeat(*YYSession, $3, $5); }
        | REPLACE '(' expr ',' expr ',' expr ')'
          { $$= new (YYSession->mem_root) Item_func_replace(*YYSession, $3, $5, $7); }
        | TRUNCATE_SYM '(' expr ',' expr ')'
          { $$= new (YYSession->mem_root) Item_func_round($3,$5,1); }
        | WAIT_SYM '(' expr ')'
          {
            std::string wait_str("wait");
            List<Item> *args= new (YYSession->mem_root) List<Item>;
            args->push_back($3);
            if (! ($$= reserved_keyword_function(YYSession, wait_str, args)))
            {
              DRIZZLE_YYABORT;
            }
          }
        | UUID_SYM '(' ')'
          {
            if (! ($$= reserved_keyword_function(YYSession, "uuid", NULL)))
            {
              DRIZZLE_YYABORT;
            }
            Lex->setCacheable(false);
	  }
        | WAIT_SYM '(' expr ',' expr ')'
          {
            std::string wait_str("wait");
            List<Item> *args= new (YYSession->mem_root) List<Item>;
            args->push_back($3);
            args->push_back($5);
            if (! ($$= reserved_keyword_function(YYSession, wait_str, args)))
            {
              DRIZZLE_YYABORT;
            }
          }
        ;

/*
  Regular function calls.
  The function name is *not* a token, and therefore is guaranteed to not
  introduce side effects to the language in general.
  MAINTAINER:
  All the new functions implemented for new features should fit into
  this category.
*/
function_call_generic:
          IDENT_sys '('
          {
            const plugin::Function *udf= plugin::Function::get($1.str, $1.length);

            /* Temporary placing the result of getFunction in $3 */
            $<udf>$= udf;
          }
          opt_udf_expr_list ')'
          {
            Create_func *builder;
            Item *item= NULL;

            /*
              Implementation note:
              names are resolved with the following order:
              - MySQL native functions,
              - User Defined Functions,
              - Stored Functions (assuming the current <use> database)

              This will be revised with WL#2128 (SQL PATH)
            */
            builder= find_native_function_builder($1);
            if (builder)
            {
              item= builder->create(YYSession, $1, $4);
            }
            else
            {
              /* Retrieving the result of service::Function::get */
              const plugin::Function *udf= $<udf>3;
              if (udf)
              {
                item= Create_udf_func::s_singleton.create(YYSession, udf, $4);
              } else {
                /* fix for bug 250065, from Andrew Garner <muzazzi@gmail.com> */
                my_error(ER_SP_DOES_NOT_EXIST, MYF(0), "FUNCTION", $1.str);
              }
            }

            if (! ($$= item))
            {
              DRIZZLE_YYABORT;
            }
            Lex->setCacheable(false);
          }
        ;

opt_udf_expr_list:
        /* empty */     { $$= NULL; }
        | udf_expr_list { $$= $1; }
        ;

udf_expr_list:
          udf_expr
          {
            $$= new (YYSession->mem_root) List<Item>;
            $$->push_back($1);
          }
        | udf_expr_list ',' udf_expr
          {
            $1->push_back($3);
            $$= $1;
          }
        ;

udf_expr:
          remember_name expr remember_end select_alias
          {
            /*
             Use Item::name as a storage for the attribute value of user
             defined function argument. It is safe to use Item::name
             because the syntax will not allow having an explicit name here.
             See WL#1017 re. udf attributes.
            */
            if ($4.str)
            {
              $2->is_autogenerated_name= false;
              $2->set_name($4.str, $4.length, system_charset_info);
            }
            else
              $2->set_name($1, (uint) ($3 - $1), YYSession->charset());
            $$= $2;
          }
        ;

sum_expr:
          AVG_SYM '(' in_sum_expr ')'
          { $$=new Item_sum_avg($3); }
        | AVG_SYM '(' DISTINCT in_sum_expr ')'
          { $$=new Item_sum_avg_distinct($4); }
        | COUNT_SYM '(' opt_all '*' ')'
          { $$=new Item_sum_count(new Item_int((int32_t) 0L,1)); }
        | COUNT_SYM '(' in_sum_expr ')'
          { $$=new Item_sum_count($3); }
        | COUNT_SYM '(' DISTINCT
          { Lex->current_select->in_sum_expr++; }
          expr_list
          { Lex->current_select->in_sum_expr--; }
          ')'
          { $$=new Item_sum_count_distinct(* $5); }
        | MIN_SYM '(' in_sum_expr ')'
          { $$=new Item_sum_min($3); }
        /*
          According to ANSI SQL, DISTINCT is allowed and has
          no sense inside MIN and MAX grouping functions; so MIN|MAX(DISTINCT ...)
          is processed like an ordinary MIN | MAX()
        */
        | MIN_SYM '(' DISTINCT in_sum_expr ')'
          { $$=new Item_sum_min($4); }
        | MAX_SYM '(' in_sum_expr ')'
          { $$=new Item_sum_max($3); }
        | MAX_SYM '(' DISTINCT in_sum_expr ')'
          { $$=new Item_sum_max($4); }
        | STD_SYM '(' in_sum_expr ')'
          { $$=new Item_sum_std($3, 0); }
        | VARIANCE_SYM '(' in_sum_expr ')'
          { $$=new Item_sum_variance($3, 0); }
        | STDDEV_SAMP_SYM '(' in_sum_expr ')'
          { $$=new Item_sum_std($3, 1); }
        | VAR_SAMP_SYM '(' in_sum_expr ')'
          { $$=new Item_sum_variance($3, 1); }
        | SUM_SYM '(' in_sum_expr ')'
          { $$=new Item_sum_sum($3); }
        | SUM_SYM '(' DISTINCT in_sum_expr ')'
          { $$=new Item_sum_sum_distinct($4); }
        | GROUP_CONCAT_SYM '(' opt_distinct
          { Lex->current_select->in_sum_expr++; }
          expr_list opt_gorder_clause
          opt_gconcat_separator
          ')'
          {
            Select_Lex *sel= Lex->current_select;
            sel->in_sum_expr--;
            $$=new Item_func_group_concat(Lex->current_context(), $3, $5,
                                          sel->gorder_list, $7);
            $5->empty();
          }
        ;

variable:
          '@'
          { }
          variable_aux
          {
            $$= $3;
          }
        ;

variable_aux:
          user_variable_ident SET_VAR expr
          {
            $$= new Item_func_set_user_var($1, $3);
            Lex->setCacheable(false);
          }
        | user_variable_ident
          {
            $$= new Item_func_get_user_var(*YYSession, $1);
            Lex->setCacheable(false);
          }
        | '@' opt_var_ident_type user_variable_ident opt_component
          {
            /* disallow "SELECT @@global.global.variable" */
            if ($3.str && $4.str && check_reserved_words(&$3))
            {
              struct my_parse_error_st pass= { ER(ER_SYNTAX_ERROR), YYSession };
              my_parse_error(&pass);
              DRIZZLE_YYABORT;
            }
            if (!($$= get_system_var(YYSession, $2, $3, $4)))
              DRIZZLE_YYABORT;
          }
        ;

opt_distinct:
          /* empty */ { $$ = false; }
        | DISTINCT    { $$ = true; }
        ;

opt_gconcat_separator:
          /* empty */
            {
              $$= new (YYSession->mem_root) String(",", 1, &my_charset_utf8_general_ci);
            }
        | SEPARATOR_SYM text_string { $$ = $2; }
        ;

opt_gorder_clause:
          /* empty */
          {
            Lex->current_select->gorder_list = NULL;
          }
        | order_clause
          {
            Select_Lex *select= Lex->current_select;
            select->gorder_list=
              (SQL_LIST*) memory::sql_memdup((char*) &select->order_list,
                                     sizeof(st_sql_list));
            select->order_list.empty();
          }
        ;

in_sum_expr:
          opt_all
          {
            if (Lex->current_select->inc_in_sum_expr())
            {
              struct my_parse_error_st pass= { ER(ER_SYNTAX_ERROR), YYSession };
              my_parse_error(&pass);
              DRIZZLE_YYABORT;
            }
          }
          expr
          {
            Lex->current_select->in_sum_expr--;
            $$= $3;
          }
        ;

cast_type:
          BINARY opt_len
          { $$=ITEM_CAST_CHAR; Lex->charset= &my_charset_bin; Lex->dec= 0; }
        | BOOLEAN_SYM
          { $$=ITEM_CAST_BOOLEAN; Lex->charset= &my_charset_bin; Lex->dec= 0; }
        | SIGNED_SYM
          { $$=ITEM_CAST_SIGNED; Lex->charset= NULL; Lex->dec=Lex->length= (char*)0; }
        | SIGNED_SYM INT_SYM
          { $$=ITEM_CAST_SIGNED; Lex->charset= NULL; Lex->dec=Lex->length= (char*)0; }
        | INT_SYM
          { $$=ITEM_CAST_SIGNED; Lex->charset= NULL; Lex->dec=Lex->length= (char*)0; }
        | UNSIGNED_SYM
          { $$=ITEM_CAST_UNSIGNED; Lex->charset= NULL; Lex->dec=Lex->length= (char*)0; }
        | UNSIGNED_SYM INT_SYM
          { $$=ITEM_CAST_UNSIGNED; Lex->charset= NULL; Lex->dec=Lex->length= (char*)0; }
        | CHAR_SYM opt_len
          { $$=ITEM_CAST_CHAR; Lex->dec= 0; }
        | DATE_SYM
          { $$=ITEM_CAST_DATE; Lex->charset= NULL; Lex->dec=Lex->length= (char*)0; }
        | TIME_SYM
          { $$=ITEM_CAST_TIME; Lex->charset= NULL; Lex->dec=Lex->length= (char*)0; }
        | DATETIME_SYM
          { $$=ITEM_CAST_DATETIME; Lex->charset= NULL; Lex->dec=Lex->length= (char*)0; }
        | DECIMAL_SYM float_options
          { $$=ITEM_CAST_DECIMAL; Lex->charset= NULL; }
        ;

expr_list:
          expr
          {
            $$= new (YYSession->mem_root) List<Item>;
            $$->push_back($1);
          }
        | expr_list ',' expr
          {
            $1->push_back($3);
            $$= $1;
          }
        ;

opt_expr:
          /* empty */    { $$= NULL; }
        | expr           { $$= $1; }
        ;

opt_else:
          /* empty */  { $$= NULL; }
        | ELSE expr    { $$= $2; }
        ;

when_list:
          WHEN_SYM expr THEN_SYM expr
          {
            $$= new List<Item>;
            $$->push_back($2);
            $$->push_back($4);
          }
        | when_list WHEN_SYM expr THEN_SYM expr
          {
            $1->push_back($3);
            $1->push_back($5);
            $$= $1;
          }
        ;

/* Equivalent to <table reference> in the SQL:2003 standard. */
/* Warning - may return NULL in case of incomplete SELECT */
table_ref:
          table_factor { $$=$1; }
        | join_table
          {
            if (!($$= Lex->current_select->nest_last_join(Lex->session)))
              DRIZZLE_YYABORT;
          }
        ;

join_table_list:
          derived_table_list { DRIZZLE_YYABORT_UNLESS($$=$1); }
        ;

/*
  The ODBC escape syntax for Outer Join is: '{' OJ join_table '}'
  The parser does not define OJ as a token, any ident is accepted
  instead in $2 (ident). Also, all productions from table_ref can
  be escaped, not only join_table. Both syntax extensions are safe
  and are ignored.
*/
esc_table_ref:
        table_ref { $$=$1; }
      | '{' ident table_ref '}' { $$=$3; }
      ;

/* Equivalent to <table reference list> in the SQL:2003 standard. */
/* Warning - may return NULL in case of incomplete SELECT */
derived_table_list:
          esc_table_ref { $$=$1; }
        | derived_table_list ',' esc_table_ref
          {
            DRIZZLE_YYABORT_UNLESS($1 && ($$=$3));
          }
        ;

/*
  Notice that JOIN is a left-associative operation, and it must be parsed
  as such, that is, the parser must process first the left join operand
  then the right one. Such order of processing ensures that the parser
  produces correct join trees which is essential for semantic analysis
  and subsequent optimization phases.
*/
join_table:
          /* INNER JOIN variants */
          /*
            Use %prec to evaluate production 'table_ref' before 'normal_join'
            so that [INNER | CROSS] JOIN is properly nested as other
            left-associative joins.
          */
          table_ref normal_join table_ref %prec TABLE_REF_PRIORITY
          { 
            DRIZZLE_YYABORT_UNLESS($1 && ($$=$3));
            Lex->is_cross= false;
          }
        | table_ref STRAIGHT_JOIN table_factor
          { 
            DRIZZLE_YYABORT_UNLESS($1 && ($$=$3)); $3->straight=1; 
          }
        | table_ref normal_join table_ref
          ON
          {
            DRIZZLE_YYABORT_UNLESS($1 && $3);
            DRIZZLE_YYABORT_UNLESS( not Lex->is_cross );
            /* Change the current name resolution context to a local context. */
            if (push_new_name_resolution_context(YYSession, $1, $3))
              DRIZZLE_YYABORT;
            Lex->current_select->parsing_place= IN_ON;
          }
          expr
          {
            add_join_on($3,$6);
            Lex->pop_context();
            Lex->current_select->parsing_place= NO_MATTER;
          }
        | table_ref STRAIGHT_JOIN table_factor
          ON
          {
            DRIZZLE_YYABORT_UNLESS($1 && $3);
            /* Change the current name resolution context to a local context. */
            if (push_new_name_resolution_context(YYSession, $1, $3))
              DRIZZLE_YYABORT;
            Lex->current_select->parsing_place= IN_ON;
          }
          expr
          {
            $3->straight=1;
            add_join_on($3,$6);
            Lex->pop_context();
            Lex->current_select->parsing_place= NO_MATTER;
          }
        | table_ref normal_join table_ref
          USING
          {
            DRIZZLE_YYABORT_UNLESS($1 && $3);
          }
          '(' using_list ')'
          { add_join_natural($1,$3,$7,Lex->current_select); $$=$3; }
        | table_ref NATURAL JOIN_SYM table_factor
          {
            DRIZZLE_YYABORT_UNLESS($1 && ($$=$4));
            add_join_natural($1,$4,NULL,Lex->current_select);
          }

          /* LEFT JOIN variants */
        | table_ref LEFT opt_outer JOIN_SYM table_ref
          ON
          {
            DRIZZLE_YYABORT_UNLESS($1 && $5);
            /* Change the current name resolution context to a local context. */
            if (push_new_name_resolution_context(YYSession, $1, $5))
              DRIZZLE_YYABORT;
            Lex->current_select->parsing_place= IN_ON;
          }
          expr
          {
            add_join_on($5,$8);
            Lex->pop_context();
            $5->outer_join|=JOIN_TYPE_LEFT;
            $$=$5;
            Lex->current_select->parsing_place= NO_MATTER;
          }
        | table_ref LEFT opt_outer JOIN_SYM table_factor
          {
            DRIZZLE_YYABORT_UNLESS($1 && $5);
          }
          USING '(' using_list ')'
          {
            add_join_natural($1,$5,$9,Lex->current_select);
            $5->outer_join|=JOIN_TYPE_LEFT;
            $$=$5;
          }
        | table_ref NATURAL LEFT opt_outer JOIN_SYM table_factor
          {
            DRIZZLE_YYABORT_UNLESS($1 && $6);
            add_join_natural($1,$6,NULL,Lex->current_select);
            $6->outer_join|=JOIN_TYPE_LEFT;
            $$=$6;
          }

          /* RIGHT JOIN variants */
        | table_ref RIGHT opt_outer JOIN_SYM table_ref
          ON
          {
            DRIZZLE_YYABORT_UNLESS($1 && $5);
            /* Change the current name resolution context to a local context. */
            if (push_new_name_resolution_context(YYSession, $1, $5))
              DRIZZLE_YYABORT;
            Lex->current_select->parsing_place= IN_ON;
          }
          expr
          {
            if (!($$= Lex->current_select->convert_right_join()))
              DRIZZLE_YYABORT;
            add_join_on($$, $8);
            Lex->pop_context();
            Lex->current_select->parsing_place= NO_MATTER;
          }
        | table_ref RIGHT opt_outer JOIN_SYM table_factor
          {
            DRIZZLE_YYABORT_UNLESS($1 && $5);
          }
          USING '(' using_list ')'
          {
            if (!($$= Lex->current_select->convert_right_join()))
              DRIZZLE_YYABORT;
            add_join_natural($$,$5,$9,Lex->current_select);
          }
        | table_ref NATURAL RIGHT opt_outer JOIN_SYM table_factor
          {
            DRIZZLE_YYABORT_UNLESS($1 && $6);
            add_join_natural($6,$1,NULL,Lex->current_select);
            if (!($$= Lex->current_select->convert_right_join()))
              DRIZZLE_YYABORT;
          }
        ;

normal_join:
          JOIN_SYM {}
        | INNER_SYM JOIN_SYM {}
        | CROSS JOIN_SYM { Lex->is_cross= true; }
        ;

/*
   This is a flattening of the rules <table factor> and <table primary>
   in the SQL:2003 standard, since we don't have <sample clause>

   I.e.
   <table factor> ::= <table primary> [ <sample clause> ]
*/  
/* Warning - may return NULL in case of incomplete SELECT */
table_factor:
          {
          }
          table_ident opt_table_alias opt_key_definition
          {
            if (!($$= Lex->current_select->add_table_to_list(YYSession, $2, $3,
                             0,
                             Lex->lock_option,
                             Lex->current_select->pop_index_hints())))
              DRIZZLE_YYABORT;
            Lex->current_select->add_joined_table($$);
          }
        | select_derived_init get_select_lex select_derived2
          {
            Select_Lex *sel= Lex->current_select;
            if ($1)
            {
              if (sel->set_braces(1))
              {
                struct my_parse_error_st pass= { ER(ER_SYNTAX_ERROR), YYSession };
                my_parse_error(&pass);
                DRIZZLE_YYABORT;
              }
              /* select in braces, can't contain global parameters */
              if (sel->master_unit()->fake_select_lex)
                sel->master_unit()->global_parameters=
                   sel->master_unit()->fake_select_lex;
            }
            if ($2->init_nested_join(Lex->session))
              DRIZZLE_YYABORT;
            $$= 0;
            /* incomplete derived tables return NULL, we must be
               nested in select_derived rule to be here. */
          }
          /*
            Represents a flattening of the following rules from the SQL:2003
            standard. This sub-rule corresponds to the sub-rule
            <table primary> ::= ... | <derived table> [ AS ] <correlation name>
           
            The following rules have been flattened into query_expression_body
            (since we have no <with clause>).

            <derived table> ::= <table subquery>
            <table subquery> ::= <subquery>
            <subquery> ::= <left paren> <query expression> <right paren>
            <query expression> ::= [ <with clause> ] <query expression body>

            For the time being we use the non-standard rule
            select_derived_union which is a compromise between the standard
            and our parser. Possibly this rule could be replaced by our
            query_expression_body.
          */
        | '(' get_select_lex select_derived_union ')' opt_table_alias
          {
            /* Use $2 instead of Lex->current_select as derived table will
               alter value of Lex->current_select. */
            if (!($3 || $5) && $2->embedding &&
                !$2->embedding->getNestedJoin()->join_list.elements)
            {
              /* we have a derived table ($3 == NULL) but no alias,
                 Since we are nested in further parentheses so we
                 can pass NULL to the outer level parentheses
                 Permits parsing of "((((select ...))) as xyz)" */
              $$= 0;
            }
            else if (!$3)
            {
              /* Handle case of derived table, alias may be NULL if there
                 are no outer parentheses, add_table_to_list() will throw
                 error in this case */
              Select_Lex *sel= Lex->current_select;
              Select_Lex_Unit *unit= sel->master_unit();
              Lex->current_select= sel= unit->outer_select();
              if (!($$= sel->add_table_to_list(Lex->session,
                                               new Table_ident(unit), $5, 0,
                                               TL_READ)))

                DRIZZLE_YYABORT;
              sel->add_joined_table($$);
              Lex->pop_context();
            }
            else if (($3->select_lex && $3->select_lex->master_unit()->is_union()) || $5)
            {
              /* simple nested joins cannot have aliases or unions */
              struct my_parse_error_st pass= { ER(ER_SYNTAX_ERROR), YYSession };
              my_parse_error(&pass);
              DRIZZLE_YYABORT;
            }
            else
              $$= $3;
          }
        ;

select_derived_union:
          select_derived opt_order_clause opt_limit_clause
        | select_derived_union
          UNION_SYM
          union_option
          {
            if (add_select_to_union_list(YYSession, Lex, (bool)$3))
              DRIZZLE_YYABORT;
          }
          query_specification
          {
            /*
              Remove from the name resolution context stack the context of the
              last select in the union.
             */
            Lex->pop_context();
          }
          opt_order_clause opt_limit_clause
        ;

/* The equivalent of select_init2 for nested queries. */
select_init2_derived:
          select_part2_derived
          {
            Select_Lex * sel= Lex->current_select;
            if (Lex->current_select->set_braces(0))
            {
              struct my_parse_error_st pass= { ER(ER_SYNTAX_ERROR), YYSession };
              my_parse_error(&pass);
              DRIZZLE_YYABORT;
            }
            if (sel->linkage == UNION_TYPE &&
                sel->master_unit()->first_select()->braces)
            {
              struct my_parse_error_st pass= { ER(ER_SYNTAX_ERROR), YYSession };
              my_parse_error(&pass);
              DRIZZLE_YYABORT;
            }
          }
        ;

/* The equivalent of select_part2 for nested queries. */
select_part2_derived:
          {
            Select_Lex *sel= Lex->current_select;
            if (sel->linkage != UNION_TYPE)
              init_select(Lex);
            Lex->current_select->parsing_place= SELECT_LIST;
          }
          select_options select_item_list
          {
            Lex->current_select->parsing_place= NO_MATTER;
          }
          opt_select_from select_lock_type
        ;

/* handle contents of parentheses in join expression */
select_derived:
          get_select_lex
          {
            if ($1->init_nested_join(Lex->session))
              DRIZZLE_YYABORT;
          }
          derived_table_list
          {
            /* for normal joins, $3 != NULL and end_nested_join() != NULL,
               for derived tables, both must equal NULL */

            if (!($$= $1->end_nested_join(Lex->session)) && $3)
              DRIZZLE_YYABORT;
            if (!$3 && $$)
            {
              struct my_parse_error_st pass= { ER(ER_SYNTAX_ERROR), YYSession };
              my_parse_error(&pass);
              DRIZZLE_YYABORT;
            }
          }
        ;

select_derived2:
          {
            Lex->derived_tables|= DERIVED_SUBQUERY;
            if (not Lex->expr_allows_subselect)
            {
              struct my_parse_error_st pass= { ER(ER_SYNTAX_ERROR), YYSession };
              my_parse_error(&pass);
              DRIZZLE_YYABORT;
            }
            if (Lex->current_select->linkage == GLOBAL_OPTIONS_TYPE || new_select(Lex, 1))
              DRIZZLE_YYABORT;
            init_select(Lex);
            Lex->current_select->linkage= DERIVED_TABLE_TYPE;
            Lex->current_select->parsing_place= SELECT_LIST;
          }
          select_options select_item_list
          {
            Lex->current_select->parsing_place= NO_MATTER;
          }
          opt_select_from
        ;

get_select_lex:
          /* Empty */ { $$= Lex->current_select; }
        ;

select_derived_init:
          SELECT_SYM
          {
            Select_Lex *sel= Lex->current_select;
            TableList *embedding;
            if (!sel->embedding || sel->end_nested_join(Lex->session))
            {
              /* we are not in parentheses */
              struct my_parse_error_st pass= { ER(ER_SYNTAX_ERROR), YYSession };
              my_parse_error(&pass);
              DRIZZLE_YYABORT;
            }
            embedding= Lex->current_select->embedding;
            $$= embedding &&
                !embedding->getNestedJoin()->join_list.elements;
            /* return true if we are deeply nested */
          }
        ;

opt_outer:
          /* empty */ {}
        | OUTER {}
        ;

index_hint_clause:
          /* empty */
          {
            $$= INDEX_HINT_MASK_ALL;
          }
        | FOR_SYM JOIN_SYM      { $$= INDEX_HINT_MASK_JOIN;  }
        | FOR_SYM ORDER_SYM BY  { $$= INDEX_HINT_MASK_ORDER; }
        | FOR_SYM GROUP_SYM BY  { $$= INDEX_HINT_MASK_GROUP; }
        ;

index_hint_type:
          FORCE_SYM  { $$= INDEX_HINT_FORCE; }
        | IGNORE_SYM { $$= INDEX_HINT_IGNORE; }
        ;

index_hint_definition:
          index_hint_type key_or_index index_hint_clause
          {
            Lex->current_select->set_index_hint_type($1, $3);
          }
          '(' key_usage_list ')'
        | USE_SYM key_or_index index_hint_clause
          {
            Lex->current_select->set_index_hint_type(INDEX_HINT_USE, $3);
          }
          '(' opt_key_usage_list ')'
       ;

index_hints_list:
          index_hint_definition
        | index_hints_list index_hint_definition
        ;

opt_index_hints_list:
          /* empty */
        | { Lex->current_select->alloc_index_hints(YYSession); } index_hints_list
        ;

opt_key_definition:
          {  Lex->current_select->clear_index_hints(); }
          opt_index_hints_list
        ;

opt_key_usage_list:
          /* empty */ { Lex->current_select->add_index_hint(YYSession, NULL, 0); }
        | key_usage_list {}
        ;

key_usage_element:
          ident
          { Lex->current_select->add_index_hint(YYSession, $1.str, $1.length); }
        | PRIMARY_SYM
          { Lex->current_select->add_index_hint(YYSession, (char *)"PRIMARY", 7); }
        ;

key_usage_list:
          key_usage_element
        | key_usage_list ',' key_usage_element
        ;

using_list:
          ident
          {
            if (!($$= new List<String>))
              DRIZZLE_YYABORT;
            $$->push_back(new (YYSession->mem_root)
                              String((const char *) $1.str, $1.length,
                                      system_charset_info));
          }
        | using_list ',' ident
          {
            $1->push_back(new (YYSession->mem_root)
                              String((const char *) $3.str, $3.length,
                                      system_charset_info));
            $$= $1;
          }
        ;

interval:
          interval_time_st {}
        | DAY_HOUR_SYM           { $$=INTERVAL_DAY_HOUR; }
        | DAY_MICROSECOND_SYM    { $$=INTERVAL_DAY_MICROSECOND; }
        | DAY_MINUTE_SYM         { $$=INTERVAL_DAY_MINUTE; }
        | DAY_SECOND_SYM         { $$=INTERVAL_DAY_SECOND; }
        | HOUR_MICROSECOND_SYM   { $$=INTERVAL_HOUR_MICROSECOND; }
        | HOUR_MINUTE_SYM        { $$=INTERVAL_HOUR_MINUTE; }
        | HOUR_SECOND_SYM        { $$=INTERVAL_HOUR_SECOND; }
        | MINUTE_MICROSECOND_SYM { $$=INTERVAL_MINUTE_MICROSECOND; }
        | MINUTE_SECOND_SYM      { $$=INTERVAL_MINUTE_SECOND; }
        | SECOND_MICROSECOND_SYM { $$=INTERVAL_SECOND_MICROSECOND; }
        | YEAR_MONTH_SYM         { $$=INTERVAL_YEAR_MONTH; }
        ;

interval_time_stamp:
	interval_time_st	{}
	| FRAC_SECOND_SYM	{
                                  $$=INTERVAL_MICROSECOND;
                                  /*
                                    FRAC_SECOND was mistakenly implemented with
                                    a wrong resolution. According to the ODBC
                                    standard it should be nanoseconds, not
                                    microseconds. Changing it to nanoseconds
                                    in MySQL would mean making TIMESTAMPDIFF
                                    and TIMESTAMPADD to return DECIMAL, since
                                    the return value would be too big for BIGINT
                                    Hence we just deprecate the incorrect
                                    implementation without changing its
                                    resolution.
                                  */
                                }
	;

interval_time_st:
          DAY_SYM         { $$=INTERVAL_DAY; }
        | WEEK_SYM        { $$=INTERVAL_WEEK; }
        | HOUR_SYM        { $$=INTERVAL_HOUR; }
        | MINUTE_SYM      { $$=INTERVAL_MINUTE; }
        | MONTH_SYM       { $$=INTERVAL_MONTH; }
        | QUARTER_SYM     { $$=INTERVAL_QUARTER; }
        | SECOND_SYM      { $$=INTERVAL_SECOND; }
        | MICROSECOND_SYM { $$=INTERVAL_MICROSECOND; }
        | YEAR_SYM        { $$=INTERVAL_YEAR; }
        ;

table_alias:
          /* empty */
        | AS
        | EQ
        ;

opt_table_alias:
          /* empty */ { $$=0; }
        | table_alias ident
          { $$= (drizzled::LEX_STRING*) memory::sql_memdup(&$2,sizeof(drizzled::LEX_STRING)); }
        ;

opt_all:
          /* empty */
        | ALL
        ;

where_clause:
          /* empty */  { Lex->current_select->where= 0; }
        | WHERE
          {
            Lex->current_select->parsing_place= IN_WHERE;
          }
          expr
          {
            Select_Lex *select= Lex->current_select;
            select->where= $3;
            select->parsing_place= NO_MATTER;
            if ($3)
              $3->top_level_item();
          }
        ;

having_clause:
          /* empty */
        | HAVING
          {
            Lex->current_select->parsing_place= IN_HAVING;
          }
          expr
          {
            Select_Lex *sel= Lex->current_select;
            sel->having= $3;
            sel->parsing_place= NO_MATTER;
            if ($3)
              $3->top_level_item();
          }
        ;

opt_escape:
          ESCAPE_SYM simple_expr
          {
            Lex->escape_used= true;
            $$= $2;
          }
        | /* empty */
          {
            Lex->escape_used= false;
            $$= new Item_string("\\", 1, &my_charset_utf8_general_ci);
          }
        ;

/*
   group by statement in select
*/

group_clause:
          /* empty */
        | GROUP_SYM BY group_list olap_opt
        ;

group_list:
          group_list ',' order_ident order_dir
          { if (YYSession->add_group_to_list($3,(bool) $4)) DRIZZLE_YYABORT; }
        | order_ident order_dir
          { if (YYSession->add_group_to_list($1,(bool) $2)) DRIZZLE_YYABORT; }
        ;

olap_opt:
          /* empty */ {}
        | WITH_ROLLUP_SYM
          {
            /*
              'WITH ROLLUP' is needed for backward compatibility,
              and cause LALR(2) conflicts.
              This syntax is not standard.
              MySQL syntax: GROUP BY col1, col2, col3 WITH ROLLUP
              SQL-2003: GROUP BY ... ROLLUP(col1, col2, col3)
            */
            if (Lex->current_select->linkage == GLOBAL_OPTIONS_TYPE)
            {
              my_error(ER_WRONG_USAGE, MYF(0), "WITH ROLLUP",
                       "global union parameters");
              DRIZZLE_YYABORT;
            }
            Lex->current_select->olap= ROLLUP_TYPE;
          }
        ;

/*
  Order by statement in ALTER TABLE
*/

alter_order_clause:
          ORDER_SYM BY alter_order_list
        ;

alter_order_list:
          alter_order_list ',' alter_order_item
        | alter_order_item
        ;

alter_order_item:
          simple_ident_nospvar order_dir
          {
            bool ascending= ($2 == 1) ? true : false;
            if (YYSession->add_order_to_list($1, ascending))
              DRIZZLE_YYABORT;
          }
        ;

/*
   Order by statement in select
*/

opt_order_clause:
          /* empty */
        | order_clause
        ;

order_clause:
          ORDER_SYM BY
          {
            Select_Lex *sel= Lex->current_select;
            Select_Lex_Unit *unit= sel-> master_unit();
            if (sel->linkage != GLOBAL_OPTIONS_TYPE &&
                sel->olap != UNSPECIFIED_OLAP_TYPE &&
                (sel->linkage != UNION_TYPE || sel->braces))
            {
              my_error(ER_WRONG_USAGE, MYF(0),
                       "CUBE/ROLLUP", "ORDER BY");
              DRIZZLE_YYABORT;
            }
            if (Lex->sql_command != SQLCOM_ALTER_TABLE && !unit->fake_select_lex)
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
                  unit->add_fake_select_lex(Lex->session))
                DRIZZLE_YYABORT;
            }
          }
          order_list
        ;

order_list:
          order_list ',' order_ident order_dir
          { if (YYSession->add_order_to_list($3,(bool) $4)) DRIZZLE_YYABORT; }
        | order_ident order_dir
          { if (YYSession->add_order_to_list($1,(bool) $2)) DRIZZLE_YYABORT; }
        ;

order_dir:
          /* empty */ { $$ =  1; }
        | ASC  { $$ =1; }
        | DESC { $$ =0; }
        ;

opt_limit_clause_init:
          /* empty */
          {
            Select_Lex *sel= Lex->current_select;
            sel->offset_limit= 0;
            sel->select_limit= 0;
          }
        | limit_clause {}
        ;

opt_limit_clause:
          /* empty */ {}
        | limit_clause {}
        ;

limit_clause:
          LIMIT limit_options {}
        ;

limit_options:
          limit_option
          {
            Select_Lex *sel= Lex->current_select;
            sel->select_limit= $1;
            sel->offset_limit= 0;
            sel->explicit_limit= 1;
          }
        | limit_option ',' limit_option
          {
            Select_Lex *sel= Lex->current_select;
            sel->select_limit= $3;
            sel->offset_limit= $1;
            sel->explicit_limit= 1;
          }
        | limit_option OFFSET_SYM limit_option
          {
            Select_Lex *sel= Lex->current_select;
            sel->select_limit= $1;
            sel->offset_limit= $3;
            sel->explicit_limit= 1;
          }
        ;

limit_option:
          ULONGLONG_NUM { $$= new Item_uint($1.str, $1.length); }
        | LONG_NUM      { $$= new Item_uint($1.str, $1.length); }
        | NUM           { $$= new Item_uint($1.str, $1.length); }
        ;

delete_limit_clause:
          /* empty */
          {
            Lex->current_select->select_limit= 0;
          }
        | LIMIT limit_option
          {
            Select_Lex *sel= Lex->current_select;
            sel->select_limit= $2;
            sel->explicit_limit= 1;
          }
        ;

ulong_num:
          NUM           { int error; $$= (ulong) internal::my_strtoll10($1.str, (char**) 0, &error); }
        | HEX_NUM       { $$= (ulong) strtol($1.str, (char**) 0, 16); }
        | LONG_NUM      { int error; $$= (ulong) internal::my_strtoll10($1.str, (char**) 0, &error); }
        | ULONGLONG_NUM { int error; $$= (ulong) internal::my_strtoll10($1.str, (char**) 0, &error); }
        | DECIMAL_NUM   { int error; $$= (ulong) internal::my_strtoll10($1.str, (char**) 0, &error); }
        | FLOAT_NUM     { int error; $$= (ulong) internal::my_strtoll10($1.str, (char**) 0, &error); }
        ;

ulonglong_num:
          NUM           { int error; $$= (uint64_t) internal::my_strtoll10($1.str, (char**) 0, &error); }
        | ULONGLONG_NUM { int error; $$= (uint64_t) internal::my_strtoll10($1.str, (char**) 0, &error); }
        | LONG_NUM      { int error; $$= (uint64_t) internal::my_strtoll10($1.str, (char**) 0, &error); }
        | DECIMAL_NUM   { int error; $$= (uint64_t) internal::my_strtoll10($1.str, (char**) 0, &error); }
        | FLOAT_NUM     { int error; $$= (uint64_t) internal::my_strtoll10($1.str, (char**) 0, &error); }
        ;

select_var_list_init:
          {
            if (not Lex->describe && (not (Lex->result= new select_dumpvar())))
              DRIZZLE_YYABORT;
          }
          select_var_list
          {}
        ;

select_var_list:
          select_var_list ',' select_var_ident
        | select_var_ident {}
        ;

select_var_ident: 
          '@' user_variable_ident
          {
            if (Lex->result)
            {
              ((select_dumpvar *)Lex->result)->var_list.push_back( new var($2,0,0,(enum_field_types)0));
            }
            else
            {
              /*
                The parser won't create select_result instance only
                if it's an EXPLAIN.
              */
              assert(Lex->describe);
            }
          }
        ;

into:
          INTO
          { }
          into_destination
        ;

into_destination:
          OUTFILE TEXT_STRING_filesystem
          {
            Lex->setCacheable(false);
            if (!(Lex->exchange= new file_exchange($2.str, 0)) ||
                !(Lex->result= new select_export(Lex->exchange)))
              DRIZZLE_YYABORT;
          }
          opt_field_term opt_line_term
        | DUMPFILE TEXT_STRING_filesystem
          {
            if (not Lex->describe)
            {
              Lex->setCacheable(false);
              if (not (Lex->exchange= new file_exchange($2.str,1)))
                DRIZZLE_YYABORT;
              if (not (Lex->result= new select_dump(Lex->exchange)))
                DRIZZLE_YYABORT;
            }
          }
        | select_var_list_init
          {Lex->setCacheable(false);}
        ;

/*
  Drop : delete tables or index or user
*/

drop:
          DROP CATALOG_SYM catalog_name
          {
            Lex->statement= new statement::catalog::Drop(YYSession, $3);
          }
        | DROP opt_temporary table_or_tables if_exists table_list
          {
            Lex->sql_command = SQLCOM_DROP_TABLE;
            statement::DropTable *statement= new statement::DropTable(YYSession);
            Lex->statement= statement;
            statement->drop_temporary= $2;
            statement->drop_if_exists= $4;
          }
        | DROP build_method INDEX_SYM ident ON table_ident {}
          {
            Lex->sql_command= SQLCOM_DROP_INDEX;
            statement::DropIndex *statement= new statement::DropIndex(YYSession);
            Lex->statement= statement;
            statement->alter_info.flags.set(ALTER_DROP_INDEX);
            statement->alter_info.build_method= $2;
            statement->alter_info.drop_list.push_back(new AlterDrop(AlterDrop::KEY, $4.str));
            if (not Lex->current_select->add_table_to_list(Lex->session, $6, NULL,
                                                          TL_OPTION_UPDATING))
              DRIZZLE_YYABORT;
          }
        | DROP DATABASE if_exists schema_name
          {
            Lex->sql_command= SQLCOM_DROP_DB;
            statement::DropSchema *statement= new statement::DropSchema(YYSession);
            Lex->statement= statement;
            statement->drop_if_exists=$3;
            Lex->name= $4;
          }
        ;

table_list:
          table_name
        | table_list ',' table_name
        ;

table_name:
          table_ident
          {
            if (!Lex->current_select->add_table_to_list(YYSession, $1, NULL, TL_OPTION_UPDATING))
              DRIZZLE_YYABORT;
          }
        ;

if_exists:
          /* empty */ { $$= false; }
        | IF EXISTS { $$= true; }
        ;

opt_temporary:
          /* empty */ { $$= false; }
        | TEMPORARY_SYM { $$= true; }
        ;

/*
  Execute a string as dynamic SQL.
*/

execute:
       EXECUTE_SYM execute_var_or_string opt_status opt_concurrent opt_wait
        {
          Lex->statement= new statement::Execute(YYSession, $2, $3, $4, $5);
        }


execute_var_or_string:
         user_variable_ident
         {
            $$.set($1);
         }
        | '@' user_variable_ident
        {
            $$.set($2, true);
        }

opt_status:
          /* empty */ { $$= false; }
        | WITH NO_SYM RETURN_SYM { $$= true; }
        ;

opt_concurrent:
          /* empty */ { $$= false; }
        | CONCURRENT { $$= true; }
        ;

opt_wait:
          /* empty */ { $$= false; }
        | WAIT_SYM { $$= true; }
        ;

/*
** Insert : add new data to table
*/

insert:
          INSERT
          {
            Lex->sql_command= SQLCOM_INSERT;
            Lex->statement= new statement::Insert(YYSession);
            Lex->duplicates= DUP_ERROR;
            init_select(Lex);
            /* for subselects */
            Lex->lock_option= TL_READ;
          }
          opt_ignore insert2
          {
            Lex->current_select->set_lock_for_tables(TL_WRITE_CONCURRENT_INSERT);
            Lex->current_select= &Lex->select_lex;
          }
          insert_field_spec opt_insert_update
          {}
        ;

replace:
          REPLACE
          {
            Lex->sql_command= SQLCOM_REPLACE;
            Lex->statement= new statement::Replace(YYSession);
            Lex->duplicates= DUP_REPLACE;
            init_select(Lex);
          }
          insert2
          {
            Lex->current_select->set_lock_for_tables(TL_WRITE_DEFAULT);
            Lex->current_select= &Lex->select_lex;
          }
          insert_field_spec
          {}
        ;

insert2:
          INTO insert_table {}
        | insert_table {}
        ;

insert_table:
          table_name
          {
            Lex->field_list.empty();
            Lex->many_values.empty();
            Lex->insert_list=0;
          };

insert_field_spec:
          insert_values {}
        | '(' ')' insert_values {}
        | '(' fields ')' insert_values {}
        | SET_SYM
          {
            if (not (Lex->insert_list = new List_item) ||
                Lex->many_values.push_back(Lex->insert_list))
              DRIZZLE_YYABORT;
          }
          ident_eq_list
        ;

fields:
          fields ',' insert_ident { Lex->field_list.push_back($3); }
        | insert_ident { Lex->field_list.push_back($1); }
        ;

insert_values:
          VALUES values_list {}
        | VALUE_SYM values_list {}
        | stored_select
          {
            Lex->current_select->set_braces(0);
          }
          union_clause {}
        | '(' stored_select ')'
          {
            Lex->current_select->set_braces(1);
          }
          union_opt {}
        ;

values_list:
          values_list ','  no_braces
        | no_braces
        ;

ident_eq_list:
          ident_eq_list ',' ident_eq_value
        | ident_eq_value
        ;

ident_eq_value:
          simple_ident_nospvar equal expr_or_default
          {
            if (Lex->field_list.push_back($1) ||
                Lex->insert_list->push_back($3))
              DRIZZLE_YYABORT;
          }
        ;

equal:
          EQ {}
        | SET_VAR {}
        ;

opt_equal:
          /* empty */ {}
        | equal {}
        ;

no_braces:
          '('
          {
              if (!(Lex->insert_list = new List_item))
                DRIZZLE_YYABORT;
          }
          opt_values ')'
          {
            if (Lex->many_values.push_back(Lex->insert_list))
              DRIZZLE_YYABORT;
          }
        ;

opt_values:
          /* empty */ {}
        | values
        ;

values:
          values ','  expr_or_default
          {
            if (Lex->insert_list->push_back($3))
              DRIZZLE_YYABORT;
          }
        | expr_or_default
          {
            if (Lex->insert_list->push_back($1))
              DRIZZLE_YYABORT;
          }
        ;

expr_or_default:
          expr { $$= $1;}
        | DEFAULT {$$= new Item_default_value(Lex->current_context()); }
        ;

opt_insert_update:
          /* empty */
        | ON DUPLICATE_SYM { Lex->duplicates= DUP_UPDATE; }
          KEY_SYM UPDATE_SYM insert_update_list
        ;

/* Update rows in a table */

update:
          UPDATE_SYM opt_ignore table_ident
          {
            init_select(Lex);
            Lex->sql_command= SQLCOM_UPDATE;
            Lex->statement= new statement::Update(YYSession);
            Lex->lock_option= TL_UNLOCK; /* Will be set later */
            Lex->duplicates= DUP_ERROR;
            if (not Lex->select_lex.add_table_to_list(YYSession, $3, NULL,0))
              DRIZZLE_YYABORT;
          }
          SET_SYM update_list
          {
            if (Lex->select_lex.get_table_list()->derived)
            {
              /* it is single table update and it is update of derived table */
              my_error(ER_NON_UPDATABLE_TABLE, MYF(0),
                       Lex->select_lex.get_table_list()->alias, "UPDATE");
              DRIZZLE_YYABORT;
            }
            /*
              In case of multi-update setting write lock for all tables may
              be too pessimistic. We will decrease lock level if possible in
              multi_update().
            */
            Lex->current_select->set_lock_for_tables(TL_WRITE_DEFAULT);
          }
          where_clause opt_order_clause delete_limit_clause {}
        ;

update_list:
          update_list ',' update_elem
        | update_elem
        ;

update_elem:
          simple_ident_nospvar equal expr_or_default
          {
            if (YYSession->add_item_to_list($1) || YYSession->add_value_to_list($3))
              DRIZZLE_YYABORT;
          }
        ;

insert_update_list:
          insert_update_list ',' insert_update_elem
        | insert_update_elem
        ;

insert_update_elem:
          simple_ident_nospvar equal expr_or_default
          {
          if (Lex->update_list.push_back($1) ||
              Lex->value_list.push_back($3))
              DRIZZLE_YYABORT;
          }
        ;

/* Delete rows from a table */

delete:
          DELETE_SYM
          {
            Lex->sql_command= SQLCOM_DELETE;
            Lex->statement= new statement::Delete(YYSession);
            init_select(Lex);
            Lex->lock_option= TL_WRITE_DEFAULT;
            Lex->ignore= 0;
            Lex->select_lex.init_order();
          }
          opt_delete_options single_multi
        ;

single_multi:
          FROM table_ident
          {
            if (!Lex->current_select->add_table_to_list(YYSession, $2, NULL, TL_OPTION_UPDATING,
                                           Lex->lock_option))
              DRIZZLE_YYABORT;
          }
          where_clause opt_order_clause
          delete_limit_clause {}
        ;

opt_delete_options:
          /* empty */ {}
        | opt_delete_option opt_delete_options {}
        ;

opt_delete_option:
         IGNORE_SYM   { Lex->ignore= 1; }
        ;

truncate:
          TRUNCATE_SYM opt_table_sym table_name
          {
            Lex->sql_command= SQLCOM_TRUNCATE;
            Lex->statement= new statement::Truncate(YYSession);
            Lex->select_lex.options= 0;
            Lex->select_lex.init_order();
          }
        ;

opt_table_sym:
          /* empty */
        | TABLE_SYM
        ;

/* Show things */

show:
          SHOW
          {
            Lex->wild=0;
            Lex->lock_option= TL_READ;
            init_select(Lex);
            Lex->current_select->parsing_place= SELECT_LIST;
          }
          show_param
          {}
        ;

/* SHOW SCHEMAS */
show_param:
           DATABASES show_wild
           {
             Lex->sql_command= SQLCOM_SELECT;
             Lex->statement= new statement::Show(YYSession);

             std::string column_name= "Database";
             if (Lex->wild)
             {
               column_name.append(" (");
               column_name.append(Lex->wild->ptr());
               column_name.append(")");
             }

             if (Lex->current_select->where)
             {
               if (prepare_new_schema_table(YYSession, Lex, "SCHEMAS"))
                 DRIZZLE_YYABORT;
             }
             else
             {
               if (prepare_new_schema_table(YYSession, Lex, "SHOW_SCHEMAS"))
                 DRIZZLE_YYABORT;
             }

             Item_field *my_field= new Item_field(&YYSession->lex->current_select->context, NULL, NULL, "SCHEMA_NAME");
             my_field->is_autogenerated_name= false;
             my_field->set_name(column_name.c_str(), column_name.length(), system_charset_info);

             if (YYSession->add_item_to_list(my_field))
               DRIZZLE_YYABORT;

              if (YYSession->add_order_to_list(my_field, true))
                DRIZZLE_YYABORT;
           }
           /* SHOW TABLES */
         | TABLES opt_db show_wild
           {
             Lex->sql_command= SQLCOM_SELECT;

             drizzled::statement::Show *select= new statement::Show(YYSession);
             Lex->statement= select;

              std::string column_name= "Tables_in_";

              util::string::const_shared_ptr schema(YYSession->schema());
              if ($2)
              {
		identifier::Schema identifier($2);
                column_name.append($2);
                Lex->select_lex.db= $2;
                if (not plugin::StorageEngine::doesSchemaExist(identifier))
                {
                  my_error(ER_BAD_DB_ERROR, MYF(0), $2);
                }
                select->setShowPredicate($2, "");
              }
              else if (schema and not schema->empty())
              {
                column_name.append(*schema);
                select->setShowPredicate(*schema, "");
              }
              else
              {
                my_error(ER_NO_DB_ERROR, MYF(0));
                DRIZZLE_YYABORT;
              }


             if (Lex->wild)
             {
               column_name.append(" (");
               column_name.append(Lex->wild->ptr());
               column_name.append(")");
             }

             if (prepare_new_schema_table(YYSession, Lex, "SHOW_TABLES"))
               DRIZZLE_YYABORT;

             Item_field *my_field= new Item_field(&YYSession->lex->current_select->context, NULL, NULL, "TABLE_NAME");
             my_field->is_autogenerated_name= false;
             my_field->set_name(column_name.c_str(), column_name.length(), system_charset_info);

             if (YYSession->add_item_to_list(my_field))
               DRIZZLE_YYABORT;

              if (YYSession->add_order_to_list(my_field, true))
                DRIZZLE_YYABORT;
           }
           /* SHOW TEMPORARY TABLES */
         | TEMPORARY_SYM TABLES show_wild
           {
             Lex->sql_command= SQLCOM_SELECT;

             Lex->statement= new statement::Show(YYSession);


             if (prepare_new_schema_table(YYSession, Lex, "SHOW_TEMPORARY_TABLES"))
               DRIZZLE_YYABORT;

             if (YYSession->add_item_to_list( new Item_field(&YYSession->lex->current_select->
                                                           context,
                                                           NULL, NULL, "*")))
               DRIZZLE_YYABORT;
             (YYSession->lex->current_select->with_wild)++;

           }
           /* SHOW TABLE STATUS */
         | TABLE_SYM STATUS_SYM opt_db show_wild
           {
             Lex->sql_command= SQLCOM_SELECT;
             drizzled::statement::Show *select= new statement::Show(YYSession);
             Lex->statement= select;

             std::string column_name= "Tables_in_";

             util::string::const_shared_ptr schema(YYSession->schema());
             if ($3)
             {
               Lex->select_lex.db= $3;

	       identifier::Schema identifier($3);
               if (not plugin::StorageEngine::doesSchemaExist(identifier))
               {
                 my_error(ER_BAD_DB_ERROR, MYF(0), $3);
               }

               select->setShowPredicate($3, "");
             }
             else if (schema)
             {
               select->setShowPredicate(*schema, "");
             }
             else
             {
               my_error(ER_NO_DB_ERROR, MYF(0));
               DRIZZLE_YYABORT;
             }

             if (prepare_new_schema_table(YYSession, Lex, "SHOW_TABLE_STATUS"))
               DRIZZLE_YYABORT;

             if (YYSession->add_item_to_list( new Item_field(&YYSession->lex->current_select->
                                                           context,
                                                           NULL, NULL, "*")))
               DRIZZLE_YYABORT;
             (YYSession->lex->current_select->with_wild)++;
           }
           /* SHOW COLUMNS FROM table_name */
        | COLUMNS from_or_in table_ident opt_db show_wild
          {
             Lex->sql_command= SQLCOM_SELECT;

             drizzled::statement::Show *select= new statement::Show(YYSession);
             Lex->statement= select;

             util::string::const_shared_ptr schema(YYSession->schema());
             if ($4)
             {
              select->setShowPredicate($4, $3->table.str);
             }
             else if ($3->db.str)
             {
              select->setShowPredicate($3->db.str, $3->table.str);
             }
             else if (schema)
             {
               select->setShowPredicate(*schema, $3->table.str);
             }
             else
             {
               my_error(ER_NO_DB_ERROR, MYF(0));
               DRIZZLE_YYABORT;
             }

             {
               drizzled::identifier::Table identifier(select->getShowSchema().c_str(), $3->table.str);
               if (not plugin::StorageEngine::doesTableExist(*YYSession, identifier))
               {
                   my_error(ER_NO_SUCH_TABLE, MYF(0),
                            select->getShowSchema().c_str(), 
                            $3->table.str);
               }
             }

             if (prepare_new_schema_table(YYSession, Lex, "SHOW_COLUMNS"))
               DRIZZLE_YYABORT;

             if (YYSession->add_item_to_list( new Item_field(&YYSession->lex->current_select->
                                                           context,
                                                           NULL, NULL, "*")))
               DRIZZLE_YYABORT;
             (YYSession->lex->current_select->with_wild)++;

          }
          /* SHOW INDEXES from table */
        | keys_or_index from_or_in table_ident opt_db where_clause
          {
             Lex->sql_command= SQLCOM_SELECT;
             drizzled::statement::Show *select= new statement::Show(YYSession);
             Lex->statement= select;

             util::string::const_shared_ptr schema(YYSession->schema());
             if ($4)
             {
              select->setShowPredicate($4, $3->table.str);
             }
             else if ($3->db.str)
             {
              select->setShowPredicate($3->db.str, $3->table.str);
             }
             else if (schema)
             {
               select->setShowPredicate(*schema, $3->table.str);
             }
             else
             {
               my_error(ER_NO_DB_ERROR, MYF(0));
               DRIZZLE_YYABORT;
             }

             {
               drizzled::identifier::Table identifier(select->getShowSchema().c_str(), $3->table.str);
               if (not plugin::StorageEngine::doesTableExist(*YYSession, identifier))
               {
                   my_error(ER_NO_SUCH_TABLE, MYF(0),
                            select->getShowSchema().c_str(), 
                            $3->table.str);
               }
             }

             if (prepare_new_schema_table(YYSession, Lex, "SHOW_INDEXES"))
               DRIZZLE_YYABORT;

             if (YYSession->add_item_to_list( new Item_field(&YYSession->lex->current_select->
                                                           context,
                                                           NULL, NULL, "*")))
               DRIZZLE_YYABORT;
             (YYSession->lex->current_select->with_wild)++;
          }
        | COUNT_SYM '(' '*' ')' WARNINGS
          {
            (void) create_select_for_variable("warning_count");
             Lex->statement= new statement::Show(YYSession);
          }
        | COUNT_SYM '(' '*' ')' ERRORS
          {
            (void) create_select_for_variable("error_count");
             Lex->statement= new statement::Show(YYSession);
          }
        | WARNINGS opt_limit_clause_init
          {
            Lex->sql_command = SQLCOM_SHOW_WARNS;
            Lex->statement= new statement::ShowWarnings(YYSession);
          }
        | ERRORS opt_limit_clause_init
          {
            Lex->sql_command = SQLCOM_SHOW_ERRORS;
            Lex->statement= new statement::ShowErrors(YYSession);
          }
        | opt_var_type STATUS_SYM show_wild
           {
             Lex->sql_command= SQLCOM_SELECT;
             Lex->statement= new statement::Show(YYSession);

             if ($1 == OPT_GLOBAL)
             {
               if (prepare_new_schema_table(YYSession, Lex, "GLOBAL_STATUS"))
                 DRIZZLE_YYABORT;
             }
             else
             {
               if (prepare_new_schema_table(YYSession, Lex, "SESSION_STATUS"))
                 DRIZZLE_YYABORT;
             }

             std::string key("Variable_name");
             std::string value("Value");

             Item_field *my_field= new Item_field(&YYSession->lex->current_select->context, NULL, NULL, "VARIABLE_NAME");
             my_field->is_autogenerated_name= false;
             my_field->set_name(key.c_str(), key.length(), system_charset_info);

             if (YYSession->add_item_to_list(my_field))
               DRIZZLE_YYABORT;

             my_field= new Item_field(&YYSession->lex->current_select->context, NULL, NULL, "VARIABLE_VALUE");
             my_field->is_autogenerated_name= false;
             my_field->set_name(value.c_str(), value.length(), system_charset_info);

             if (YYSession->add_item_to_list(my_field))
               DRIZZLE_YYABORT;
           }
        | CREATE TABLE_SYM table_ident
           {
             Lex->sql_command= SQLCOM_SELECT;
             statement::Show *select= new statement::Show(YYSession);
             Lex->statement= select;

             if (Lex->statement == NULL)
               DRIZZLE_YYABORT;

             if (prepare_new_schema_table(YYSession, Lex, "TABLE_SQL_DEFINITION"))
               DRIZZLE_YYABORT;

             util::string::const_shared_ptr schema(YYSession->schema());
             if ($3->db.str)
             {
               select->setShowPredicate($3->db.str, $3->table.str);
             }
             else if (schema)
             {
               select->setShowPredicate(*schema, $3->table.str);
             }
             else
             {
               my_error(ER_NO_DB_ERROR, MYF(0));
               DRIZZLE_YYABORT;
             }

             std::string key("Table");
             std::string value("Create Table");

             Item_field *my_field= new Item_field(&YYSession->lex->current_select->context, NULL, NULL, "TABLE_NAME");
             my_field->is_autogenerated_name= false;
             my_field->set_name(key.c_str(), key.length(), system_charset_info);

             if (YYSession->add_item_to_list(my_field))
               DRIZZLE_YYABORT;

             my_field= new Item_field(&YYSession->lex->current_select->context, NULL, NULL, "TABLE_SQL_DEFINITION");
             my_field->is_autogenerated_name= false;
             my_field->set_name(value.c_str(), value.length(), system_charset_info);

             if (YYSession->add_item_to_list(my_field))
               DRIZZLE_YYABORT;
           }
        | PROCESSLIST_SYM
          {
           {
             Lex->sql_command= SQLCOM_SELECT;
             Lex->statement= new statement::Show(YYSession);

             if (prepare_new_schema_table(YYSession, Lex, "PROCESSLIST"))
               DRIZZLE_YYABORT;

             if (YYSession->add_item_to_list( new Item_field(&YYSession->lex->current_select->
                                                           context,
                                                           NULL, NULL, "*")))
               DRIZZLE_YYABORT;
             (YYSession->lex->current_select->with_wild)++;
           }
          }
        | opt_var_type  VARIABLES show_wild
           {
             Lex->sql_command= SQLCOM_SELECT;
             Lex->statement= new statement::Show(YYSession);

             if ($1 == OPT_GLOBAL)
             {
               if (prepare_new_schema_table(YYSession, Lex, "GLOBAL_VARIABLES"))
                 DRIZZLE_YYABORT;
             }
             else
             {
               if (prepare_new_schema_table(YYSession, Lex, "SESSION_VARIABLES"))
                 DRIZZLE_YYABORT;
             }

             std::string key("Variable_name");
             std::string value("Value");

             Item_field *my_field= new Item_field(&YYSession->lex->current_select->context, NULL, NULL, "VARIABLE_NAME");
             my_field->is_autogenerated_name= false;
             my_field->set_name(key.c_str(), key.length(), system_charset_info);

             if (YYSession->add_item_to_list(my_field))
               DRIZZLE_YYABORT;

             my_field= new Item_field(&YYSession->lex->current_select->context, NULL, NULL, "VARIABLE_VALUE");
             my_field->is_autogenerated_name= false;
             my_field->set_name(value.c_str(), value.length(), system_charset_info);

             if (YYSession->add_item_to_list(my_field))
               DRIZZLE_YYABORT;
           }
        | CREATE DATABASE opt_if_not_exists ident
           {
             Lex->sql_command= SQLCOM_SELECT;
             drizzled::statement::Show *select= new statement::Show(YYSession);
             Lex->statement= select;

             if (prepare_new_schema_table(YYSession, Lex, "SCHEMA_SQL_DEFINITION"))
               DRIZZLE_YYABORT;

             util::string::const_shared_ptr schema(YYSession->schema());
             if ($4.str)
             {
              select->setShowPredicate($4.str);
             }
             else if (schema)
             {
               select->setShowPredicate(*schema);
             }
             else
             {
               my_error(ER_NO_DB_ERROR, MYF(0));
               DRIZZLE_YYABORT;
             }

             std::string key("Database");
             std::string value("Create Database");

             Item_field *my_field= new Item_field(&YYSession->lex->current_select->context, NULL, NULL, "SCHEMA_NAME");
             my_field->is_autogenerated_name= false;
             my_field->set_name(key.c_str(), key.length(), system_charset_info);

             if (YYSession->add_item_to_list(my_field))
               DRIZZLE_YYABORT;

             my_field= new Item_field(&YYSession->lex->current_select->context, NULL, NULL, "SCHEMA_SQL_DEFINITION");
             my_field->is_autogenerated_name= false;
             my_field->set_name(value.c_str(), value.length(), system_charset_info);

             if (YYSession->add_item_to_list(my_field))
               DRIZZLE_YYABORT;
           }

opt_db:
          /* empty */  { $$= 0; }
        | from_or_in ident { $$= $2.str; }
        ;

from_or_in:
          FROM
        | IN_SYM
        ;

show_wild:
          /* empty */
        | LIKE TEXT_STRING_sys
          {
            Lex->wild= new (YYSession->mem_root) String($2.str, $2.length,
                                                    system_charset_info);
            if (Lex->wild == NULL)
              DRIZZLE_YYABORT;
          }
        | WHERE expr
          {
            Lex->current_select->where= $2;
            if ($2)
              $2->top_level_item();
          }
        ;

/* A Oracle compatible synonym for show */
describe:
          describe_command table_ident
          {
            Lex->lock_option= TL_READ;
            init_select(Lex);
            Lex->current_select->parsing_place= SELECT_LIST;
            Lex->sql_command= SQLCOM_SELECT;
            drizzled::statement::Show *select= new statement::Show(YYSession);
            Lex->statement= select;
            Lex->select_lex.db= 0;

             util::string::const_shared_ptr schema(YYSession->schema());
             if ($2->db.str)
             {
               select->setShowPredicate($2->db.str, $2->table.str);
             }
             else if (schema)
             {
               select->setShowPredicate(*schema, $2->table.str);
             }
             else
             {
               my_error(ER_NO_DB_ERROR, MYF(0));
               DRIZZLE_YYABORT;
             }

             {
               drizzled::identifier::Table identifier(select->getShowSchema().c_str(), $2->table.str);
               if (not plugin::StorageEngine::doesTableExist(*YYSession, identifier))
               {
                   my_error(ER_NO_SUCH_TABLE, MYF(0),
                            select->getShowSchema().c_str(), 
                            $2->table.str);
               }
             }

             if (prepare_new_schema_table(YYSession, Lex, "SHOW_COLUMNS"))
               DRIZZLE_YYABORT;

             if (YYSession->add_item_to_list( new Item_field(&YYSession->lex->current_select->
                                                           context,
                                                           NULL, NULL, "*")))
             {
               DRIZZLE_YYABORT;
             }
             (YYSession->lex->current_select->with_wild)++;

          }
          opt_describe_column {}
        | describe_command opt_extended_describe
          { Lex->describe|= DESCRIBE_NORMAL; }
          select
          {
            Lex->select_lex.options|= SELECT_DESCRIBE;
          }
        ;

describe_command:
          DESC
        | DESCRIBE
        ;

opt_extended_describe:
          /* empty */ {}
        | EXTENDED_SYM   { Lex->describe|= DESCRIBE_EXTENDED; }
        ;

opt_describe_column:
          /* empty */ {}
        | text_string { Lex->wild= $1; }
        | ident
          {
            Lex->wild= new (YYSession->mem_root) String((const char*) $1.str,
                                                    $1.length,
                                                    system_charset_info);
          }
        ;


/* flush things */

flush:
          FLUSH_SYM
          {
            Lex->sql_command= SQLCOM_FLUSH;
            Lex->statement= new statement::Flush(YYSession);
            Lex->type= 0;
          }
          flush_options
          {}
        ;

flush_options:
          flush_options ',' flush_option
        | flush_option
        ;

flush_option:
          table_or_tables
          {
            statement::Flush *statement= (statement::Flush*)Lex->statement;
            statement->setFlushTables(true);
          }
          opt_table_list {}
        | TABLES WITH READ_SYM LOCK_SYM
          {
            statement::Flush *statement= (statement::Flush*)Lex->statement;
            statement->setFlushTablesWithReadLock(true);
          }
        | LOGS_SYM
          {
            statement::Flush *statement= (statement::Flush*)Lex->statement;
            statement->setFlushLog(true);
          }
        | STATUS_SYM
          {
            statement::Flush *statement= (statement::Flush*)Lex->statement;
            statement->setFlushStatus(true);
          }
        | GLOBAL_SYM STATUS_SYM
          {
            statement::Flush *statement= (statement::Flush*)Lex->statement;
            statement->setFlushGlobalStatus(true);
          }
        ;

opt_table_list:
          /* empty */  {}
        | table_list {}
        ;

/* kill threads */

kill:
          KILL_SYM kill_option expr
          {
            if ($2)
            {
              Lex->type= ONLY_KILL_QUERY;
            }

            Lex->value_list.empty();
            Lex->value_list.push_front($3);
            Lex->sql_command= SQLCOM_KILL;
            Lex->statement= new statement::Kill(YYSession);
          }
        ;

kill_option:
          /* empty */ { $$= false; }
        | CONNECTION_SYM { $$= false; }
        | QUERY_SYM      { $$= true; }
        ;

/* change database */

use:
          USE_SYM schema_name
          {
            Lex->sql_command=SQLCOM_CHANGE_DB;
            Lex->statement= new statement::ChangeSchema(YYSession);
            Lex->select_lex.db= $2.str;
          }
        ;

/* import, export of files */

load:
          LOAD data_file
          {
            Lex->sql_command= SQLCOM_LOAD;
            statement::Load *statement= new statement::Load(YYSession);
            Lex->statement= statement;

            Lex_input_stream *lip= YYSession->m_lip;
            statement->fname_start= lip->get_ptr();
          }
          load_data_lock INFILE TEXT_STRING_filesystem
          {
            Lex->lock_option= $4;
            Lex->duplicates= DUP_ERROR;
            Lex->ignore= 0;
            if (not (Lex->exchange= new file_exchange($6.str, 0, $2)))
              DRIZZLE_YYABORT;
          }
          opt_duplicate INTO
          {
            Lex_input_stream *lip= YYSession->m_lip;
            ((statement::Load *)Lex->statement)->fname_end= lip->get_ptr();
          }
          TABLE_SYM table_ident
          {
            if (!Lex->current_select->add_table_to_list(YYSession,
                    $12, NULL, TL_OPTION_UPDATING,
                    Lex->lock_option))
              DRIZZLE_YYABORT;
            Lex->field_list.empty();
            Lex->update_list.empty();
            Lex->value_list.empty();
          }
          opt_field_term opt_line_term opt_ignore_lines opt_field_or_var_spec
          opt_load_data_set_spec
          {}
        ;

data_file:
        DATA_SYM  { $$= FILETYPE_CSV; };

load_data_lock:
          /* empty */ { $$= TL_WRITE_DEFAULT; }
        | CONCURRENT
          {
              $$= TL_WRITE_CONCURRENT_INSERT;
          }
        ;

opt_duplicate:
          /* empty */ { Lex->duplicates=DUP_ERROR; }
        | REPLACE { Lex->duplicates=DUP_REPLACE; }
        | IGNORE_SYM { Lex->ignore= 1; }
        ;

opt_duplicate_as:
          /* empty */ { Lex->duplicates=DUP_ERROR; }
        | AS { Lex->duplicates=DUP_ERROR; }
        | REPLACE { Lex->duplicates=DUP_REPLACE; }
        | IGNORE_SYM { Lex->ignore= true; }
        | REPLACE AS { Lex->duplicates=DUP_REPLACE; }
        | IGNORE_SYM AS { Lex->ignore= true; }
        ;

opt_field_term:
          /* empty */
        | COLUMNS field_term_list
        ;

field_term_list:
          field_term_list field_term
        | field_term
        ;

field_term:
          TERMINATED BY text_string
          {
            assert(Lex->exchange != 0);
            Lex->exchange->field_term= $3;
          }
        | OPTIONALLY ENCLOSED BY text_string
          {
            assert(Lex->exchange != 0);
            Lex->exchange->enclosed= $4;
            Lex->exchange->opt_enclosed= 1;
          }
        | ENCLOSED BY text_string
          {
            assert(Lex->exchange != 0);
            Lex->exchange->enclosed= $3;
          }
        | ESCAPED BY text_string
          {
            assert(Lex->exchange != 0);
            Lex->exchange->escaped= $3;
          }
        ;

opt_line_term:
          /* empty */
        | LINES line_term_list
        ;

line_term_list:
          line_term_list line_term
        | line_term
        ;

line_term:
          TERMINATED BY text_string
          {
            assert(Lex->exchange != 0);
            Lex->exchange->line_term= $3;
          }
        | STARTING BY text_string
          {
            assert(Lex->exchange != 0);
            Lex->exchange->line_start= $3;
          }
        ;

opt_ignore_lines:
          /* empty */
        | IGNORE_SYM NUM lines_or_rows
          {
            assert(Lex->exchange != 0);
            Lex->exchange->skip_lines= atol($2.str);
          }
        ;

lines_or_rows:
        LINES { }
        | ROWS_SYM { }
        ;

opt_field_or_var_spec:
          /* empty */ {}
        | '(' fields_or_vars ')' {}
        | '(' ')' {}
        ;

fields_or_vars:
          fields_or_vars ',' field_or_var
          { Lex->field_list.push_back($3); }
        | field_or_var
          { Lex->field_list.push_back($1); }
        ;

field_or_var:
          simple_ident_nospvar {$$= $1;}
        | '@' user_variable_ident
          { $$= new Item_user_var_as_out_param($2); }
        ;

opt_load_data_set_spec:
          /* empty */ {}
        | SET_SYM insert_update_list {}
        ;

/* Common definitions */

text_literal:
        TEXT_STRING_literal
        {
          $$ = new Item_string($1.str, $1.length, YYSession->variables.getCollation());
        }
        | text_literal TEXT_STRING_literal
          {
            ((Item_string*) $1)->append($2.str, $2.length);
          }
        ;

text_string:
          TEXT_STRING_literal
          {
            $$= new (YYSession->mem_root) String($1.str,
                                             $1.length,
                                             YYSession->variables.getCollation());
          }
        | HEX_NUM
          {
            Item *tmp= new Item_hex_string($1.str, $1.length);
            /*
              it is OK only emulate fix_fields, because we need only
              value of constant
            */
            $$= tmp ?
              tmp->quick_fix_field(), tmp->val_str((String*) 0) :
              (String*) 0;
          }
        | BIN_NUM
          {
            Item *tmp= new Item_bin_string($1.str, $1.length);
            /*
              it is OK only emulate fix_fields, because we need only
              value of constant
            */
            $$= tmp ? tmp->quick_fix_field(), tmp->val_str((String*) 0) :
              (String*) 0;
          }
        ;

signed_literal:
          literal { $$ = $1; }
        | '+' NUM_literal { $$ = $2; }
        | '-' NUM_literal
          {
            $2->max_length++;
            $$= $2->neg();
          }
        ;

literal:
          text_literal { $$ = $1; }
        | NUM_literal { $$ = $1; }
        | NULL_SYM
          {
            $$ = new Item_null();
            YYSession->m_lip->next_state=MY_LEX_OPERATOR_OR_IDENT;
          }
        | FALSE_SYM { $$= new drizzled::item::False(); }
        | TRUE_SYM { $$= new drizzled::item::True(); }
        | HEX_NUM { $$ = new Item_hex_string($1.str, $1.length);}
        | BIN_NUM { $$= new Item_bin_string($1.str, $1.length); }
        | DATE_SYM text_literal { $$ = $2; }
        | TIMESTAMP_SYM text_literal { $$ = $2; }
        ;

NUM_literal:
          NUM
          {
            int error;
            $$ = new Item_int($1.str, (int64_t) internal::my_strtoll10($1.str, NULL, &error), $1.length);
          }
        | LONG_NUM
          {
            int error;
            $$ = new Item_int($1.str, (int64_t) internal::my_strtoll10($1.str, NULL, &error), $1.length);
          }
        | ULONGLONG_NUM
          { $$ = new Item_uint($1.str, $1.length); }
        | DECIMAL_NUM
          {
            $$= new Item_decimal($1.str, $1.length, YYSession->charset());
            if (YYSession->is_error())
            {
              DRIZZLE_YYABORT;
            }
          }
        | FLOAT_NUM
          {
            $$ = new Item_float($1.str, $1.length);
            if (YYSession->is_error())
            {
              DRIZZLE_YYABORT;
            }
          }
        ;

/**********************************************************************
** Creating different items.
**********************************************************************/

insert_ident:
          simple_ident_nospvar { $$=$1; }
        | table_wild { $$=$1; }
        ;

table_wild:
          ident '.' '*'
          {
            Select_Lex *sel= Lex->current_select;
            $$ = new Item_field(Lex->current_context(), NULL, $1.str, "*");
            sel->with_wild++;
          }
        | ident '.' ident '.' '*'
          {
            Select_Lex *sel= Lex->current_select;
            $$ = new Item_field(Lex->current_context(), $1.str, $3.str,"*");
            sel->with_wild++;
          }
        ;

order_ident:
          expr { $$=$1; }
        ;

simple_ident:
          ident
          {
            {
              Select_Lex *sel=Lex->current_select;
              $$= (sel->parsing_place != IN_HAVING ||
                  sel->get_in_sum_expr() > 0) ?
                  (Item*) new Item_field(Lex->current_context(),
                                         (const char *)NULL, NULL, $1.str) :
                  (Item*) new Item_ref(Lex->current_context(),
                                       (const char *)NULL, NULL, $1.str);
            }
          }
        | simple_ident_q { $$= $1; }
        ;

simple_ident_nospvar:
          ident
          {
            Select_Lex *sel=Lex->current_select;
            $$= (sel->parsing_place != IN_HAVING ||
                sel->get_in_sum_expr() > 0) ?
                (Item*) new Item_field(Lex->current_context(),
                                       (const char *)NULL, NULL, $1.str) :
                (Item*) new Item_ref(Lex->current_context(),
                                     (const char *)NULL, NULL, $1.str);
          }
        | simple_ident_q { $$= $1; }
        ;

simple_ident_q:
          ident '.' ident
          {
            {
              Select_Lex *sel= Lex->current_select;
              if (sel->no_table_names_allowed)
              {
                my_error(ER_TABLENAME_NOT_ALLOWED_HERE,
                         MYF(0), $1.str, YYSession->where);
              }
              $$= (sel->parsing_place != IN_HAVING ||
                  sel->get_in_sum_expr() > 0) ?
                  (Item*) new Item_field(Lex->current_context(),
                                         (const char *)NULL, $1.str, $3.str) :
                  (Item*) new Item_ref(Lex->current_context(),
                                       (const char *)NULL, $1.str, $3.str);
            }
          }
        | '.' ident '.' ident
          {
            Select_Lex *sel= Lex->current_select;
            if (sel->no_table_names_allowed)
            {
              my_error(ER_TABLENAME_NOT_ALLOWED_HERE,
                       MYF(0), $2.str, YYSession->where);
            }
            $$= (sel->parsing_place != IN_HAVING ||
                sel->get_in_sum_expr() > 0) ?
                (Item*) new Item_field(Lex->current_context(), NULL, $2.str, $4.str) :
                (Item*) new Item_ref(Lex->current_context(),
                                     (const char *)NULL, $2.str, $4.str);
          }
        | ident '.' ident '.' ident
          {
            Select_Lex *sel= Lex->current_select;
            if (sel->no_table_names_allowed)
            {
              my_error(ER_TABLENAME_NOT_ALLOWED_HERE,
                       MYF(0), $3.str, YYSession->where);
            }
            $$= (sel->parsing_place != IN_HAVING ||
                sel->get_in_sum_expr() > 0) ?
                (Item*) new Item_field(Lex->current_context(), $1.str, $3.str,
                                       $5.str) :
                (Item*) new Item_ref(Lex->current_context(), $1.str, $3.str,
                                     $5.str);
          }
        ;

field_ident:
          ident { $$=$1;}
        | ident '.' ident '.' ident
          {
            TableList *table=
              reinterpret_cast<TableList*>(Lex->current_select->table_list.first);
            if (my_strcasecmp(table_alias_charset, $1.str, table->getSchemaName()))
            {
              my_error(ER_WRONG_DB_NAME, MYF(0), $1.str);
              DRIZZLE_YYABORT;
            }
            if (my_strcasecmp(table_alias_charset, $3.str,
                              table->getTableName()))
            {
              my_error(ER_WRONG_TABLE_NAME, MYF(0), $3.str);
              DRIZZLE_YYABORT;
            }
            $$=$5;
          }
        | ident '.' ident
          {
            TableList *table=
              reinterpret_cast<TableList*>(Lex->current_select->table_list.first);
            if (my_strcasecmp(table_alias_charset, $1.str, table->alias))
            {
              my_error(ER_WRONG_TABLE_NAME, MYF(0), $1.str);
              DRIZZLE_YYABORT;
            }
            $$=$3;
          }
        | '.' ident { $$=$2;} /* For Delphi */
        ;

table_ident:
          ident { $$=new Table_ident($1); }
        | schema_name '.' ident { $$=new Table_ident($1,$3);}
        | '.' ident { $$=new Table_ident($2);} /* For Delphi */
        ;

schema_name:
          ident
        ;

catalog_name:
          ident
        ;

IDENT_sys:
          IDENT 
          {
            $$= $1;
          }
        | IDENT_QUOTED
          {
            const CHARSET_INFO * const cs= system_charset_info;
            int dummy_error;
            uint32_t wlen= cs->cset->well_formed_len(cs, $1.str,
                                                 $1.str+$1.length,
                                                 $1.length, &dummy_error);
            if (wlen < $1.length)
            {
              my_error(ER_INVALID_CHARACTER_STRING, MYF(0),
                       cs->csname, $1.str + wlen);
              DRIZZLE_YYABORT;
            }
            $$= $1;
          }
        ;

TEXT_STRING_sys:
          TEXT_STRING
          {
            $$= $1;
          }
        ;

TEXT_STRING_literal:
          TEXT_STRING
          {
            $$= $1;
          }
        ;

TEXT_STRING_filesystem:
          TEXT_STRING
          {
            $$= $1;
          }
        ;

ident:
          IDENT_sys    { $$=$1; }
        | keyword
          {
            $$.str= YYSession->strmake($1.str, $1.length);
            $$.length= $1.length;
          }
        ;

ident_or_text:
          IDENT_sys           { $$=$1;}
        | TEXT_STRING_sys { $$=$1;}
        ;

engine_option_value:
          IDENT_sys           { $$=$1;}
        | TEXT_STRING_sys { $$=$1;}
        ;

keyword_exception_for_variable:
          TIMESTAMP_SYM         {}
        | SQL_BUFFER_RESULT     {}
        | IDENTITY_SYM          {}
        ;

/* Keyword that we allow for identifiers (except SP labels) */
keyword:
          keyword_sp            {}
        | BEGIN_SYM             {}
        | CHECKSUM_SYM          {}
        | CLOSE_SYM             {}
        | COMMENT_SYM           {}
        | COMMIT_SYM            {}
        | CONTAINS_SYM          {}
        | DEALLOCATE_SYM        {}
        | DO_SYM                {}
        | END                   {}
        | FLUSH_SYM             {}
        | NO_SYM                {}
        | OPEN_SYM              {}
        | ROLLBACK_SYM          {}
        | SAVEPOINT_SYM         {}
        | SECURITY_SYM          {}
        | SERVER_SYM            {}
        | SIGNED_SYM            {}
        | START_SYM             {}
        | STOP_SYM              {}
        | TRUNCATE_SYM          {}
        ;

/*
 * Keywords that we allow for labels in SPs.
 * Anything that's the beginning of a statement or characteristics
 * must be in keyword above, otherwise we get (harmful) shift/reduce
 * conflicts.
 */
keyword_sp:
          ACTION                   {}
        | ADDDATE_SYM              {}
        | AFTER_SYM                {}
        | AGGREGATE_SYM            {}
        | ANY_SYM                  {}
        | AT_SYM                   {}
        | AUTO_INC                 {}
        | AVG_SYM                  {}
        | BIT_SYM                  {}
        | BOOL_SYM                 {}
        | BOOLEAN_SYM              {}
        | BTREE_SYM                {}
        | CASCADED                 {}
        | CHAIN_SYM                {}
        | COALESCE                 {}
        | COLLATION_SYM            {}
        | COLUMNS                  {}
        | COMMITTED_SYM            {}
        | COMPACT_SYM              {}
        | COMPRESSED_SYM           {}
        | CONCURRENT               {}
        | CONNECTION_SYM           {} /* Causes conflict because of kill */
        | CONSISTENT_SYM           {}
        | CUBE_SYM                 {}
        | DATA_SYM                 {}
        | DATABASES                {}
        | DATETIME_SYM             {}
        | DATE_SYM                 {} /* Create conflict */
        | DAY_SYM                  {}
        | DISABLE_SYM              {}
        | DISCARD                  {}
        | DUMPFILE                 {}
        | DUPLICATE_SYM            {}
        | DYNAMIC_SYM              {}
        | ENDS_SYM                 {}
        | ENUM_SYM                 {}
        | ENGINE_SYM               {}
        | ERRORS                   {}
        | ESCAPE_SYM               {}
        | EXCLUSIVE_SYM            {}
        | EXTENDED_SYM             {}
        | FOUND_SYM                {}
        | ENABLE_SYM               {}
        | FULL                     {}
        | FILE_SYM                 {}
        | FIRST_SYM                {}
        | FIXED_SYM                {}
        | FRAC_SECOND_SYM          {}
        | GLOBAL_SYM               {}
        | HASH_SYM                 {}
        | HOUR_SYM                 {}
        | IDENTIFIED_SYM           {}
        | IMPORT                   {}
        | INDEXES                  {}
        | ISOLATION                {}
        | KEY_BLOCK_SIZE           {}
        | LAST_SYM                 {}
        | LEVEL_SYM                {}
        | LOCAL_SYM                {}
        | LOCKS_SYM                {}
        | LOGS_SYM                 {}
        | MAX_VALUE_SYM            {}
        | MEDIUM_SYM               {}
        | MERGE_SYM                {}
        | MICROSECOND_SYM          {}
        | MINUTE_SYM               {}
        | MODIFY_SYM               {}
        | MODE_SYM                 {}
        | MONTH_SYM                {}
        | NAME_SYM                 {}
        | NAMES_SYM                {}
        | NATIONAL_SYM             {}
        | NEXT_SYM                 {}
        | NEW_SYM                  {}
        | NONE_SYM                 {}
        | OFFLINE_SYM              {}
        | OFFSET_SYM               {}
        | ONE_SHOT_SYM             {}
        | ONE_SYM                  {}
        | ONLINE_SYM               {}
        | PARTIAL                  {}
        | PREV_SYM                 {}
        | PROCESS                  {}
        | PROCESSLIST_SYM          {}
        | QUARTER_SYM              {}
        | QUERY_SYM                {} // Causes conflict
        | REDUNDANT_SYM            {}
        | REPEATABLE_SYM           {}
        | RETURNS_SYM              {}
        | ROLLUP_SYM               {}
        | ROUTINE_SYM              {}
        | ROWS_SYM                 {}
        | ROW_FORMAT_SYM           {}
        | ROW_SYM                  {}
        | SECOND_SYM               {}
        | SERIAL_SYM               {}
        | SERIALIZABLE_SYM         {}
        | SESSION_SYM              {}
        | SIMPLE_SYM               {}
        | SHARE_SYM                {}
        | SNAPSHOT_SYM             {}
        | STATUS_SYM               {}
        | STRING_SYM               {}
        | SUBDATE_SYM              {}
        | SUBJECT_SYM              {}
        | SUSPEND_SYM              {}
        | TABLES                   {}
        | TABLESPACE               {}
        | TEMPORARY_SYM            {}
        | TEXT_SYM                 {}
        | TRANSACTION_SYM          {}
        | TIME_SYM                 {}
        | TIMESTAMP_ADD            {}
        | TIMESTAMP_DIFF           {}
        | TYPE_SYM                 {}
        | UNCOMMITTED_SYM          {}
        | UNDOFILE_SYM             {}
        | UNKNOWN_SYM              {}
        | UUID_SYM                 {}
        | USER                     {}
        | VARIABLES                {}
        | VALUE_SYM                {}
        | WARNINGS                 {}
        | WEEK_SYM                 {}
        | WORK_SYM                 {}
        | YEAR_SYM                 {}
        ;

/* Option functions */

set:
          SET_SYM opt_option
          {
            Lex->sql_command= SQLCOM_SET_OPTION;
            Lex->statement= new statement::SetOption(YYSession);
            init_select(Lex);
            Lex->option_type=OPT_SESSION;
            Lex->var_list.empty();
          }
          option_value_list
          {}
        ;

opt_option:
          /* empty */ {}
        | OPTION {}
        ;

option_value_list:
          option_type_value
        | option_value_list ',' option_type_value
        ;

option_type_value:
          {
          }
          ext_option_value
          {
          }
        ;

option_type:
          option_type2    {}
        | GLOBAL_SYM  { $$=OPT_GLOBAL; }
        | LOCAL_SYM   { $$=OPT_SESSION; }
        | SESSION_SYM { $$=OPT_SESSION; }
        ;

option_type2:
          /* empty */ { $$= OPT_DEFAULT; }
        | ONE_SHOT_SYM { ((statement::SetOption *)Lex->statement)->one_shot_set= true; $$= OPT_SESSION; }
        ;

opt_var_type:
          /* empty */ { $$=OPT_SESSION; }
        | GLOBAL_SYM  { $$=OPT_GLOBAL; }
        | LOCAL_SYM   { $$=OPT_SESSION; }
        | SESSION_SYM { $$=OPT_SESSION; }
        ;

opt_var_ident_type:
          /* empty */     { $$=OPT_DEFAULT; }
        | GLOBAL_SYM '.'  { $$=OPT_GLOBAL; }
        | LOCAL_SYM '.'   { $$=OPT_SESSION; }
        | SESSION_SYM '.' { $$=OPT_SESSION; }
        ;

ext_option_value:
          sys_option_value
        | option_type2 option_value
        ;

sys_option_value:
          option_type internal_variable_name equal set_expr_or_default
          {
            if ($2.var)
            { /* System variable */
              if ($1)
              {
                Lex->option_type= $1;
              }
              Lex->var_list.push_back(SetVarPtr(new set_var(Lex->option_type, $2.var, &$2.base_name, $4)));
            }
          }
        | option_type TRANSACTION_SYM ISOLATION LEVEL_SYM isolation_types
          {
            Lex->option_type= $1;
            Lex->var_list.push_back(SetVarPtr(new set_var(Lex->option_type,
                                              find_sys_var("tx_isolation"),
                                              &null_lex_str,
                                              new Item_int((int32_t)
                                              $5))));
          }
        ;

option_value:
          '@' user_variable_ident equal expr
          {
            Lex->var_list.push_back(SetVarPtr(new set_var_user(new Item_func_set_user_var($2,$4))));
          }
        | '@' '@' opt_var_ident_type internal_variable_name equal set_expr_or_default
          {
            Lex->var_list.push_back(SetVarPtr(new set_var($3, $4.var, &$4.base_name, $6)));
          }
        ;

user_variable_ident:
          internal_variable_ident { $$=$1;}
        | TEXT_STRING_sys { $$=$1;}
        | LEX_HOSTNAME { $$=$1;}
        ;

internal_variable_ident:
          keyword_exception_for_variable
          {
            $$.str= YYSession->strmake($1.str, $1.length);
            $$.length= $1.length;
          }
        | IDENT_sys    { $$=$1; }
        ;

internal_variable_name:
          internal_variable_ident
          {
            /* We have to lookup here since local vars can shadow sysvars */
            {
              /* Not an SP local variable */
              sys_var *tmp= find_sys_var(std::string($1.str, $1.length));
              if (!tmp)
                DRIZZLE_YYABORT;
              $$.var= tmp;
              $$.base_name= null_lex_str;
            }
          }
        ;

isolation_types:
          READ_SYM UNCOMMITTED_SYM { $$= ISO_READ_UNCOMMITTED; }
        | READ_SYM COMMITTED_SYM   { $$= ISO_READ_COMMITTED; }
        | REPEATABLE_SYM READ_SYM  { $$= ISO_REPEATABLE_READ; }
        | SERIALIZABLE_SYM         { $$= ISO_SERIALIZABLE; }
        ;

set_expr_or_default:
          expr { $$=$1; }
        | DEFAULT { $$=0; }
        | ON     { $$=new Item_string("ON",  2, system_charset_info); }
        | ALL    { $$=new Item_string("ALL", 3, system_charset_info); }
        | BINARY { $$=new Item_string("binary", 6, system_charset_info); }
        ;

table_or_tables:
          TABLE_SYM
        | TABLES
        ;

unlock:
          UNLOCK_SYM
          {
            Lex->sql_command= SQLCOM_UNLOCK_TABLES;
            Lex->statement= new statement::UnlockTables(YYSession);
          }
          table_or_tables
          {}
        ;

begin:
          BEGIN_SYM
          {
            Lex->sql_command = SQLCOM_BEGIN;
            Lex->statement= new statement::StartTransaction(YYSession);
          }
          opt_work {}
        ;

opt_work:
          /* empty */ {}
        | WORK_SYM  {}
        ;

opt_chain:
          /* empty */
          { $$= (YYSession->variables.completion_type == 1); }
        | AND_SYM NO_SYM CHAIN_SYM { $$=0; }
        | AND_SYM CHAIN_SYM        { $$=1; }
        ;

opt_release:
          /* empty */
          { $$= (YYSession->variables.completion_type == 2); }
        | RELEASE_SYM        { $$=1; }
        | NO_SYM RELEASE_SYM { $$=0; }
;

opt_savepoint:
          /* empty */ {}
        | SAVEPOINT_SYM {}
        ;

commit:
          COMMIT_SYM opt_work opt_chain opt_release
          {
            Lex->sql_command= SQLCOM_COMMIT;
            statement::Commit *statement= new statement::Commit(YYSession);
            Lex->statement= statement;
            statement->tx_chain= $3;
            statement->tx_release= $4;
          }
        ;

rollback:
          ROLLBACK_SYM opt_work opt_chain opt_release
          {
            Lex->sql_command= SQLCOM_ROLLBACK;
            statement::Rollback *statement= new statement::Rollback(YYSession);
            Lex->statement= statement;
            statement->tx_chain= $3;
            statement->tx_release= $4;
          }
        | ROLLBACK_SYM opt_work TO_SYM opt_savepoint savepoint_ident
          {
            Lex->sql_command= SQLCOM_ROLLBACK_TO_SAVEPOINT;
            Lex->statement= new statement::RollbackToSavepoint(YYSession);
            Lex->ident= $5;
          }
        ;

savepoint:
          SAVEPOINT_SYM savepoint_ident
          {
            Lex->sql_command= SQLCOM_SAVEPOINT;
            Lex->statement= new statement::Savepoint(YYSession);
            Lex->ident= $2;
          }
        ;

release:
          RELEASE_SYM SAVEPOINT_SYM savepoint_ident
          {
            Lex->sql_command= SQLCOM_RELEASE_SAVEPOINT;
            Lex->statement= new statement::ReleaseSavepoint(YYSession);
            Lex->ident= $3;
          }
        ;

savepoint_ident:
               IDENT_sys
               ;

/*
   UNIONS : glue selects together
*/


union_clause:
          /* empty */ {}
        | union_list
        ;

union_list:
          UNION_SYM union_option
          {
            if (add_select_to_union_list(YYSession, Lex, (bool)$2))
              DRIZZLE_YYABORT;
          }
          select_init
          {
            /*
              Remove from the name resolution context stack the context of the
              last select in the union.
            */
            Lex->pop_context();
          }
        ;

union_opt:
          /* Empty */ { $$= 0; }
        | union_list { $$= 1; }
        | union_order_or_limit { $$= 1; }
        ;

union_order_or_limit:
          {
            assert(Lex->current_select->linkage != GLOBAL_OPTIONS_TYPE);
            Select_Lex *sel= Lex->current_select;
            Select_Lex_Unit *unit= sel->master_unit();
            Select_Lex *fake= unit->fake_select_lex;
            if (fake)
            {
              unit->global_parameters= fake;
              fake->no_table_names_allowed= 1;
              Lex->current_select= fake;
            }
            YYSession->where= "global ORDER clause";
          }
          order_or_limit
          {
            YYSession->lex->current_select->no_table_names_allowed= 0;
            YYSession->where= "";
          }
        ;

order_or_limit:
          order_clause opt_limit_clause_init
        | limit_clause
        ;

union_option:
          /* empty */ { $$=1; }
        | DISTINCT  { $$=1; }
        | ALL       { $$=0; }
        ;

query_specification:
          SELECT_SYM select_init2_derived
          {
            $$= Lex->current_select->master_unit()->first_select();
          }
        | '(' select_paren_derived ')'
          {
            $$= Lex->current_select->master_unit()->first_select();
          }
        ;

query_expression_body:
          query_specification
        | query_expression_body
          UNION_SYM union_option
          {
            if (add_select_to_union_list(YYSession, Lex, (bool)$3))
              DRIZZLE_YYABORT;
          }
          query_specification
          {
            Lex->pop_context();
            $$= $1;
          }
        ;

/* Corresponds to <query expression> in the SQL:2003 standard. */
subselect:
          subselect_start query_expression_body subselect_end
          {
            $$= $2;
          }
        ;

subselect_start:
          {
            if (not Lex->expr_allows_subselect)
            {
              struct my_parse_error_st pass= { ER(ER_SYNTAX_ERROR), YYSession };
              my_parse_error(&pass);
              DRIZZLE_YYABORT;
            }
            /*
              we are making a "derived table" for the parenthesis
              as we need to have a lex level to fit the union
              after the parenthesis, e.g.
              (SELECT .. ) UNION ...  becomes
              SELECT * FROM ((SELECT ...) UNION ...)
            */
            if (new_select(Lex, 1))
              DRIZZLE_YYABORT;
          }
        ;

subselect_end:
          {
            Lex->pop_context();
            Select_Lex *child= Lex->current_select;
            Lex->current_select= Lex->current_select->return_after_parsing();
            Lex->nest_level--;
            Lex->current_select->n_child_sum_items += child->n_sum_items;
            /*
              A subselect can add fields to an outer select. Reserve space for
              them.
            */
            Lex->current_select->select_n_where_fields+=
            child->select_n_where_fields;
          }
        ;
/**
  @} (end of group Parser)
*/
