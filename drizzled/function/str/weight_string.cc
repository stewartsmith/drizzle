/* -*- mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; -*-
 *  vim:expandtab:shiftwidth=2:tabstop=2:smarttab:
 *
 *  Copyright (C) 2008 Sun Microsystems
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

#include <drizzled/server_includes.h>
#include CSTDINT_H
#include <drizzled/function/str/weight_string.h>

void Item_func_weight_string::fix_length_and_dec()
{
  const CHARSET_INFO * const cs= args[0]->collation.collation;
  collation.set(&my_charset_bin, args[0]->collation.derivation);
  flags= my_strxfrm_flag_normalize(flags, cs->levels_for_order);
  max_length= cs->mbmaxlen * cmax(args[0]->max_length, nweights);
  maybe_null= 1;
}

/* Return a weight_string according to collation */
String *Item_func_weight_string::val_str(String *str)
{
  String *res;
  const CHARSET_INFO * const cs= args[0]->collation.collation;
  uint32_t tmp_length, frm_length;
  assert(fixed == 1);

  if (args[0]->result_type() != STRING_RESULT ||
      !(res= args[0]->val_str(str)))
    goto nl;

  tmp_length= cs->coll->strnxfrmlen(cs, cs->mbmaxlen *
                                        cmax(res->length(), nweights));

  if (tmp_value.alloc(tmp_length))
    goto nl;

  frm_length= cs->coll->strnxfrm(cs,
                                 (unsigned char*) tmp_value.ptr(), tmp_length,
                                 nweights ? nweights : tmp_length,
                                 (const unsigned char*) res->ptr(), res->length(),
                                 flags);
  tmp_value.length(frm_length);
  null_value= 0;
  return &tmp_value;

nl:
  null_value= 1;
  return 0;
}

