/* - mode: c; c-basic-offset: 2; indent-tabs-mode: nil; -*-
 *  vim:expandtab:shiftwidth=2:tabstop=2:smarttab:
 *
 *  Copyright (C) 2011 Brian Aker
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
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

#include <drizzled/session/state.h>
#include <drizzled/definitions.h>
#include <string>

namespace drizzled {
namespace session {

State::State(const char *in_packet, size_t in_packet_length)
{
  if (in_packet_length)
  {
    size_t minimum= std::min(in_packet_length, static_cast<size_t>(PROCESS_LIST_WIDTH));
    _query.resize(minimum + 1);
    memcpy(&_query[0], in_packet, minimum);
  }
  else
  {
    _query.resize(0);
  }
}

const char *State::query() const
{
  if (_query.size())
    return &_query[0];

  return "";
}

const char *State::query(size_t &size) const
{
  if (_query.size())
  {
    size= _query.size() -1;
    return &_query[0];
  }

  size= 0;
  return "";
}

} /* namespace session */
} /* namespace drizzled */
