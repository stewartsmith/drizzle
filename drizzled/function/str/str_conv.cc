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

#include <config.h>

#include <drizzled/function/str/str_conv.h>

namespace drizzled
{

String *Item_str_conv::val_str(String *str)
{
  assert(fixed == 1);
  String *res;
  if (!(res=args[0]->val_str(str)))
  {
    null_value=1;
    return 0;
  }
  null_value=0;
  if (multiply == 1)
  {
    uint32_t len;
    res= copy_if_not_alloced(str,res,res->length());
    len= converter(collation.collation, (char*) res->ptr(), res->length(),
                                        (char*) res->ptr(), res->length());
    assert(len <= res->length());
    res->length(len);
  }
  else
  {
    uint32_t len= res->length() * multiply;
    tmp_value.alloc(len);
    tmp_value.set_charset(collation.collation);
    len= converter(collation.collation, (char*) res->ptr(), res->length(),
                                        (char*) tmp_value.ptr(), len);
    tmp_value.length(len);
    res= &tmp_value;
  }
  return res;
}

void Item_func_lcase::fix_length_and_dec()
{
  collation.set(args[0]->collation);
  multiply= collation.collation->casedn_multiply;
  converter= collation.collation->cset->casedn;
  max_length= args[0]->max_length * multiply;
}

void Item_func_ucase::fix_length_and_dec()
{
  collation.set(args[0]->collation);
  multiply= collation.collation->caseup_multiply;
  converter= collation.collation->cset->caseup;
  max_length= args[0]->max_length * multiply;
}

} /* namespace drizzled */
