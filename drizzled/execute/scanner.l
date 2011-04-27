/*  vim:expandtab:shiftwidth=2:tabstop=2:smarttab:
 * 
 *  Drizzle Execute Parser
 *
 *  Copyright (C) 2011 Data Differential, http://datadifferential.com/
 *  All rights reserved.
 *
 *  Redistribution and use in source and binary forms, with or without
 *  modification, are permitted provided that the following conditions are
 *  met:
 *
 *      * Redistributions of source code must retain the above copyright
 *  notice, this list of conditions and the following disclaimer.
 *
 *      * Redistributions in binary form must reproduce the above
 *  copyright notice, this list of conditions and the following disclaimer
 *  in the documentation and/or other materials provided with the
 *  distribution.
 *
 *      * The names of its contributors may not be used to endorse or
 *  promote products derived from this software without specific prior
 *  written permission.
 *
 *  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 *  "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 *  LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 *  A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 *  OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 *  SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 *  LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 *  DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 *  THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 *  (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 *  OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */

%top{

#pragma GCC diagnostic ignored "-Wold-style-cast"
#pragma GCC diagnostic ignored "-Wsign-compare"
#pragma GCC diagnostic ignored "-Wunused-parameter"

#define YY_EXTRA_TYPE Context*

}


%{
#include <cstdlib>
#include <cstring>

#define PARAM yyget_extra(yyscanner)

#define get_lex_chars(buffer, result, max_size, context) \
{ \
  if (context->pos >= context->length) \
  { \
    result= YY_NULL; \
  } \
  else \
  { \
    result= (int)(context->length - context->pos); \
    (size_t)result > (size_t)max_size ? result= max_size : 0; \
    memcpy(buffer, context->buf + context->pos, result); \
    context->pos += result; \
  } \
}


#define YY_INPUT(buffer, result, max_size) get_lex_chars(buffer, result, max_size, PARAM)

%}

%option 8bit
%option bison-bridge
%option case-insensitive
%option debug
%option nounput
%option noyywrap
%option outfile="drizzled/execute/scanner.cc" header-file="drizzled/execute/scanner.h"
%option perf-report
%option prefix="execute_"
%option reentrant

%%


[\t\r\n] ; /* skip whitespace */

[[:alnum:]]["."[:alnum:]_-]+[[:alnum:]] {
      yylval->string.c_str = yytext;
      yylval->string.length = yyleng;

      return STRING;
    }

.   {
      yyextra->begin= yytext;
      return UNKNOWN;
    }

%%

namespace drizzled {
namespace execute {

void Context::init_scanner()
{
  yylex_init(&scanner);
  yyset_extra(this, scanner);
}

void Context::destroy_scanner()
{
  yylex_destroy(scanner);
}

} // namespace execute
} // namespace drizzled

