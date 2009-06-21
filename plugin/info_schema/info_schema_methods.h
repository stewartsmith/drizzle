/* -*- mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; -*-
 *  vim:expandtab:shiftwidth=2:tabstop=2:smarttab:
 *
 *  Copyright (C) 2009 Sun Microsystems
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

#ifndef DRIZZLED_INFO_SCHEMA_METHODS_H
#define DRIZZLED_INFO_SCHEMA_METHODS_H

#include "drizzled/info_schema.h"

/**
 * @class
 *   CharSetISMethods
 * @brief
 *   Class which implements any methods that the 
 *   CHARACTER_SET I_S table needs besides the default
 *   methods.
 */
class CharSetISMethods : public InfoSchemaMethods
{
public:
  virtual int fillTable(Session *session, 
                        TableList *tables,
                        COND *cond);
  virtual int oldFormat(Session *session, InfoSchemaTable *schema_table) const;
};

/**
 * @class
 *   ProcessListISMethods
 * @brief
 *   Class which implements any methods that the PROCESSLIST
 *   I_S table needs besides the default methods
 */
class ProcessListISMethods : public InfoSchemaMethods
{
public:
  virtual int fillTable(Session *session,
                        TableList *tables,
                        COND *cond);
};

#endif /* DRIZZLE_INFO_SCHEMA_METHODS_H */
