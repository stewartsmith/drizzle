/*
  *  Copyright (C) 2010 PrimeBase Technologies GmbH, Germany
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
 *
 * Barry Leslie
 *
 * 2010-06-01
 */

#pragma once

#include <drizzled/plugin/event_observer.h>
#include "parameters_ms.h"

namespace drizzled
{

namespace plugin
{
class PBMSEvents: public EventObserver
{
public:

  PBMSEvents(): EventObserver(std::string("PBMSEvents"))
  {
	// Databases that I never want to observe events in.
	PBMSParameters::blackListedDB("pbms");
	PBMSParameters::blackListedDB("DATA_DICTIONARY");
	PBMSParameters::blackListedDB("INFORMATION_SCHEMA");
  }

  void registerTableEventsDo(TableShare &table_share, EventObserverList &observers);
  void registerSchemaEventsDo(const std::string &db, EventObserverList &observers);
  void registerSessionEventsDo(Session &session, EventObserverList &observers);

  bool observeEventDo(EventData &);

};
} /* namespace plugin */
} /* namespace drizzled */
