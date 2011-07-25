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


/* A lexical scanner on a temporary buffer with a yacc interface */

#include <config.h>

#define DRIZZLE_LEX 1

#include <drizzled/sql_reserved_words.h>

#include <drizzled/configmake.h>
#include <drizzled/item/num.h>
#include <drizzled/error.h>
#include <drizzled/session.h>
#include <drizzled/sql_base.h>
#include <drizzled/lookup_symbol.h>
#include <drizzled/index_hint.h>
#include <drizzled/select_result.h>
#include <drizzled/item/subselect.h>
#include <drizzled/statement.h>
#include <drizzled/sql_lex.h>
#include <drizzled/plugin.h>

#include <cstdio>
#include <ctype.h>

#include <drizzled/message/alter_table.pb.h>

union ParserType;

using namespace std;

/* Stay outside of the namespace because otherwise bison goes nuts */
int base_sql_lex(ParserType *arg, drizzled::Session *yysession);

namespace drizzled {

static int lex_one_token(ParserType *arg, drizzled::Session *yysession);

/**
  save order by and tables in own lists.
*/
static void add_to_list(Session *session, SQL_LIST &list, Item *item, bool asc)
{
  Order* order = new (session->mem) Order;
  order->item_ptr= item;
  order->item= &order->item_ptr;
  order->asc = asc;
  order->free_me=0;
  order->used=0;
  order->counter_used= 0;
  list.link_in_list((unsigned char*) order, (unsigned char**) &order->next);
}

/**
  LEX_STRING constant for null-string to be used in parser and other places.
*/
const LEX_STRING null_lex_str= {NULL, 0};

Lex_input_stream::Lex_input_stream(Session *session,
                                   const char* buffer,
                                   unsigned int length) :
  m_session(session),
  yylineno(1),
  yytoklen(0),
  yylval(NULL),
  lookahead_token(END_OF_INPUT),
  lookahead_yylval(NULL),
  m_ptr(buffer),
  m_tok_start(NULL),
  m_tok_end(NULL),
  m_end_of_query(buffer + length),
  m_tok_start_prev(NULL),
  m_buf(buffer),
  m_buf_length(length),
  m_echo(true),
  m_cpp_tok_start(NULL),
  m_cpp_tok_start_prev(NULL),
  m_cpp_tok_end(NULL),
  m_body_utf8(NULL),
  m_cpp_utf8_processed_ptr(NULL),
  next_state(MY_LEX_START),
  ignore_space(1),
  in_comment(NO_COMMENT)
{
  m_cpp_buf= (char*) session->mem.alloc(length + 1);
  m_cpp_ptr= m_cpp_buf;
}

/**
  @brief The operation appends unprocessed part of pre-processed buffer till
  the given pointer (ptr) and sets m_cpp_utf8_processed_ptr to end_ptr.

  The idea is that some tokens in the pre-processed buffer (like character
  set introducers) should be skipped.

  Example:
    CPP buffer: SELECT 'str1', _latin1 'str2';
    m_cpp_utf8_processed_ptr -- points at the "SELECT ...";
    In order to skip "_latin1", the following call should be made:
      body_utf8_append(<pointer to "_latin1 ...">, <pointer to " 'str2'...">)

  @param ptr      Pointer in the pre-processed buffer, which specifies the
                  end of the chunk, which should be appended to the utf8
                  body.
  @param end_ptr  Pointer in the pre-processed buffer, to which
                  m_cpp_utf8_processed_ptr will be set in the end of the
                  operation.
*/
void Lex_input_stream::body_utf8_append(const char *ptr,
                                        const char *end_ptr)
{
  assert(m_cpp_buf <= ptr && ptr <= m_cpp_buf + m_buf_length);
  assert(m_cpp_buf <= end_ptr && end_ptr <= m_cpp_buf + m_buf_length);

  if (!m_body_utf8)
    return;

  if (m_cpp_utf8_processed_ptr >= ptr)
    return;

  int bytes_to_copy= ptr - m_cpp_utf8_processed_ptr;

  memcpy(m_body_utf8_ptr, m_cpp_utf8_processed_ptr, bytes_to_copy);
  m_body_utf8_ptr += bytes_to_copy;
  *m_body_utf8_ptr= 0;

  m_cpp_utf8_processed_ptr= end_ptr;
}

/**
  The operation appends unprocessed part of the pre-processed buffer till
  the given pointer (ptr) and sets m_cpp_utf8_processed_ptr to ptr.

  @param ptr  Pointer in the pre-processed buffer, which specifies the end
              of the chunk, which should be appended to the utf8 body.
*/
void Lex_input_stream::body_utf8_append(const char *ptr)
{
  body_utf8_append(ptr, ptr);
}

/**
  The operation converts the specified text literal to the utf8 and appends
  the result to the utf8-body.

  @param session      Thread context.
  @param txt      Text literal.
  @param txt_cs   Character set of the text literal.
  @param end_ptr  Pointer in the pre-processed buffer, to which
                  m_cpp_utf8_processed_ptr will be set in the end of the
                  operation.
*/
void Lex_input_stream::body_utf8_append_literal(const LEX_STRING *txt,
                                                const char *end_ptr)
{
  if (!m_cpp_utf8_processed_ptr)
    return;

  /* NOTE: utf_txt.length is in bytes, not in symbols. */

  memcpy(m_body_utf8_ptr, txt->str, txt->length);
  m_body_utf8_ptr += txt->length;
  *m_body_utf8_ptr= 0;

  m_cpp_utf8_processed_ptr= end_ptr;
}

/*
  This is called before every query that is to be parsed.
  Because of this, it's critical to not do too much things here.
  (We already do too much here)
*/
void LEX::start(Session *arg)
{
  lex_start(arg);
}

void lex_start(Session *session)
{
  LEX *lex= &session->lex();

  lex->session= lex->unit.session= session;

  lex->context_stack.clear();
  lex->unit.init_query();
  lex->unit.init_select();
  /* 'parent_lex' is used in init_query() so it must be before it. */
  lex->select_lex.parent_lex= lex;
  lex->select_lex.init_query();
  lex->value_list.clear();
  lex->update_list.clear();
  lex->auxiliary_table_list.clear();
  lex->unit.next= lex->unit.master=
    lex->unit.link_next= lex->unit.return_to= 0;
  lex->unit.prev= lex->unit.link_prev= 0;
  lex->unit.slave= lex->unit.global_parameters= lex->current_select=
    lex->all_selects_list= &lex->select_lex;
  lex->select_lex.master= &lex->unit;
  lex->select_lex.prev= &lex->unit.slave;
  lex->select_lex.link_next= lex->select_lex.slave= lex->select_lex.next= 0;
  lex->select_lex.link_prev= (Select_Lex_Node**)&(lex->all_selects_list);
  lex->select_lex.options= 0;
  lex->select_lex.init_order();
  lex->select_lex.group_list.clear();
  lex->describe= 0;
  lex->derived_tables= 0;
  lex->lock_option= TL_READ;
  lex->leaf_tables_insert= 0;
  lex->var_list.clear();
  lex->select_lex.select_number= 1;
  lex->length=0;
  lex->select_lex.in_sum_expr=0;
  lex->select_lex.group_list.clear();
  lex->select_lex.order_list.clear();
  lex->sql_command= SQLCOM_END;
  lex->duplicates= DUP_ERROR;
  lex->ignore= 0;
  lex->escape_used= false;
  lex->query_tables= 0;
  lex->reset_query_tables_list(false);
  lex->expr_allows_subselect= true;
  lex->use_only_table_context= false;

  lex->name.str= 0;
  lex->name.length= 0;
  lex->nest_level=0 ;
  lex->allow_sum_func= 0;
  lex->in_sum_func= NULL;
  lex->type= 0;

  lex->is_lex_started= true;
  lex->statement= NULL;

  lex->is_cross= false;
  lex->reset();
}

void LEX::end()
{
  if (yacc_yyss)
  {
    free(yacc_yyss);
    free(yacc_yyvs);
    yacc_yyss= 0;
    yacc_yyvs= 0;
  }

  safe_delete(result);
  safe_delete(_create_table);
  safe_delete(_alter_table);
  _create_table= NULL;
  _alter_table= NULL;
  _create_field= NULL;

  result= 0;
  setCacheable(true);

  safe_delete(statement);
}

static int find_keyword(Lex_input_stream *lip, uint32_t len, bool function)
{
  /* Plenty of memory for the largest lex symbol we have */
  char tok_upper[64];
  const char *tok= lip->get_tok_start();
  uint32_t tok_pos= 0;
  for (;tok_pos<len && tok_pos<63;tok_pos++)
    tok_upper[tok_pos]=my_toupper(system_charset_info, tok[tok_pos]);
  tok_upper[tok_pos]=0;

  const SYMBOL *symbol= lookup_symbol(tok_upper, len, function);
  if (symbol)
  {
    lip->yylval->symbol.symbol=symbol;
    lip->yylval->symbol.str= (char*) tok;
    lip->yylval->symbol.length=len;

    return symbol->tok;
  }

  return 0;
}

/* make a copy of token before ptr and set yytoklen */
static LEX_STRING get_token(Lex_input_stream *lip, uint32_t skip, uint32_t length)
{
  LEX_STRING tmp;
  lip->yyUnget();                       // ptr points now after last token char
  tmp.length=lip->yytoklen=length;
  tmp.str= lip->m_session->mem.strmake(lip->get_tok_start() + skip, tmp.length);

  lip->m_cpp_text_start= lip->get_cpp_tok_start() + skip;
  lip->m_cpp_text_end= lip->m_cpp_text_start + tmp.length;

  return tmp;
}

/*
 todo:
   There are no dangerous charsets in mysql for function
   get_quoted_token yet. But it should be fixed in the
   future to operate multichar strings (like ucs2)
*/
static LEX_STRING get_quoted_token(Lex_input_stream *lip,
                                   uint32_t skip,
                                   uint32_t length, char quote)
{
  LEX_STRING tmp;
  const char *from, *end;
  char *to;
  lip->yyUnget();                       // ptr points now after last token char
  tmp.length= lip->yytoklen=length;
  tmp.str=(char*) lip->m_session->mem.alloc(tmp.length+1);
  from= lip->get_tok_start() + skip;
  to= tmp.str;
  end= to+length;

  lip->m_cpp_text_start= lip->get_cpp_tok_start() + skip;
  lip->m_cpp_text_end= lip->m_cpp_text_start + length;

  for ( ; to != end; )
  {
    if ((*to++= *from++) == quote)
    {
      from++;					// Skip double quotes
      lip->m_cpp_text_start++;
    }
  }
  *to= 0;					// End null for safety
  return tmp;
}


/*
  Return an unescaped text literal without quotes
  Fix sometimes to do only one scan of the string
*/
static char *get_text(Lex_input_stream *lip, int pre_skip, int post_skip)
{
  unsigned char c,sep;
  bool found_escape= false;
  const charset_info_st * const cs= lip->m_session->charset();

  lip->tok_bitmap= 0;
  sep= lip->yyGetLast();                        // String should end with this
  while (! lip->eof())
  {
    c= lip->yyGet();
    lip->tok_bitmap|= c;
    {
      if (use_mb(cs))
      {
        int l= my_ismbchar(cs, lip->get_ptr() -1, lip->get_end_of_query());
        if (l != 0)
        {
          lip->skip_binary(l-1);
          continue;
        }
      }
    }
    if (c == '\\')
    {					// Escaped character
      found_escape= true;
      if (lip->eof())
        return 0;
      lip->yySkip();
    }
    else if (c == sep)
    {
      if (c == lip->yyGet())            // Check if two separators in a row
      {
        found_escape= true;                 // duplicate. Remember for delete
        continue;
      }
      else
        lip->yyUnget();

      /* Found end. Unescape and return string */
      const char* str= lip->get_tok_start();
      const char* end= lip->get_ptr();
      /* Extract the text from the token */
      str+= pre_skip;
      end-= post_skip;
      assert(end >= str);

      char* start= (char*) lip->m_session->mem.alloc((uint32_t) (end-str)+1);

      lip->m_cpp_text_start= lip->get_cpp_tok_start() + pre_skip;
      lip->m_cpp_text_end= lip->get_cpp_ptr() - post_skip;

      if (! found_escape)
      {
        lip->yytoklen= (uint32_t) (end-str);
        memcpy(start, str, lip->yytoklen);
        start[lip->yytoklen]= 0;
      }
      else
      {
        char *to;

        for (to= start; str != end; str++)
        {
          if (use_mb(cs))
          {
            int l= my_ismbchar(cs, str, end);
            if (l != 0)
            {
              while (l--)
                *to++= *str++;
              str--;
              continue;
            }
          }
          if (*str == '\\' && (str + 1) != end)
          {
            switch (*++str) {
            case 'n':
              *to++= '\n';
              break;
            case 't':
              *to++= '\t';
              break;
            case 'r':
              *to++= '\r';
              break;
            case 'b':
              *to++= '\b';
              break;
            case '0':
              *to++= 0;			// Ascii null
              break;
            case 'Z':			// ^Z must be escaped on Win32
              *to++= '\032';
              break;
            case '_':
            case '%':
              *to++= '\\';		// remember prefix for wildcard
              /* Fall through */
            default:
              *to++= *str;
              break;
            }
          }
          else if (*str == sep)
            *to++= *str++;		// Two ' or "
          else
            *to++ = *str;
        }
        *to= 0;
        lip->yytoklen= (uint32_t) (to - start);
      }
      return start;
    }
  }
  return 0;					// unexpected end of query
}


/*
** Calc type of integer; long integer, int64_t integer or real.
** Returns smallest type that match the string.
** When using uint64_t values the result is converted to a real
** because else they will be unexpected sign changes because all calculation
** is done with int64_t or double.
*/

static const char *long_str= "2147483647";
static const uint32_t long_len= 10;
static const char *signed_long_str= "-2147483648";
static const char *int64_t_str= "9223372036854775807";
static const uint32_t int64_t_len= 19;
static const char *signed_int64_t_str= "-9223372036854775808";
static const uint32_t signed_int64_t_len= 19;
static const char *unsigned_int64_t_str= "18446744073709551615";
static const uint32_t unsigned_int64_t_len= 20;

static inline uint32_t int_token(const char *str,uint32_t length)
{
  if (length < long_len)			// quick normal case
    return NUM;
  bool neg=0;

  if (*str == '+')				// Remove sign and pre-zeros
  {
    str++; length--;
  }
  else if (*str == '-')
  {
    str++; length--;
    neg=1;
  }
  while (*str == '0' && length)
  {
    str++; length --;
  }
  if (length < long_len)
    return NUM;

  uint32_t smaller,bigger;
  const char *cmp;
  if (neg)
  {
    if (length == long_len)
    {
      cmp= signed_long_str+1;
      smaller=NUM;				// If <= signed_long_str
      bigger=LONG_NUM;				// If >= signed_long_str
    }
    else if (length < signed_int64_t_len)
      return LONG_NUM;
    else if (length > signed_int64_t_len)
      return DECIMAL_NUM;
    else
    {
      cmp=signed_int64_t_str+1;
      smaller=LONG_NUM;				// If <= signed_int64_t_str
      bigger=DECIMAL_NUM;
    }
  }
  else
  {
    if (length == long_len)
    {
      cmp= long_str;
      smaller=NUM;
      bigger=LONG_NUM;
    }
    else if (length < int64_t_len)
      return LONG_NUM;
    else if (length > int64_t_len)
    {
      if (length > unsigned_int64_t_len)
        return DECIMAL_NUM;
      cmp=unsigned_int64_t_str;
      smaller=ULONGLONG_NUM;
      bigger=DECIMAL_NUM;
    }
    else
    {
      cmp=int64_t_str;
      smaller=LONG_NUM;
      bigger= ULONGLONG_NUM;
    }
  }
  while (*cmp && *cmp++ == *str++) ;
  return ((unsigned char) str[-1] <= (unsigned char) cmp[-1]) ? smaller : bigger;
}

} /* namespace drizzled */
/*
  base_sql_lex remember the following states from the following sql_baselex()

  - MY_LEX_EOQ			Found end of query
  - MY_LEX_OPERATOR_OR_IDENT	Last state was an ident, text or number
				(which can't be followed by a signed number)
*/
int base_sql_lex(union ParserType *yylval, drizzled::Session *session)
{
  drizzled::Lex_input_stream *lip= session->m_lip;
  int token;

  if (lip->lookahead_token != END_OF_INPUT)
  {
    /*
      The next token was already parsed in advance,
      return it.
    */
    token= lip->lookahead_token;
    lip->lookahead_token= END_OF_INPUT;
    *yylval= *(lip->lookahead_yylval);
    lip->lookahead_yylval= NULL;
    return token;
  }

  token= drizzled::lex_one_token(yylval, session);

  switch(token) {
  case WITH:
    /*
      Parsing 'WITH' 'ROLLUP' requires 2 look ups,
      which makes the grammar LALR(2).
      Replace by a single 'WITH_ROLLUP' or 'WITH_CUBE' token,
      to transform the grammar into a LALR(1) grammar,
      which sql_yacc.yy can process.
    */
    token= drizzled::lex_one_token(yylval, session);
    if (token == ROLLUP_SYM)
    {
      return WITH_ROLLUP_SYM;
    }
    else
    {
      /*
        Save the token following 'WITH'
      */
      lip->lookahead_yylval= lip->yylval;
      lip->yylval= NULL;
      lip->lookahead_token= token;
      return WITH;
    }
  default:
    break;
  }

  return token;
}

