/*  vim:expandtab:shiftwidth=2:tabstop=2:smarttab:
 * 
 *  Drizzle Execute Parser
 *
 *  Copyright (C) 2011 Data Differential, http://datadifferential.com/
 *  Copyright (C) 2011 Vijay Samuel, vjsamuel1990@gmail.com
 *
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

#include <config.h>
#include <drizzled/execute/parser.h>  
#include <drizzled/execute/context.h>
#include <drizzled/execute/symbol.h>
#include <drizzled/lex_string.h>

using namespace drizzled;

#ifndef __INTEL_COMPILER
#pragma GCC diagnostic ignored "-Wold-style-cast"
#pragma GCC diagnostic ignored "-Wsign-compare"
#pragma GCC diagnostic ignored "-Wunused-parameter"
#pragma GCC diagnostic ignored "-Wmissing-declarations"
#endif

#define YY_EXTRA_TYPE ::drizzled::execute::Context*

}


%{
#include <cstdio>
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


UTF8_BYTE_ORDER_MARK   [\xEF][\xBB][\xBF]

DIGITS    [0-9]
ASCII     [\x0-\xFF]

U1          [a-zA-Z_]
U2          [\xC2-\xDF][\x80-\xBF]
U3          [\xE0][\xA0-\xBF][\x80-\xBF]
U4          [\xE1-\xEC][\x80-\xBF][\x80-\xBF]
U5          [\xED][\x80-\x9F][\x80-\xBF]
U6          [\xEE-\xEF][\x80-\xBF][\x80-\xBF]
U7          [\nxF0][\x90-\xBF][\x80-\xBF][\x80-\xBF]
U8          [\xF1-\xF3][\x80-\xBF][\x80-\xBF][\x80-\xBF]
U9          [\xF4][\x80-\x8F][\x80-\xBF][\x80-\xBF]

LETTERS     {ASCII}|{U2}|{U3}|{U4}|{U5}|{U6}|{U7}|{U8}|{U9} 
ALNUM       {LETTERS}|{DIGITS}

%option nounistd
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

{UTF8_BYTE_ORDER_MARK}       {  /*  Byte Order Mark */   } 

[\t\r\n ] ; /* skip whitespace */


[[:punct:]]?{ALNUM}*["."{ALNUM}_-]*{ALNUM}*[[:punct:]]? {
      yylval->string.str = yytext;
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

