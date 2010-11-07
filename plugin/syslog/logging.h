/* -*- mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; -*-
 *  vim:expandtab:shiftwidth=2:tabstop=2:smarttab:
 *
 *  Copyright (C) 2009 Sun Microsystems
 *  Copyright (C) 2010 Mark Atwood
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

#ifndef PLUGIN_SYSLOG_LOGGING_H
#define PLUGIN_SYSLOG_LOGGING_H

#include "module.h"
#include <drizzled/plugin/logging.h>

class Logging_syslog: public drizzled::plugin::Logging
{
 private:
  int syslog_facility;
  int syslog_priority;

  Logging_syslog(const Logging_syslog&);
  Logging_syslog& operator=(const Logging_syslog&);

 public:
  Logging_syslog();

  virtual bool post (drizzled::Session *session);
};

#endif /* PLUGIN_SYSLOG_LOGGING_H */
