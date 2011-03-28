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

#include <drizzled/common_fwd.h>

namespace drizzled {

class Comp_creator
{
public:
  virtual ~Comp_creator() {}                  /* Remove gcc warning */
  virtual Item_bool_func2* create(Item *a, Item *b) const = 0;
  virtual const char* symbol(bool invert) const = 0;
  virtual bool eqne_op() const = 0;
  virtual bool l_op() const = 0;
};

typedef Comp_creator* (*chooser_compare_func_creator)(bool invert);

Comp_creator *comp_eq_creator(bool invert);
Comp_creator *comp_ge_creator(bool invert);
Comp_creator *comp_gt_creator(bool invert);
Comp_creator *comp_le_creator(bool invert);
Comp_creator *comp_lt_creator(bool invert);
Comp_creator *comp_ne_creator(bool invert);

} /* namespace drizzled */

