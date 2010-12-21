/* - mode: c; c-basic-offset: 2; indent-tabs-mode: nil; -*-
 *  vim:expandtab:shiftwidth=2:tabstop=2:smarttab:
 *
 *  Copyright (C) 2009 Sun Microsystems, Inc.
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
 * Defines the methods used by the background worker thread which
 * maintains summary information about the transaction log.
 */

#ifndef PLUGIN_TRANSACTION_LOG_BACKGROUND_WORKER_H
#define PLUGIN_TRANSACTION_LOG_BACKGROUND_WORKER_H

#include <pthread.h>

/**
 * Initializes the background worker thread
 *
 * @return false on success; true on failure
 */
bool initTransactionLogBackgroundWorker();

/**
 * The routine which the background worker executes
 */
extern "C" {
  void *collectTransactionLogStats(void *arg);
}

#endif /* PLUGIN_TRANSACTION_LOG_BACKGROUND_WORKER_H */
