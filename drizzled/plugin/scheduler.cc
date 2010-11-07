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

#include "config.h"

#include <algorithm>

#include "drizzled/plugin/scheduler.h"

#include "drizzled/gettext.h"
#include "drizzled/errmsg_print.h"

using namespace std;

namespace drizzled
{

extern size_t my_thread_stack_size;

vector<plugin::Scheduler *> all_schedulers;

/* Globals (TBK) */
static plugin::Scheduler *scheduler= NULL;


class FindSchedulerByName : public unary_function<plugin::Scheduler *, bool>
{
  const string *name;
public:
  FindSchedulerByName(const string *name_arg)
    : name(name_arg) {}
  result_type operator() (argument_type sched)
  {
    return (bool)((name->compare(sched->getName()) == 0));
  }
};


bool plugin::Scheduler::addPlugin(plugin::Scheduler *sched)
{
  vector<plugin::Scheduler *>::iterator iter=
    find_if(all_schedulers.begin(), all_schedulers.end(), 
            FindSchedulerByName(&sched->getName()));

  if (iter != all_schedulers.end())
  {
    errmsg_printf(ERRMSG_LVL_ERROR,
                  _("Attempted to register a scheduler %s, but a scheduler "
                    "has already been registered with that name.\n"),
                    sched->getName().c_str());
    return true;
  }

  sched->deactivate();
  all_schedulers.push_back(sched);

  return false;
}


void plugin::Scheduler::removePlugin(plugin::Scheduler *sched)
{
  all_schedulers.erase(find(all_schedulers.begin(),
                            all_schedulers.end(),
                            sched));
}


bool plugin::Scheduler::setPlugin(const string& name)
{
  vector<plugin::Scheduler *>::iterator iter=
    find_if(all_schedulers.begin(), all_schedulers.end(), 
            FindSchedulerByName(&name));

  if (iter != all_schedulers.end())
  {
    if (scheduler != NULL)
      scheduler->deactivate();
    scheduler= *iter;
    scheduler->activate();
    return false;
  }

  errmsg_printf(ERRMSG_LVL_WARN,
                _("Attempted to configure %s as the scheduler, which did "
                  "not exist.\n"), name.c_str());
  return true;
}


plugin::Scheduler *plugin::Scheduler::getScheduler()
{
  return scheduler;
}

} /* namespace drizzled */
