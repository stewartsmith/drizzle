/* -*- mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; -*-
 *  vim:expandtab:shiftwidth=2:tabstop=2:smarttab:
 *
 *  Copyright (C) 2008 Sun Microsystems, Inc.
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

/**
 * @file
 *
 * Contains function declarations that deal with the SHOW commands.  These
 * will eventually go away, but for now we split these definitions out into
 * their own header file for easier maintenance
 */
#ifndef DRIZZLED_SHOW_H
#define DRIZZLED_SHOW_H

#include <vector>

#include "drizzled/sql_list.h"
#include "drizzled/lex_string.h"
#include "drizzled/sql_parse.h"
#include "drizzled/plugin.h"

namespace drizzled
{

/* Forward declarations */
class String;
class Join;
class Session;
struct st_ha_create_information;
typedef st_ha_create_information HA_CREATE_INFO;
class TableList;

class Table;
typedef class Item COND;

int wild_case_compare(const CHARSET_INFO * const cs, 
                      const char *str,const char *wildstr);

int get_quote_char_for_identifier();

namespace show {

bool buildScemas(Session *session);
bool buildTables(Session *session, const char *ident);
bool buildTemporaryTables(Session *session);
bool buildTableStatus(Session *session, const char *ident);
bool buildDescribe(Session *session, Table_ident *ident);
bool buildCreateSchema(Session *session, LEX_STRING &ident);

} // namespace show

} /* namespace drizzled */

#endif /* DRIZZLED_SHOW_H */
