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
%{

#include <config.h>
#include <drizzled/execute/symbol.h>
#include <drizzled/execute/context.h>

using namespace drizzled;
}%

%error-verbose
%debug
%defines
%expect 0
%output "drizzled/execute/parser.cc"
%defines "drizzled/execute/parser.h"
%lex-param { yyscan_t *scanner }
%name-prefix="execute_"
%parse-param { Context *context }
%parse-param { yyscan_t *scanner }
%pure-parser
%require "2.2"
%start begin
%verbose

%{

#include <config.h>

#include <stdint.h>

#pragma GCC diagnostic ignored "-Wold-style-cast"

#define YYENABLE_NLS 0
#define YYLTYPE_IS_TRIVIAL 0

int execute_lex(YYSTYPE* lvalp, void* scanner);

#define parser_abort(A, B) do { parser::abort_func((A), (B)); YYABORT; } while (0) 

inline void execute_error(Context *context, yyscan_t *scanner, const char *error)
{
  if (not context->end())
    context->abort(context, error);
}

%}


%token <string> STRING
%token <string> QUOTED_STRING

%%

begin:
          STRING ';'
        ;


%% 

namespace drizzled {
namespace execute {

void Context::start() 
{
  execute_parse(this, (void **)scanner);
}

} // namespace execute
} // namespace drizzled
