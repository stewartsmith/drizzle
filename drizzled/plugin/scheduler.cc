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
#include <drizzled/plugin/scheduler.h>
#include <drizzled/errmsg_print.h>
#include <drizzled/gettext.h>

namespace drizzled {

typedef std::vector<plugin::Scheduler*> schedulers_t;

static schedulers_t g_schedulers;
static plugin::Scheduler* g_scheduler= NULL;

bool plugin::Scheduler::addPlugin(plugin::Scheduler *sched)
{
  BOOST_FOREACH(schedulers_t::reference it, g_schedulers)
  {
    if (it->getName() != sched->getName())
      continue;
    errmsg_printf(error::ERROR, _("Attempted to register a scheduler %s, but a scheduler has already been registered with that name.\n"), sched->getName().c_str());
    return true;
  }
  sched->deactivate();
  g_schedulers.push_back(sched);
  return false;
}

void plugin::Scheduler::removePlugin(plugin::Scheduler *sched)
{
  g_schedulers.erase(std::find(g_schedulers.begin(), g_schedulers.end(), sched));
}

bool plugin::Scheduler::setPlugin(const std::string& name)
{
  BOOST_FOREACH(schedulers_t::reference it, g_schedulers)
  {
    if (it->getName() != name)
      continue;
    if (g_scheduler)
      g_scheduler->deactivate();
    g_scheduler= it;
    g_scheduler->activate();
    return false;
  }
  errmsg_printf(error::WARN, _("Attempted to configure %s as the scheduler, which did not exist.\n"), name.c_str());
  return true;
}

plugin::Scheduler *plugin::Scheduler::getScheduler()
{
  return g_scheduler;
}

} /* namespace drizzled */
