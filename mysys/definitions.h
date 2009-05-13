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

/**
 * @file
 *
 * Mostly constants and enums used by mysys
 */

#ifndef MYSYS_DEFINITIONS_H
#define MYSYS_DEFINITIONS_H

enum thr_lock_type { TL_IGNORE=-1,
                     /* UNLOCK ANY LOCK */
                     TL_UNLOCK,
                     /* Read lock */
                     TL_READ,
                     TL_READ_WITH_SHARED_LOCKS,
                     /* READ, Don't allow concurrent insert */
                     TL_READ_NO_INSERT,
                     /*
                       Write lock, but allow other threads to read / write.
                       Used by BDB tables in MySQL to mark that someone is
                       reading/writing to the table.
                     */
                     TL_WRITE_ALLOW_WRITE,
                     /*
                       Write lock, but allow other threads to read.
                       Used by ALTER TABLE in MySQL to allow readers
                       to use the table until ALTER TABLE is finished.
                     */
                     TL_WRITE_ALLOW_READ,
                     /*
                       WRITE lock used by concurrent insert. Will allow
                       READ, if one could use concurrent insert on table.
                     */
                     TL_WRITE_CONCURRENT_INSERT,
                     /*
                       parser only! Late bound low_priority flag.
                       At open_tables() becomes thd->update_lock_default.
                     */
                     TL_WRITE_DEFAULT,
                     /* Normal WRITE lock */
                     TL_WRITE,
                     /* Abort new lock request with an error */
                     TL_WRITE_ONLY};

enum enum_thr_lock_result { THR_LOCK_SUCCESS= 0, THR_LOCK_ABORTED= 1,
                            THR_LOCK_WAIT_TIMEOUT= 2, THR_LOCK_DEADLOCK= 3 };


#endif /* MYSYS_DEFINITIONS_H */
