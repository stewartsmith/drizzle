/* - mode: c; c-basic-offset: 2; indent-tabs-mode: nil; -*-
 *  vim:expandtab:shiftwidth=2:tabstop=2:smarttab:
 *
 *  Copyright (C) 2011 Andrew Hutchings
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
#include <plugin/protocol_dictionary/dictionary.h>
#include <drizzled/plugin/listen.h>

using namespace drizzled;

ProtocolTool::ProtocolTool() :
  plugin::TableFunction("DATA_DICTIONARY", "PROTOCOL_COUNTERS")
{
  add_field("PROTOCOL");
  add_field("COUNTER");
  add_field("VALUE", plugin::TableFunction::NUMBER, 0, false);
}

ProtocolTool::Generator::Generator(Field **arg) :
  plugin::TableFunction::Generator(arg)
{
  protocol_it= plugin::Listen::getListenProtocols().begin();
  protocol= *protocol_it;
  counter_it= protocol->getListenCounters().begin();
}

bool ProtocolTool::Generator::populate()
{
  if (protocol_it == plugin::Listen::getListenProtocols().end())
    return false;

  protocol= *protocol_it;

  while (counter_it == protocol->getListenCounters().end())
  {
    protocol_it++;
    if (protocol_it == plugin::Listen::getListenProtocols().end())
      return false;
    protocol= *protocol_it;
    counter_it= protocol->getListenCounters().begin();
  }

  fill();
  counter_it++;
  return true;
}

void ProtocolTool::Generator::fill()
{
  protocol= *protocol_it;
  counter= *counter_it;

  push(protocol->getName());
  push(*counter->first);
  push((uint64_t) *counter->second);
}

static ProtocolTool *protocols;

static int init(drizzled::module::Context &context)
{
  protocols= new ProtocolTool;
	context.add(protocols);
  return 0;
}

DRIZZLE_DECLARE_PLUGIN
{
  DRIZZLE_VERSION_ID,
  "protocol_dictionary",
  "1.0",
  "Andrew Hutchings",
  "Provides dictionary for protocol counters.",
  PLUGIN_LICENSE_GPL,
  init,     /* Plugin Init */
  NULL,               /* depends */
  NULL                /* config options   */
}
DRIZZLE_DECLARE_PLUGIN_END;

