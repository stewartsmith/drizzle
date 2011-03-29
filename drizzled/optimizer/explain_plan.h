/* -*- mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; -*-
 *  vim:expandtab:shiftwidth=2:tabstop=2:smarttab:
 *
 *  Copyright (C) 2008-2009 Sun Microsystems, Inc.
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

#pragma once

namespace drizzled {
namespace optimizer {

/** select type for EXPLAIN */
enum select_type
{ 
  ST_PRIMARY,
  ST_SIMPLE,
  ST_DERIVED,
  ST_DEPENDENT_SUBQUERY,
  ST_UNCACHEABLE_SUBQUERY,
  ST_SUBQUERY,
	ST_DEPENDENT_UNION,
  ST_UNCACHEABLE_UNION,
  ST_UNION,
  ST_UNION_RESULT
};

class ExplainPlan
{
public:

  ExplainPlan()
    :
      join(NULL),
      need_tmp_table(false),
      need_order(false),
      distinct(false),
      message(NULL)
  {}

  ExplainPlan(Join *in_join,
              bool in_need_tmp_table,
              bool in_need_order,
              bool in_distinct,
              const char *in_message)
    :
      join(in_join),
      need_tmp_table(in_need_tmp_table),
      need_order(in_need_order),
      distinct(in_distinct),
      message(in_message)
  {}

  void printPlan();

  bool explainUnion(Session *session,
                    Select_Lex_Unit *unit,
                    select_result *result);

private:

  Join *join;

  bool need_tmp_table;

  bool need_order;

  bool distinct;

  const char *message;
};

} /* namespace optimizer */

} /* namespace drizzled */

