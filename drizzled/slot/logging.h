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

#ifndef DRIZZLED_SLOT_LOGGING_H
#define DRIZZLED_SLOT_LOGGING_H

#include <drizzled/plugin/logging.h>

#include <vector>

namespace drizzled
{
namespace slot
{

/* there are no parameters other than the session because logging can
 * pull everything it needs out of the session.  If need to add
 * parameters, look at how errmsg.h and errmsg.cc do it. */

class Logging
{
private:
  std::vector<plugin::Logging *> all_loggers;

public:
  Logging() : all_loggers() {}
  ~Logging() {}

  void add(plugin::Logging *handler);
  void remove(plugin::Logging *handler);
  bool pre_do(Session *session);
  bool post_do(Session *session);
};

} /* namespace slot */
} /* namespace drizzled */

#endif /* DRIZZLED_SLOT_LOGGING_H */
