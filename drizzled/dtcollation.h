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

#pragma once

#include <drizzled/definitions.h>
#include <drizzled/visibility.h>

namespace drizzled {

class DRIZZLED_API DTCollation
{
public:
  const charset_info_st *collation;
  enum Derivation derivation;

  DRIZZLED_LOCAL DTCollation();
  DRIZZLED_LOCAL DTCollation(const charset_info_st * const collation_arg,
                             Derivation derivation_arg);
  void set(DTCollation &dt);
  void set(const charset_info_st * const collation_arg,
           Derivation derivation_arg);
  void set(const charset_info_st * const collation_arg);
  void set(Derivation derivation_arg);
  bool set(DTCollation &dt1, DTCollation &dt2, uint32_t flags= 0);

/**
  Aggregate two collations together taking
  into account their coercibility (aka derivation):.

  0 == DERIVATION_EXPLICIT  - an explicitly written COLLATE clause @n
  1 == DERIVATION_NONE      - a mix of two different collations @n
  2 == DERIVATION_IMPLICIT  - a column @n
  3 == DERIVATION_COERCIBLE - a string constant.

  The most important rules are:
  -# If collations are the same:
  chose this collation, and the strongest derivation.
  -# If collations are different:
  - Character sets may differ, but only if conversion without
  data loss is possible. The caller provides flags whether
  character set conversion attempts should be done. If no
  flags are substituted, then the character sets must be the same.
  Currently processed flags are:
  MY_COLL_ALLOW_SUPERSET_CONV  - allow conversion to a superset
  MY_COLL_ALLOW_COERCIBLE_CONV - allow conversion of a coercible value
  - two EXPLICIT collations produce an error, e.g. this is wrong:
  CONCAT(expr1 collate latin1_swedish_ci, expr2 collate latin1_german_ci)
  - the side with smaller derivation value wins,
  i.e. a column is stronger than a string constant,
  an explicit COLLATE clause is stronger than a column.
  - if derivations are the same, we have DERIVATION_NONE,
  we'll wait for an explicit COLLATE clause which possibly can
  come from another argument later: for example, this is valid,
  but we don't know yet when collecting the first two arguments:
     @code
       CONCAT(latin1_swedish_ci_column,
              latin1_german1_ci_column,
              expr COLLATE latin1_german2_ci)
  @endcode
*/

  DRIZZLED_LOCAL bool aggregate(DTCollation &dt, uint32_t flags= 0);

  DRIZZLED_LOCAL const char *derivation_name() const;

};


bool agg_item_collations(DTCollation &c, const char *name,
                         Item **items, uint32_t nitems,
                         uint32_t flags, int item_sep);
bool agg_item_collations_for_comparison(DTCollation &c, const char *name,
                                        Item **items, uint32_t nitems,
                                        uint32_t flags);

/*

 @note In Drizzle we have just one charset, so no conversion is required (though collation may).

  Collect arguments' character sets together.

  We allow to apply automatic character set conversion in some cases.
  The conditions when conversion is possible are:
  - arguments A and B have different charsets
  - A wins according to coercibility rules
    (i.e. a column is stronger than a string constant,
     an explicit COLLATE clause is stronger than a column)
  - character set of A is either superset for character set of B,
    or B is a string constant which can be converted into the
    character set of A without data loss.

  If all of the above is true, then it's possible to convert
  B into the character set of A, and then compare according
  to the collation of A.

  For functions with more than two arguments:
  @code
    collect(A,B,C) ::= collect(collect(A,B),C)
  @endcode
  Since this function calls Session::change_item_tree() on the passed Item **
  pointers, it is necessary to pass the original Item **'s, not copies.
  Otherwise their values will not be properly restored (see BUG#20769).
  If the items are not consecutive (eg. args[2] and args[5]), use the
  item_sep argument, ie.
  @code
    agg_item_charsets(coll, fname, &args[2], 2, flags, 3)
  @endcode
*/
bool agg_item_charsets(DTCollation &c, const char *name,
                       Item **items, uint32_t nitems, uint32_t flags,
                       int item_sep);


void my_coll_agg_error(DTCollation &c1, DTCollation &c2, const char *fname);
void my_coll_agg_error(DTCollation &c1, DTCollation &c2, DTCollation &c3,
                       const char *fname);
void my_coll_agg_error(Item** args, uint32_t count, const char *fname,
                       int item_sep);

} /* namespace drizzled */