namespace drizzled
{

int lex_one_token(ParserType *yylval, drizzled::Session *session)
{
  unsigned char c= 0; /* Just set to shutup GCC */
  bool comment_closed;
  int	tokval, result_state;
  unsigned int length;
  enum my_lex_states state;
  Lex_input_stream *lip= session->m_lip;
  LEX *lex= &session->lex();
  const charset_info_st * const cs= session->charset();
  unsigned char *state_map= cs->state_map;
  unsigned char *ident_map= cs->ident_map;

  lip->yylval=yylval;			// The global state

  lip->start_token();
  state=lip->next_state;
  lip->next_state=MY_LEX_OPERATOR_OR_IDENT;
  for (;;)
  {
    switch (state) {
    case MY_LEX_OPERATOR_OR_IDENT:	// Next is operator or keyword
    case MY_LEX_START:			// Start of token
      // Skip starting whitespace
      while(state_map[c= lip->yyPeek()] == MY_LEX_SKIP)
      {
        if (c == '\n')
          lip->yylineno++;

        lip->yySkip();
      }

      /* Start of real token */
      lip->restart_token();
      c= lip->yyGet();
      state= (enum my_lex_states) state_map[c];
      break;
    case MY_LEX_ESCAPE:
      if (lip->yyGet() == 'N')
      {					// Allow \N as shortcut for NULL
        yylval->lex_str.str=(char*) "\\N";
        yylval->lex_str.length=2;
        return NULL_SYM;
      }
    case MY_LEX_CHAR:			// Unknown or single char token
    case MY_LEX_SKIP:			// This should not happen
      if (c == '-' && lip->yyPeek() == '-' &&
          (my_isspace(cs,lip->yyPeekn(1)) ||
           my_iscntrl(cs,lip->yyPeekn(1))))
      {
        state=MY_LEX_COMMENT;
        break;
      }

      if (c != ')')
        lip->next_state= MY_LEX_START;	// Allow signed numbers

      if (c == ',')
      {
        /*
          Warning:
          This is a work around, to make the "remember_name" rule in
          sql/sql_yacc.yy work properly.
          The problem is that, when parsing "select expr1, expr2",
          the code generated by bison executes the *pre* action
          remember_name (see select_item) *before* actually parsing the
          first token of expr2.
        */
        lip->restart_token();
      }

      return((int) c);

    case MY_LEX_IDENT_OR_HEX:
      if (lip->yyPeek() == '\'')
      {					// Found x'hex-number'
        state= MY_LEX_HEX_NUMBER;
        break;
      }
    case MY_LEX_IDENT_OR_BIN:
      if (lip->yyPeek() == '\'')
      {                                 // Found b'bin-number'
        state= MY_LEX_BIN_NUMBER;
        break;
      }
    case MY_LEX_IDENT:
      const char *start;
      if (use_mb(cs))
      {
        result_state= IDENT_QUOTED;
        if (my_mbcharlen(cs, lip->yyGetLast()) > 1)
        {
          int l = my_ismbchar(cs,
                              lip->get_ptr() -1,
                              lip->get_end_of_query());
          if (l == 0) {
            state = MY_LEX_CHAR;
            continue;
          }
          lip->skip_binary(l - 1);
        }
        while (ident_map[c=lip->yyGet()])
        {
          if (my_mbcharlen(cs, c) > 1)
          {
            int l= my_ismbchar(cs, lip->get_ptr() -1, lip->get_end_of_query());
            if (l == 0)
              break;
            lip->skip_binary(l-1);
          }
        }
      }
      else
      {
        for (result_state= c; ident_map[c= lip->yyGet()]; result_state|= c) {};
        /* If there were non-ASCII characters, mark that we must convert */
        result_state= result_state & 0x80 ? IDENT_QUOTED : IDENT;
      }
      length= lip->yyLength();
      start= lip->get_ptr();
      if (lip->ignore_space)
      {
        /*
          If we find a space then this can't be an identifier. We notice this
          below by checking start != lex->ptr.
        */
        for (; state_map[c] == MY_LEX_SKIP ; c= lip->yyGet()) {};
      }
      if (start == lip->get_ptr() && c == '.' && ident_map[(uint8_t)lip->yyPeek()])
	      lip->next_state=MY_LEX_IDENT_SEP;
      else
      {					// '(' must follow directly if function
        lip->yyUnget();
        if ((tokval = find_keyword(lip, length, c == '(')))
        {
          lip->next_state= MY_LEX_START;	// Allow signed numbers
          return(tokval);		// Was keyword
        }
        lip->yySkip();                  // next state does a unget
      }
      yylval->lex_str=get_token(lip, 0, length);

      lip->body_utf8_append(lip->m_cpp_text_start);

      lip->body_utf8_append_literal(&yylval->lex_str, lip->m_cpp_text_end);

      return(result_state);			// IDENT or IDENT_QUOTED

    case MY_LEX_IDENT_SEP:		// Found ident and now '.'
      yylval->lex_str.str= (char*) lip->get_ptr();
      yylval->lex_str.length= 1;
      c= lip->yyGet();                  // should be '.'
      lip->next_state= MY_LEX_IDENT_START;// Next is an ident (not a keyword)
      if (!ident_map[(uint8_t)lip->yyPeek()])            // Probably ` or "
        lip->next_state= MY_LEX_START;
      return((int) c);

    case MY_LEX_NUMBER_IDENT:		// number or ident which num-start
      if (lip->yyGetLast() == '0')
      {
        c= lip->yyGet();
        if (c == 'x')
        {
          while (my_isxdigit(cs,(c = lip->yyGet()))) ;
          if ((lip->yyLength() >= 3) && !ident_map[c])
          {
            /* skip '0x' */
            yylval->lex_str=get_token(lip, 2, lip->yyLength()-2);
            return (HEX_NUM);
          }
          lip->yyUnget();
          state= MY_LEX_IDENT_START;
          break;
        }
        else if (c == 'b')
        {
          while ((c= lip->yyGet()) == '0' || c == '1') {};
          if ((lip->yyLength() >= 3) && !ident_map[c])
          {
            /* Skip '0b' */
            yylval->lex_str= get_token(lip, 2, lip->yyLength()-2);
            return (BIN_NUM);
          }
          lip->yyUnget();
          state= MY_LEX_IDENT_START;
          break;
        }
        lip->yyUnget();
      }

      while (my_isdigit(cs, (c = lip->yyGet()))) ;
      if (!ident_map[c])
      {					// Can't be identifier
        state=MY_LEX_INT_OR_REAL;
        break;
      }
      if (c == 'e' || c == 'E')
      {
        // The following test is written this way to allow numbers of type 1e1
        if (my_isdigit(cs,lip->yyPeek()) ||
            (c=(lip->yyGet())) == '+' || c == '-')
        {				// Allow 1E+10
          if (my_isdigit(cs,lip->yyPeek()))     // Number must have digit after sign
          {
            lip->yySkip();
            while (my_isdigit(cs,lip->yyGet())) ;
            yylval->lex_str=get_token(lip, 0, lip->yyLength());
            return(FLOAT_NUM);
          }
        }
        lip->yyUnget();
      }
      // fall through
    case MY_LEX_IDENT_START:			// We come here after '.'
      result_state= IDENT;
      if (use_mb(cs))
      {
        result_state= IDENT_QUOTED;
        while (ident_map[c=lip->yyGet()])
        {
          if (my_mbcharlen(cs, c) > 1)
          {
            int l= my_ismbchar(cs, lip->get_ptr() -1, lip->get_end_of_query());
            if (l == 0)
              break;
            lip->skip_binary(l-1);
          }
        }
      }
      else
      {
        for (result_state=0; ident_map[c= lip->yyGet()]; result_state|= c) {};
        /* If there were non-ASCII characters, mark that we must convert */
        result_state= result_state & 0x80 ? IDENT_QUOTED : IDENT;
      }
      if (c == '.' && ident_map[(uint8_t)lip->yyPeek()])
      	lip->next_state=MY_LEX_IDENT_SEP;// Next is '.'

      yylval->lex_str= get_token(lip, 0, lip->yyLength());

      lip->body_utf8_append(lip->m_cpp_text_start);

      lip->body_utf8_append_literal(&yylval->lex_str, lip->m_cpp_text_end);

      return(result_state);

    case MY_LEX_USER_VARIABLE_DELIMITER:	// Found quote char
    {
      uint32_t double_quotes= 0;
      char quote_char= c;                       // Used char
      while ((c=lip->yyGet()))
      {
        int var_length;
        if ((var_length= my_mbcharlen(cs, c)) == 1)
        {
          if (c == quote_char)
          {
                  if (lip->yyPeek() != quote_char)
              break;
                  c=lip->yyGet();
            double_quotes++;
            continue;
          }
        }
        else if (var_length < 1)
          break;				// Error
        lip->skip_binary(var_length-1);
      }
      if (double_quotes)
	      yylval->lex_str=get_quoted_token(lip, 1, lip->yyLength() - double_quotes -1, quote_char);
      else
        yylval->lex_str=get_token(lip, 1, lip->yyLength() -1);
      if (c == quote_char)
        lip->yySkip();                  // Skip end `
      lip->next_state= MY_LEX_START;
      lip->body_utf8_append(lip->m_cpp_text_start);
      lip->body_utf8_append_literal(&yylval->lex_str, lip->m_cpp_text_end);
      return(IDENT_QUOTED);
    }
    case MY_LEX_INT_OR_REAL:		// Complete int or incomplete real
      if (c != '.')
      {					// Found complete integer number.
        yylval->lex_str=get_token(lip, 0, lip->yyLength());
      	return int_token(yylval->lex_str.str,yylval->lex_str.length);
      }
      // fall through
    case MY_LEX_REAL:			// Incomplete real number
      while (my_isdigit(cs,c = lip->yyGet())) ;

      if (c == 'e' || c == 'E')
      {
        c = lip->yyGet();
        if (c == '-' || c == '+')
                c = lip->yyGet();                     // Skip sign
        if (!my_isdigit(cs,c))
        {				// No digit after sign
          state= MY_LEX_CHAR;
          break;
        }
        while (my_isdigit(cs,lip->yyGet())) ;
        yylval->lex_str=get_token(lip, 0, lip->yyLength());
        return(FLOAT_NUM);
      }
      yylval->lex_str=get_token(lip, 0, lip->yyLength());
      return(DECIMAL_NUM);

    case MY_LEX_HEX_NUMBER:		// Found x'hexstring'
      lip->yySkip();                    // Accept opening '
      while (my_isxdigit(cs, (c= lip->yyGet()))) ;
      if (c != '\'')
        return(ABORT_SYM);              // Illegal hex constant
      lip->yySkip();                    // Accept closing '
      length= lip->yyLength();          // Length of hexnum+3
      if ((length % 2) == 0)
        return(ABORT_SYM);              // odd number of hex digits
      yylval->lex_str=get_token(lip,
                                2,          // skip x'
                                length-3);  // don't count x' and last '
      return (HEX_NUM);

    case MY_LEX_BIN_NUMBER:           // Found b'bin-string'
      lip->yySkip();                  // Accept opening '
      while ((c= lip->yyGet()) == '0' || c == '1') {};
      if (c != '\'')
        return(ABORT_SYM);            // Illegal hex constant
      lip->yySkip();                  // Accept closing '
      length= lip->yyLength();        // Length of bin-num + 3
      yylval->lex_str= get_token(lip,
                                 2,         // skip b'
                                 length-3); // don't count b' and last '
      return (BIN_NUM);

    case MY_LEX_CMP_OP:			// Incomplete comparison operator
      if (state_map[(uint8_t)lip->yyPeek()] == MY_LEX_CMP_OP ||
          state_map[(uint8_t)lip->yyPeek()] == MY_LEX_LONG_CMP_OP)
        lip->yySkip();
      if ((tokval = find_keyword(lip, lip->yyLength() + 1, 0)))
      {
        lip->next_state= MY_LEX_START;	// Allow signed numbers
        return(tokval);
      }
      state = MY_LEX_CHAR;		// Something fishy found
      break;

    case MY_LEX_LONG_CMP_OP:		// Incomplete comparison operator
      if (state_map[(uint8_t)lip->yyPeek()] == MY_LEX_CMP_OP ||
          state_map[(uint8_t)lip->yyPeek()] == MY_LEX_LONG_CMP_OP)
      {
        lip->yySkip();
        if (state_map[(uint8_t)lip->yyPeek()] == MY_LEX_CMP_OP)
          lip->yySkip();
      }
      if ((tokval = find_keyword(lip, lip->yyLength() + 1, 0)))
      {
        lip->next_state= MY_LEX_START;	// Found long op
        return(tokval);
      }
      state = MY_LEX_CHAR;		// Something fishy found
      break;

    case MY_LEX_BOOL:
      if (c != lip->yyPeek())
      {
        state=MY_LEX_CHAR;
        break;
      }
      lip->yySkip();
      tokval = find_keyword(lip,2,0);	// Is a bool operator
      lip->next_state= MY_LEX_START;	// Allow signed numbers
      return(tokval);

    case MY_LEX_STRING_OR_DELIMITER:
      if (0)
      {
        state= MY_LEX_USER_VARIABLE_DELIMITER;
        break;
      }
      /* " used for strings */
    case MY_LEX_STRING:			// Incomplete text string
      if (!(yylval->lex_str.str = get_text(lip, 1, 1)))
      {
        state= MY_LEX_CHAR;		// Read char by char
        break;
      }
      yylval->lex_str.length=lip->yytoklen;

      lip->body_utf8_append(lip->m_cpp_text_start);

      lip->body_utf8_append_literal(&yylval->lex_str, lip->m_cpp_text_end);

      lex->text_string_is_7bit= (lip->tok_bitmap & 0x80) ? 0 : 1;
      return(TEXT_STRING);

    case MY_LEX_COMMENT:			//  Comment
      lex->select_lex.options|= OPTION_FOUND_COMMENT;
      while ((c = lip->yyGet()) != '\n' && c) ;
      lip->yyUnget();                   // Safety against eof
      state = MY_LEX_START;		// Try again
      break;

    case MY_LEX_LONG_COMMENT:		/* Long C comment? */
      if (lip->yyPeek() != '*')
      {
        state=MY_LEX_CHAR;		// Probable division
        break;
      }
      lex->select_lex.options|= OPTION_FOUND_COMMENT;
      /* Reject '/' '*', since we might need to turn off the echo */
      lip->yyUnget();

      if (lip->yyPeekn(2) == '!')
      {
        lip->in_comment= DISCARD_COMMENT;
        /* Accept '/' '*' '!', but do not keep this marker. */
        lip->set_echo(false);
        lip->yySkip();
        lip->yySkip();
        lip->yySkip();

        /*
          The special comment format is very strict:
          '/' '*' '!', followed by digits ended by a non-digit.
          There must be at least 5 digits for it to count
        */
        const int MAX_VERSION_SIZE= 16;
        char version_str[MAX_VERSION_SIZE];

        int pos= 0;
        do
        {
          version_str[pos]= lip->yyPeekn(pos);
          pos++;
        } while ((pos < MAX_VERSION_SIZE-1) && isdigit(version_str[pos-1]));
        version_str[pos]= 0;

        /* To keep some semblance of compatibility, we impose a 5 digit floor */
        if (pos > 4)
        {
          uint64_t version;
          version=strtoll(version_str, NULL, 10);

          /* Accept 'M' 'm' 'm' 'd' 'd' */
          lip->yySkipn(pos-1);

          if (version <= DRIZZLE_VERSION_ID)
          {
            /* Expand the content of the special comment as real code */
            lip->set_echo(true);
            state=MY_LEX_START;
            break;
          }
        }
        else
        {
          state=MY_LEX_START;
          lip->set_echo(true);
          break;
        }
      }
      else
      {
        lip->in_comment= PRESERVE_COMMENT;
        lip->yySkip();                  // Accept /
        lip->yySkip();                  // Accept *
      }
      /*
        Discard:
        - regular '/' '*' comments,
        - special comments '/' '*' '!' for a future version,
        by scanning until we find a closing '*' '/' marker.
        Note: There is no such thing as nesting comments,
        the first '*' '/' sequence seen will mark the end.
      */
      comment_closed= false;
      while (! lip->eof())
      {
        c= lip->yyGet();
        if (c == '*')
        {
          if (lip->yyPeek() == '/')
          {
            lip->yySkip();
            comment_closed= true;
            state = MY_LEX_START;
            break;
          }
        }
        else if (c == '\n')
          lip->yylineno++;
      }
      /* Unbalanced comments with a missing '*' '/' are a syntax error */
      if (! comment_closed)
        return (ABORT_SYM);
      state = MY_LEX_START;             // Try again
      lip->in_comment= NO_COMMENT;
      lip->set_echo(true);
      break;

    case MY_LEX_END_LONG_COMMENT:
      if ((lip->in_comment != NO_COMMENT) && lip->yyPeek() == '/')
      {
        /* Reject '*' '/' */
        lip->yyUnget();
        /* Accept '*' '/', with the proper echo */
        lip->set_echo(lip->in_comment == PRESERVE_COMMENT);
        lip->yySkipn(2);
        /* And start recording the tokens again */
        lip->set_echo(true);
        lip->in_comment=NO_COMMENT;
        state=MY_LEX_START;
      }
      else
        state=MY_LEX_CHAR;		// Return '*'
      break;

    case MY_LEX_SET_VAR:		// Check if ':='
      if (lip->yyPeek() != '=')
      {
        state=MY_LEX_CHAR;		// Return ':'
        break;
      }
      lip->yySkip();
      return (SET_VAR);

    case MY_LEX_SEMICOLON:			// optional line terminator
      if (lip->yyPeek())
      {
        state= MY_LEX_CHAR;		// Return ';'
        break;
      }
      lip->next_state=MY_LEX_END;       // Mark for next loop
      return(END_OF_INPUT);

    case MY_LEX_EOL:
      if (lip->eof())
      {
        lip->yyUnget();                 // Reject the last '\0'
        lip->set_echo(false);
        lip->yySkip();
        lip->set_echo(true);
        /* Unbalanced comments with a missing '*' '/' are a syntax error */
        if (lip->in_comment != NO_COMMENT)
          return (ABORT_SYM);
        lip->next_state=MY_LEX_END;     // Mark for next loop
        return(END_OF_INPUT);
      }
      state=MY_LEX_CHAR;
      break;

    case MY_LEX_END:
      lip->next_state=MY_LEX_END;
      return false;			// We found end of input last time

      /* Actually real shouldn't start with . but allow them anyhow */

    case MY_LEX_REAL_OR_POINT:
      if (my_isdigit(cs,lip->yyPeek()))
        state= MY_LEX_REAL;		// Real
      else
      {
      	state= MY_LEX_IDENT_SEP;	// return '.'
        lip->yyUnget();                 // Put back '.'
      }
      break;

    case MY_LEX_USER_END:		// end '@' of user@hostname
      switch (state_map[(uint8_t)lip->yyPeek()]) {
      case MY_LEX_STRING:
      case MY_LEX_USER_VARIABLE_DELIMITER:
      case MY_LEX_STRING_OR_DELIMITER:
      	break;
      case MY_LEX_USER_END:
        lip->next_state=MY_LEX_SYSTEM_VAR;
        break;
      default:
        lip->next_state=MY_LEX_HOSTNAME;
        break;
      }
      yylval->lex_str.str=(char*) lip->get_ptr();
      yylval->lex_str.length=1;
      return((int) '@');

    case MY_LEX_HOSTNAME:		// end '@' of user@hostname
      for (c=lip->yyGet() ;
           my_isalnum(cs,c) || c == '.' || c == '_' ||  c == '$';
           c= lip->yyGet()) ;
      yylval->lex_str=get_token(lip, 0, lip->yyLength());
      return(LEX_HOSTNAME);

    case MY_LEX_SYSTEM_VAR:
      yylval->lex_str.str=(char*) lip->get_ptr();
      yylval->lex_str.length=1;
      lip->yySkip();                                    // Skip '@'
      lip->next_state= (state_map[(uint8_t)lip->yyPeek()] ==
			MY_LEX_USER_VARIABLE_DELIMITER ?
			MY_LEX_OPERATOR_OR_IDENT :
			MY_LEX_IDENT_OR_KEYWORD);
      return((int) '@');

    case MY_LEX_IDENT_OR_KEYWORD:
      /*
        We come here when we have found two '@' in a row.
        We should now be able to handle:
        [(global | local | session) .]variable_name
      */

      for (result_state= 0; ident_map[c= lip->yyGet()]; result_state|= c) {};
      /* If there were non-ASCII characters, mark that we must convert */
      result_state= result_state & 0x80 ? IDENT_QUOTED : IDENT;

      if (c == '.')
        lip->next_state=MY_LEX_IDENT_SEP;

      length= lip->yyLength();
      if (length == 0)
        return(ABORT_SYM);              // Names must be nonempty.

      if ((tokval= find_keyword(lip, length,0)))
      {
        lip->yyUnget();                         // Put back 'c'
        return(tokval);				// Was keyword
      }
      yylval->lex_str=get_token(lip, 0, length);

      lip->body_utf8_append(lip->m_cpp_text_start);

      lip->body_utf8_append_literal(&yylval->lex_str, lip->m_cpp_text_end);

      return(result_state);
    }
  }
}

/*
  Select_Lex structures initialisations
*/
void Select_Lex_Node::init_query()
{
  options= 0;
  linkage= UNSPECIFIED_TYPE;
  no_error= no_table_names_allowed= 0;
  uncacheable.reset();
}

void Select_Lex_Node::init_select()
{
}

void Select_Lex_Unit::init_query()
{
  Select_Lex_Node::init_query();
  linkage= GLOBAL_OPTIONS_TYPE;
  global_parameters= first_select();
  select_limit_cnt= HA_POS_ERROR;
  offset_limit_cnt= 0;
  union_distinct= 0;
  prepared= optimized= executed= 0;
  item= 0;
  union_result= 0;
  table= 0;
  fake_select_lex= 0;
  cleaned= 0;
  item_list.clear();
  describe= 0;
  found_rows_for_union= 0;
}

void Select_Lex::init_query()
{
  Select_Lex_Node::init_query();
  table_list.clear();
  top_join_list.clear();
  join_list= &top_join_list;
  embedding= leaf_tables= 0;
  item_list.clear();
  join= 0;
  having= where= 0;
  olap= UNSPECIFIED_OLAP_TYPE;
  having_fix_field= 0;
  context.select_lex= this;
  context.init();
  /*
    Add the name resolution context of the current (sub)query to the
    stack of contexts for the whole query.
    TODO:
    push_context may return an error if there is no memory for a new
    element in the stack, however this method has no return value,
    thus push_context should be moved to a place where query
    initialization is checked for failure.
  */
  parent_lex->push_context(&context);
  cond_count= between_count= with_wild= 0;
  max_equal_elems= 0;
  ref_pointer_array= 0;
  select_n_where_fields= 0;
  select_n_having_items= 0;
  subquery_in_having= explicit_limit= 0;
  is_item_list_lookup= 0;
  parsing_place= NO_MATTER;
  exclude_from_table_unique_test= false;
  nest_level= 0;
  link_next= 0;
}

void Select_Lex::init_select()
{
  sj_nests.clear();
  group_list.clear();
  db= 0;
  having= 0;
  in_sum_expr= with_wild= 0;
  options= 0;
  braces= 0;
  interval_list.clear();
  inner_sum_func_list= 0;
  linkage= UNSPECIFIED_TYPE;
  order_list.elements= 0;
  order_list.first= 0;
  order_list.next= (unsigned char**) &order_list.first;
  /* Set limit and offset to default values */
  select_limit= 0;      /* denotes the default limit = HA_POS_ERROR */
  offset_limit= 0;      /* denotes the default offset = 0 */
  with_sum_func= 0;
  is_cross= false;
  is_correlated= 0;
  cur_pos_in_select_list= UNDEF_POS;
  non_agg_fields.clear();
  cond_value= having_value= Item::COND_UNDEF;
  inner_refs_list.clear();
  full_group_by_flag.reset();
}

/*
  Select_Lex structures linking
*/

/* include on level down */
void Select_Lex_Node::include_down(Select_Lex_Node *upper)
{
  if ((next= upper->slave))
    next->prev= &next;
  prev= &upper->slave;
  upper->slave= this;
  master= upper;
  slave= 0;
}

/*
  include on level down (but do not link)

  SYNOPSYS
    Select_Lex_Node::include_standalone()
    upper - reference on node underr which this node should be included
    ref - references on reference on this node
*/
void Select_Lex_Node::include_standalone(Select_Lex_Node *upper,
					    Select_Lex_Node **ref)
{
  next= 0;
  prev= ref;
  master= upper;
  slave= 0;
}

/* include neighbour (on same level) */
void Select_Lex_Node::include_neighbour(Select_Lex_Node *before)
{
  if ((next= before->next))
    next->prev= &next;
  prev= &before->next;
  before->next= this;
  master= before->master;
  slave= 0;
}

/* including in global Select_Lex list */
void Select_Lex_Node::include_global(Select_Lex_Node **plink)
{
  if ((link_next= *plink))
    link_next->link_prev= &link_next;
  link_prev= plink;
  *plink= this;
}

//excluding from global list (internal function)
void Select_Lex_Node::fast_exclude()
{
  if (link_prev)
  {
    if ((*link_prev= link_next))
      link_next->link_prev= link_prev;
  }
  // Remove slave structure
  for (; slave; slave= slave->next)
    slave->fast_exclude();

}

/*
  excluding select_lex structure (except first (first select can't be
  deleted, because it is most upper select))
*/
void Select_Lex_Node::exclude()
{
  //exclude from global list
  fast_exclude();
  //exclude from other structures
  if ((*prev= next))
    next->prev= prev;
  /*
     We do not need following statements, because prev pointer of first
     list element point to master->slave
     if (master->slave == this)
       master->slave= next;
  */
}


/*
  Exclude level of current unit from tree of SELECTs

  SYNOPSYS
    Select_Lex_Unit::exclude_level()

  NOTE: units which belong to current will be brought up on level of
  currernt unit
*/
void Select_Lex_Unit::exclude_level()
{
  Select_Lex_Unit *units= 0, **units_last= &units;
  for (Select_Lex *sl= first_select(); sl; sl= sl->next_select())
  {
    // unlink current level from global SELECTs list
    if (sl->link_prev && (*sl->link_prev= sl->link_next))
      sl->link_next->link_prev= sl->link_prev;

    // bring up underlay levels
    Select_Lex_Unit **last= 0;
    for (Select_Lex_Unit *u= sl->first_inner_unit(); u; u= u->next_unit())
    {
      u->master= master;
      last= (Select_Lex_Unit**)&(u->next);
    }
    if (last)
    {
      (*units_last)= sl->first_inner_unit();
      units_last= last;
    }
  }
  if (units)
  {
    // include brought up levels in place of current
    (*prev)= units;
    (*units_last)= (Select_Lex_Unit*)next;
    if (next)
      next->prev= (Select_Lex_Node**)units_last;
    units->prev= prev;
  }
  else
  {
    // exclude currect unit from list of nodes
    (*prev)= next;
    if (next)
      next->prev= prev;
  }
}

/*
  Exclude subtree of current unit from tree of SELECTs
*/
void Select_Lex_Unit::exclude_tree()
{
  for (Select_Lex *sl= first_select(); sl; sl= sl->next_select())
  {
    // unlink current level from global SELECTs list
    if (sl->link_prev && (*sl->link_prev= sl->link_next))
      sl->link_next->link_prev= sl->link_prev;

    // unlink underlay levels
    for (Select_Lex_Unit *u= sl->first_inner_unit(); u; u= u->next_unit())
    {
      u->exclude_level();
    }
  }
  // exclude currect unit from list of nodes
  (*prev)= next;
  if (next)
    next->prev= prev;
}

/**
 * Mark all Select_Lex struct from this to 'last' as dependent
 *
 * @param Pointer to last Select_Lex struct, before wich all
 *        Select_Lex have to be marked as dependent
 * @note 'last' should be reachable from this Select_Lex_Node
 */
void Select_Lex::mark_as_dependent(Select_Lex *last)
{
  /*
    Mark all selects from resolved to 1 before select where was
    found table as depended (of select where was found table)
  */
  for (Select_Lex *s= this;
       s && s != last;
       s= s->outer_select())
  {
    if (! (s->uncacheable.test(UNCACHEABLE_DEPENDENT)))
    {
      // Select is dependent of outer select
      s->uncacheable.set(UNCACHEABLE_DEPENDENT);
      s->uncacheable.set(UNCACHEABLE_UNITED);
      Select_Lex_Unit *munit= s->master_unit();
      munit->uncacheable.set(UNCACHEABLE_UNITED);
      munit->uncacheable.set(UNCACHEABLE_DEPENDENT);
      for (Select_Lex *sl= munit->first_select(); sl ; sl= sl->next_select())
      {
        if (sl != s &&
            ! (sl->uncacheable.test(UNCACHEABLE_DEPENDENT) && sl->uncacheable.test(UNCACHEABLE_UNITED)))
          sl->uncacheable.set(UNCACHEABLE_UNITED);
      }
    }
    s->is_correlated= true;
    Item_subselect *subquery_predicate= s->master_unit()->item;
    if (subquery_predicate)
      subquery_predicate->is_correlated= true;
  }
}

bool Select_Lex_Node::set_braces(bool)
{ return true; }

bool Select_Lex_Node::inc_in_sum_expr()
{ return true; }

uint32_t Select_Lex_Node::get_in_sum_expr()
{ return 0; }

TableList* Select_Lex_Node::get_table_list()
{ return NULL; }

List<Item>* Select_Lex_Node::get_item_list()
{ return NULL; }

TableList *Select_Lex_Node::add_table_to_list(Session *,
                                              Table_ident *,
                                              LEX_STRING *,
                                              const bitset<NUM_OF_TABLE_OPTIONS>&,
                                              thr_lock_type,
                                              List<Index_hint> *,
                                              LEX_STRING *)
{
  return 0;
}


/*
  prohibit using LIMIT clause
*/
bool Select_Lex::test_limit()
{
  if (select_limit != 0)
  {
    my_error(ER_NOT_SUPPORTED_YET, MYF(0),
             "LIMIT & IN/ALL/ANY/SOME subquery");
    return true;
  }
  return false;
}

Select_Lex_Unit* Select_Lex_Unit::master_unit()
{
  return this;
}

Select_Lex* Select_Lex_Unit::outer_select()
{
  return (Select_Lex*) master;
}

void Select_Lex::add_order_to_list(Session *session, Item *item, bool asc)
{
  add_to_list(session, order_list, item, asc);
}

void Select_Lex::add_item_to_list(Session *, Item *item)
{
	item_list.push_back(item);
}

void Select_Lex::add_group_to_list(Session *session, Item *item, bool asc)
{
  add_to_list(session, group_list, item, asc);
}

Select_Lex_Unit* Select_Lex::master_unit()
{
  return (Select_Lex_Unit*) master;
}

Select_Lex* Select_Lex::outer_select()
{
  return (Select_Lex*) master->get_master();
}

bool Select_Lex::set_braces(bool value)
{
  braces= value;
  return false;
}

bool Select_Lex::inc_in_sum_expr()
{
  in_sum_expr++;
  return false;
}

uint32_t Select_Lex::get_in_sum_expr()
{
  return in_sum_expr;
}

TableList* Select_Lex::get_table_list()
{
  return (TableList*) table_list.first;
}

List<Item>* Select_Lex::get_item_list()
{
  return &item_list;
}

void Select_Lex::setup_ref_array(Session *session, uint32_t order_group_num)
{
  if (not ref_pointer_array)
    ref_pointer_array= new (session->mem) Item*[5 * (n_child_sum_items + item_list.size() + select_n_having_items + select_n_where_fields + order_group_num)];
}

void Select_Lex_Unit::print(String *str)
{
  bool union_all= !union_distinct;
  for (Select_Lex *sl= first_select(); sl; sl= sl->next_select())
  {
    if (sl != first_select())
    {
      str->append(STRING_WITH_LEN(" union "));
      if (union_all)
	str->append(STRING_WITH_LEN("all "));
      else if (union_distinct == sl)
        union_all= true;
    }
    if (sl->braces)
      str->append('(');
    sl->print(session, str);
    if (sl->braces)
      str->append(')');
  }
  if (fake_select_lex == global_parameters)
  {
    if (fake_select_lex->order_list.elements)
    {
      str->append(STRING_WITH_LEN(" order by "));
      fake_select_lex->print_order(
        str,
        (Order *) fake_select_lex->order_list.first);
    }
    fake_select_lex->print_limit(session, str);
  }
}

void Select_Lex::print_order(String *str,
                             Order *order)
{
  for (; order; order= order->next)
  {
    if (order->counter_used)
    {
      char buffer[20];
      uint32_t length= snprintf(buffer, 20, "%d", order->counter);
      str->append(buffer, length);
    }
    else
      (*order->item)->print(str);
    if (!order->asc)
      str->append(STRING_WITH_LEN(" desc"));
    if (order->next)
      str->append(',');
  }
}

void Select_Lex::print_limit(Session *, String *str)
{
  Select_Lex_Unit *unit= master_unit();
  Item_subselect *item= unit->item;

  if (item && unit->global_parameters == this)
  {
    Item_subselect::subs_type subs_type= item->substype();
    if (subs_type == Item_subselect::EXISTS_SUBS ||
        subs_type == Item_subselect::IN_SUBS ||
        subs_type == Item_subselect::ALL_SUBS)
    {
      assert(!item->fixed ||
                  /*
                    If not using materialization both:
                    select_limit == 1, and there should be no offset_limit.
                  */
                  (((subs_type == Item_subselect::IN_SUBS) &&
                    ((Item_in_subselect*)item)->exec_method ==
                    Item_in_subselect::MATERIALIZATION) ?
                   true :
                   (select_limit->val_int() == 1L) &&
                   offset_limit == 0));
      return;
    }
  }
  if (explicit_limit)
  {
    str->append(STRING_WITH_LEN(" limit "));
    if (offset_limit)
    {
      offset_limit->print(str);
      str->append(',');
    }
    select_limit->print(str);
  }
}

LEX::~LEX()
{
  delete _create_table;
  delete _alter_table;
}

/*
  Initialize (or reset) Query_tables_list object.

  SYNOPSIS
    reset_query_tables_list()
      init  true  - we should perform full initialization of object with
                    allocating needed memory
            false - object is already initialized so we should only reset
                    its state so it can be used for parsing/processing
                    of new statement

  DESCRIPTION
    This method initializes Query_tables_list so it can be used as part
    of LEX object for parsing/processing of statement. One can also use
    this method to reset state of already initialized Query_tables_list
    so it can be used for processing of new statement.
*/
void Query_tables_list::reset_query_tables_list(bool init)
{
  if (!init && query_tables)
  {
    TableList *table= query_tables;
    for (;;)
    {
      if (query_tables_last == &table->next_global ||
          !(table= table->next_global))
        break;
    }
  }
  query_tables= 0;
  query_tables_last= &query_tables;
  query_tables_own_last= 0;
}

/*
  Initialize LEX object.

  SYNOPSIS
    LEX::LEX()

  NOTE
    LEX object initialized with this constructor can be used as part of
    Session object for which one can safely call open_tables(), lock_tables()
    and close_thread_tables() functions. But it is not yet ready for
    statement parsing. On should use lex_start() function to prepare LEX
    for this.
*/
LEX::LEX() :
    result(0),
    yacc_yyss(0),
    yacc_yyvs(0),
    session(NULL),
    charset(NULL),
    var_list(),
    sql_command(SQLCOM_END),
    statement(NULL),
    option_type(OPT_DEFAULT),
    is_lex_started(0),
    cacheable(true),
    sum_expr_used(false),
    _create_table(NULL),
    _alter_table(NULL),
    _create_field(NULL),
    _exists(false)
{
  reset_query_tables_list(true);
}

/**
  This method should be called only during parsing.
  It is aware of compound statements (stored routine bodies)
  and will initialize the destination with the default
  database of the stored routine, rather than the default
  database of the connection it is parsed in.
  E.g. if one has no current database selected, or current database
  set to 'bar' and then issues:

  CREATE PROCEDURE foo.p1() BEGIN SELECT * FROM t1 END//

  t1 is meant to refer to foo.t1, not to bar.t1.

  This method is needed to support this rule.

  @return true in case of error (parsing should be aborted, false in
  case of success
*/
bool LEX::copy_db_to(char **p_db, size_t *p_db_length) const
{
  return session->copy_db_to(p_db, p_db_length);
}

/*
  initialize limit counters

  SYNOPSIS
    Select_Lex_Unit::set_limit()
    values	- Select_Lex with initial values for counters
*/
void Select_Lex_Unit::set_limit(Select_Lex *sl)
{
  ha_rows select_limit_val;
  uint64_t val;

  val= sl->select_limit ? sl->select_limit->val_uint() : HA_POS_ERROR;
  select_limit_val= (ha_rows)val;
  /*
    Check for overflow : ha_rows can be smaller then uint64_t if
    BIG_TABLES is off.
    */
  if (val != (uint64_t)select_limit_val)
    select_limit_val= HA_POS_ERROR;
  offset_limit_cnt= (ha_rows)(sl->offset_limit ? sl->offset_limit->val_uint() :
                                                 0UL);
  select_limit_cnt= select_limit_val + offset_limit_cnt;
  if (select_limit_cnt < select_limit_val)
    select_limit_cnt= HA_POS_ERROR;		// no limit
}

/*
  Unlink the first table from the global table list and the first table from
  outer select (lex->select_lex) local list

  SYNOPSIS
    unlink_first_table()
    link_to_local	Set to 1 if caller should link this table to local list

  NOTES
    We assume that first tables in both lists is the same table or the local
    list is empty.

  RETURN
    0	If 'query_tables' == 0
    unlinked table
      In this case link_to_local is set.

*/
TableList *LEX::unlink_first_table(bool *link_to_local)
{
  TableList *first;
  if ((first= query_tables))
  {
    /*
      Exclude from global table list
    */
    if ((query_tables= query_tables->next_global))
      query_tables->prev_global= &query_tables;
    else
      query_tables_last= &query_tables;
    first->next_global= 0;

    /*
      and from local list if it is not empty
    */
    if ((*link_to_local= test(select_lex.table_list.first)))
    {
      select_lex.context.table_list=
        select_lex.context.first_name_resolution_table= first->next_local;
      select_lex.table_list.first= (unsigned char*) (first->next_local);
      select_lex.table_list.elements--;	//safety
      first->next_local= 0;
      /*
        Ensure that the global list has the same first table as the local
        list.
      */
      first_lists_tables_same();
    }
  }
  return first;
}

/*
  Bring first local table of first most outer select to first place in global
  table list

  SYNOPSYS
     LEX::first_lists_tables_same()

  NOTES
    In many cases (for example, usual INSERT/DELETE/...) the first table of
    main Select_Lex have special meaning => check that it is the first table
    in global list and re-link to be first in the global list if it is
    necessary.  We need such re-linking only for queries with sub-queries in
    the select list, as only in this case tables of sub-queries will go to
    the global list first.
*/
void LEX::first_lists_tables_same()
{
  TableList *first_table= (TableList*) select_lex.table_list.first;
  if (query_tables != first_table && first_table != 0)
  {
    TableList *next;
    if (query_tables_last == &first_table->next_global)
      query_tables_last= first_table->prev_global;

    if ((next= *first_table->prev_global= first_table->next_global))
      next->prev_global= first_table->prev_global;
    /* include in new place */
    first_table->next_global= query_tables;
    /*
       We are sure that query_tables is not 0, because first_table was not
       first table in the global list => we can use
       query_tables->prev_global without check of query_tables
    */
    query_tables->prev_global= &first_table->next_global;
    first_table->prev_global= &query_tables;
    query_tables= first_table;
  }
}

/*
  Link table back that was unlinked with unlink_first_table()

  SYNOPSIS
    link_first_table_back()
    link_to_local	do we need link this table to local

  RETURN
    global list
*/
void LEX::link_first_table_back(TableList *first, bool link_to_local)
{
  if (first)
  {
    if ((first->next_global= query_tables))
      query_tables->prev_global= &first->next_global;
    else
      query_tables_last= &first->next_global;
    query_tables= first;

    if (link_to_local)
    {
      first->next_local= (TableList*) select_lex.table_list.first;
      select_lex.context.table_list= first;
      select_lex.table_list.first= (unsigned char*) first;
      select_lex.table_list.elements++;	//safety
    }
  }
}

/*
  cleanup lex for case when we open table by table for processing

  SYNOPSIS
    LEX::cleanup_after_one_table_open()

  NOTE
    This method is mostly responsible for cleaning up of selects lists and
    derived tables state. To rollback changes in Query_tables_list one has
    to call Query_tables_list::reset_query_tables_list(false).
*/
void LEX::cleanup_after_one_table_open()
{
  /*
    session->lex().derived_tables & additional units may be set if we open
    a view. It is necessary to clear session->lex().derived_tables flag
    to prevent processing of derived tables during next openTablesLock
    if next table is a real table and cleanup & remove underlying units
    NOTE: all units will be connected to session->lex().select_lex, because we
    have not UNION on most upper level.
    */
  if (all_selects_list != &select_lex)
  {
    derived_tables= 0;
    /* cleunup underlying units (units of VIEW) */
    for (Select_Lex_Unit *un= select_lex.first_inner_unit();
         un;
         un= un->next_unit())
      un->cleanup();
    /* reduce all selects list to default state */
    all_selects_list= &select_lex;
    /* remove underlying units (units of VIEW) subtree */
    select_lex.cut_subtree();
  }
}

/*
  There are Select_Lex::add_table_to_list &
  Select_Lex::set_lock_for_tables are in sql_parse.cc

  Select_Lex::print is in sql_select.cc

  Select_Lex_Unit::prepare, Select_Lex_Unit::exec,
  Select_Lex_Unit::cleanup, Select_Lex_Unit::reinit_exec_mechanism,
  Select_Lex_Unit::change_result
  are in sql_union.cc
*/

/*
  Sets the kind of hints to be added by the calls to add_index_hint().

  SYNOPSIS
    set_index_hint_type()
      type_arg     The kind of hints to be added from now on.
      clause       The clause to use for hints to be added from now on.

  DESCRIPTION
    Used in filling up the tagged hints list.
    This list is filled by first setting the kind of the hint as a
    context variable and then adding hints of the current kind.
    Then the context variable index_hint_type can be reset to the
    next hint type.
*/
void Select_Lex::set_index_hint_type(enum index_hint_type type_arg, index_clause_map clause)
{
  current_index_hint_type= type_arg;
  current_index_hint_clause= clause;
}

/*
  Makes an array to store index usage hints (ADD/FORCE/IGNORE INDEX).

  SYNOPSIS
    alloc_index_hints()
      session         current thread.
*/
void Select_Lex::alloc_index_hints (Session *session)
{
  index_hints= new (session->mem_root) List<Index_hint>();
}

/*
  adds an element to the array storing index usage hints
  (ADD/FORCE/IGNORE INDEX).

  SYNOPSIS
    add_index_hint()
      session         current thread.
      str         name of the index.
      length      number of characters in str.

  RETURN VALUE
    0 on success, non-zero otherwise
*/
void Select_Lex::add_index_hint(Session *session, char *str, uint32_t length)
{
  index_hints->push_front(new (session->mem_root) Index_hint(current_index_hint_type, current_index_hint_clause, str, length));
}

message::AlterTable *LEX::alter_table()
{
  if (not _alter_table)
    _alter_table= new message::AlterTable;

  return _alter_table;
}

} /* namespace drizzled */
