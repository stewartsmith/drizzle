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
#include <drizzled/comp_creator.h>
#include <drizzled/item/cmpfunc.h>

namespace drizzled
{

Comp_creator *comp_eq_creator(bool invert)
{
  return invert?
    (Comp_creator *)Ne_creator::instance():
    (Comp_creator *)Eq_creator::instance();
}


Comp_creator *comp_ge_creator(bool invert)
{
  return invert?
    (Comp_creator *)Lt_creator::instance():
    (Comp_creator *)Ge_creator::instance();
}


Comp_creator *comp_gt_creator(bool invert)
{
  return invert?
    (Comp_creator *)Le_creator::instance():
    (Comp_creator *)Gt_creator::instance();
}


Comp_creator *comp_le_creator(bool invert)
{
  return invert?
    (Comp_creator *)Gt_creator::instance():
    (Comp_creator *)Le_creator::instance();
}


Comp_creator *comp_lt_creator(bool invert)
{
  return invert?
    (Comp_creator *)Ge_creator::instance():
    (Comp_creator *)Lt_creator::instance();
}


Comp_creator *comp_ne_creator(bool invert)
{
  return invert?
    (Comp_creator *)Eq_creator::instance():
    (Comp_creator *)Ne_creator::instance();
}

} /* namespace drizzled */
