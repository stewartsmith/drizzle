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
 * Defines the implementation of the background worker thread
 * which maintains summary information about transaction log.
 *
 * @details
 *
 * The general process that the background worker (collector) thread goes through
 * is the following:
 *
 * a) At thread start, we first check to see if there is an index
 *    file for the transaction log that has previously been written
 *    by the collector thread.  If there is, we read in the index and
 *    use that information to construct our in-memory index.
 *
 * b) If no index is around, we read the transaction log and create
 *    an in-memory index.  This in-memory index is sync'd to disk 
 *    every once in a while.  We don't care about crashes and the state
 *    of the index written to disk, since the transaction log file is
 *    the main stable state of the transaction log, not the index.
 *
 * c) Periodically, the collector thread queries the transaction
 *    log's current offset.  If this offset is greater than the last 
 *    offset in the collector's index, it reads a segment of the transaction
 *    log, building the index for the transactions it finds.
 *
 * d) When querying the INFORMATION_SCHEMA, or the replication system
 *    needs an exact read of the index, the collector thread is signalled
 *    via a pthread_cond_broadcast() call and the collector thread reads
 *    the transaction log up until the last log offset of the transaction log,
 *    building the index as it reads the log file.
 */

#include <config.h>
#include <drizzled/gettext.h>
#include <drizzled/errmsg_print.h>
#include <drizzled/definitions.h>
#include <errno.h>

#include "transaction_log.h"
#include "background_worker.h"

bool initTransactionLogBackgroundWorker()
{
  pthread_t thread;
  int error;
  if ((error= pthread_create(&thread, NULL, collectTransactionLogStats, 0)))
  {
    drizzled::sql_perror("Unable to create background worker thread.");
    return true;
  }
  return false;
}

void *collectTransactionLogStats(void *)
{
  /* Check to see if there is an index file on disk */

  /* No index file. Create one on disk and in memory. */

  /* Read in the index file. */

  /* Enter collection loop */

  /* Ask for the transaction log's latest written timestamp */

  /* Does our index have a smaller timestamp? */

  pthread_exit(0);

  return NULL;
}
