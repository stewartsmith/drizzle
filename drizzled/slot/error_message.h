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

#ifndef DRIZZLED_SLOT_ERROR_MESSAGE_H
#define DRIZZLED_SLOT_ERROR_MESSAGE_H

// need stdarg for va_list
#include <stdarg.h>
#include <vector>

namespace drizzled
{
namespace plugin
{
  class ErrorMessage;
}
namespace slot
{

class ErrorMessage
{
private:
  std::vector<plugin::ErrorMessage *> all_errmsg_handler;

  bool errmsg_has;

public:
  ErrorMessage() : all_errmsg_handler(), errmsg_has(false) {}
  ~ErrorMessage() {}

  void add(plugin::ErrorMessage *handler);
  void remove(plugin::ErrorMessage *handler);

  bool vprintf(Session *session, int priority, char const *format, va_list ap);
};

} /* namespace slot */
} /* namespace drizzled */

#endif /* DRIZZLED_SLOT_ERROR_MESSAGE_H */
