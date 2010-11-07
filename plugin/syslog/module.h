/* -*- mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; -*-
 *  vim:expandtab:shiftwidth=2:tabstop=2:smarttab:
 *
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

#ifndef PLUGIN_SYSLOG_MODULE_H
#define PLUGIN_SYSLOG_MODULE_H

namespace syslog_module
{

extern char* sysvar_ident;
extern char* sysvar_facility;
extern bool sysvar_logging_enable;
extern char* sysvar_logging_priority;
extern unsigned long sysvar_logging_threshold_slow;
extern unsigned long sysvar_logging_threshold_big_resultset;
extern unsigned long sysvar_logging_threshold_big_examined;
extern bool sysvar_errmsg_enable;
extern char* sysvar_errmsg_priority;

} // namespace syslog_module

#endif /* PLUGIN_SYSLOG_MODULE_H */
