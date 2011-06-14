/* Copyright (C) 2000 MySQL AB

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA */

/* For use with thr_locks */

#pragma once

#include <boost/thread/mutex.hpp>
#include <boost/thread/shared_mutex.hpp>
#include <boost/thread/condition_variable.hpp>

#include <drizzled/visibility.h>

namespace drizzled
{

extern uint64_t max_write_lock_count;
extern uint64_t table_lock_wait_timeout;


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
/*
  A description of the thread which owns the lock. The address
  of an instance of this structure is used to uniquely identify the thread.
*/

struct THR_LOCK_INFO
{
  uint64_t thread_id;
  uint32_t n_cursors;

  THR_LOCK_INFO() : 
    thread_id(0),
    n_cursors(0)
  { }

  void init();

};

/*
  Lock owner identifier. Globally identifies the lock owner within the
  thread and among all the threads. The address of an instance of this
  structure is used as id.
*/

struct THR_LOCK_OWNER
{
  THR_LOCK_INFO *info;

  THR_LOCK_OWNER() :
    info(0)
  { }

};

struct THR_LOCK;
struct THR_LOCK_DATA;

struct DRIZZLED_API THR_LOCK_DATA {
  THR_LOCK_OWNER *owner;
  struct THR_LOCK_DATA *next,**prev;
  struct THR_LOCK *lock;
  boost::condition_variable_any *cond;
  enum thr_lock_type type;
  void *status_param;			/* Param to status functions */

  THR_LOCK_DATA() :
    owner(0),
    next(0),
    prev(0),
    lock(0),
    cond(0),
    type(TL_UNLOCK),
    status_param(0)
  { }

  void init(THR_LOCK *lock,
            void *status_param= NULL);
};

struct st_lock_list {
  THR_LOCK_DATA *data,**last;

  st_lock_list() :
    data(0),
    last(0)
  { }
};

struct THR_LOCK {
private:
  boost::mutex mutex;
public:
  struct st_lock_list read_wait;
  struct st_lock_list read;
  struct st_lock_list write_wait;
  struct st_lock_list write;
  /* write_lock_count is incremented for write locks and reset on read locks */
  uint32_t write_lock_count;
  uint32_t read_no_write_count;

  THR_LOCK() :
    write_lock_count(0),
    read_no_write_count(0)
  { }

  ~THR_LOCK()
  { }

  void abort_locks();
  bool abort_locks_for_thread(uint64_t thread);

  void lock()
  {
    mutex.lock();
  }

  void unlock()
  {
    mutex.unlock();
  }

  void init()
  {
  }

  void deinit()
  {
  }

  boost::mutex *native_handle()
  {
    return &mutex;
  }
};

#define thr_lock_owner_init(owner, info_arg) (owner)->info= (info_arg)
DRIZZLED_API void thr_lock_init(THR_LOCK *lock);
enum enum_thr_lock_result thr_multi_lock(Session &session, THR_LOCK_DATA **data,
                                         uint32_t count, THR_LOCK_OWNER *owner);
void thr_multi_unlock(THR_LOCK_DATA **data,uint32_t count);

} /* namespace drizzled */

