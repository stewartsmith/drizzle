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


#ifndef DRIZZLED_SELECT_RESULT_INTERCEPTOR_H
#define DRIZZLED_SELECT_RESULT_INTERCEPTOR_H

#include <drizzled/select_result.h>

namespace drizzled
{

/*
  Base class for select_result descendands which intercept and
  transform result set rows. As the rows are not sent to the client,
  sending of result set metadata should be suppressed as well.
*/

class select_result_interceptor: public select_result
{
public:
  select_result_interceptor() {}              /* Remove gcc warning */
  uint32_t field_count(List<Item> &) const
  { return 0; }
  bool send_fields(List<Item> &)
  { return false; }
};


} /* namespace drizzled */

#endif /* DRIZZLED_SELECT_RESULT_INTERCEPTOR_H */
