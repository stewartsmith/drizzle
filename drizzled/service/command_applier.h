/* -*- mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; -*-
 *  vim:expandtab:shiftwidth=2:tabstop=2:smarttab:
 *
 *  Copyright (C) 2009 Sun Microsystems
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

#ifndef DRIZZLED_SERVICE_COMMAND_APPLIER_H
#define DRIZZLED_SERVICE_COMMAND_APPLIER_H

namespace drizzled
{
namespace plugin
{
  class CommandApplier;
}
  
namespace service
{

class CommandApplier
{
public:
  void add(plugin::CommandApplier *applier);

  void remove(plugin::CommandApplier *applier);

};


} /* end namespace service */
} /* end namespace drizzled */

#endif /* DRIZZLED_SERVICE_COMMAND_APPLIER_H */
