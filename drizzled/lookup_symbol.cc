/* -*- mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; -*-
 *  vim:expandtab:shiftwidth=2:tabstop=2:smarttab:
 *
 *  Copyright (C) 2009 Sun Microsystems, Inc.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; version 2 of the License.
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
#include <drizzled/item/num.h>
#include <drizzled/error.h>
#include <drizzled/session.h>
#include <drizzled/sql_base.h>
#include <drizzled/lookup_symbol.h>
#include <drizzled/comp_creator.h>
#include <drizzled/sql_lex.h>

#include <drizzled/lex_symbol.h>
#include <drizzled/function_hash.h>
#include <drizzled/symbol_hash.h>

namespace drizzled
{

const SYMBOL *lookup_symbol(const char *s, unsigned int len, bool function)
{
  const SYMBOL* ret_sym= NULL;
  if (function)
  {
    ret_sym= function_hash::in_word_set(s, len);
    if (ret_sym && ret_sym->tok)
      return ret_sym;
  }
  ret_sym= symbol_hash::in_word_set(s, len);
  if (ret_sym && ret_sym->tok)
    return ret_sym;
  return NULL;
}

} /* namespace drizzled */
