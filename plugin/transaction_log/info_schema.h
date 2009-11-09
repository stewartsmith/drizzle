/* - mode: c; c-basic-offset: 2; indent-tabs-mode: nil; -*-
 *  vim:expandtab:shiftwidth=2:tabstop=2:smarttab:
 *
 *  Copyright (C) 2009 Sun Microsystems
 *
 *  Authors:
 *
 *  Jay Pipes <joinfu@sun.com>
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

/**
 * @file
 *
 * Defines the INFORMATION_SCHEMA views exposing information about the
 * transaction log.
 */

#ifndef PLUGIN_TRANSACTION_LOG_INFO_SCHEMA_H
#define PLUGIN_TRANSACTION_LOG_INFO_SCHEMA_H

#include <drizzled/plugin/info_schema_table.h>

#include <vector>

class TransactionLogViewISMethods : public drizzled::plugin::InfoSchemaMethods
{
public:
  virtual int fillTable(Session *session,
                        TableList *tables);
};

class TransactionLogEntriesViewISMethods : public drizzled::plugin::InfoSchemaMethods
{
public:
  virtual int fillTable(Session *session,
                        TableList *tables);
};

class TransactionLogTransactionsViewISMethods : public drizzled::plugin::InfoSchemaMethods
{
public:
  virtual int fillTable(Session *session,
                        TableList *tables);
};

/**
 * Populate the vectors of columns for each I_S table.
 *
 * @return false on success; true on failure.
 */
bool initViewColumns();

/**
 * Clears the vectors of columns for each I_S table.
 */
void cleanupViewColumns();

/**
 * Initialize the methods for each I_S table.
 *
 * @return false on success; true on failure
 */
bool initViewMethods();

/**
 * Delete memory allocated for the I_S table methods.
 */
void cleanupViewMethods();

/**
 * Initialize the I_S tables related to the transaction log.
 *
 * @return false on success; true on failure
 */
bool initViews();

/**
 * Delete memory allocated for the I_S tables.
 */
void cleanupViews();
#endif /* PLUGIN_TRANSACTION_LOG_INFO_SCHEMA_H */
