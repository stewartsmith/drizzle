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
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA */

/* For use with thr_locks */

#ifndef DRIZZLED_THR_LOCK_H
#define DRIZZLED_THR_LOCK_H

#include <pthread.h>

namespace drizzled
{

struct st_thr_lock;

namespace internal
{
extern pthread_mutex_t THR_LOCK_lock;
}

extern uint64_t max_write_lock_count;
extern uint64_t table_lock_wait_timeout;
extern bool thr_lock_inited;


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

typedef struct st_thr_lock_info
{
  pthread_t thread;
  uint64_t thread_id;
  uint32_t n_cursors;
} THR_LOCK_INFO;

/*
  Lock owner identifier. Globally identifies the lock owner within the
  thread and among all the threads. The address of an instance of this
  structure is used as id.
*/

typedef struct st_thr_lock_owner
{
  THR_LOCK_INFO *info;
} THR_LOCK_OWNER;


typedef struct st_thr_lock_data {
  THR_LOCK_OWNER *owner;
  struct st_thr_lock_data *next,**prev;
  struct st_thr_lock *lock;
  pthread_cond_t *cond;
  enum thr_lock_type type;
  void *status_param;			/* Param to status functions */
} THR_LOCK_DATA;

struct st_lock_list {
  THR_LOCK_DATA *data,**last;
};

typedef struct st_thr_lock {
  pthread_mutex_t mutex;
  struct st_lock_list read_wait;
  struct st_lock_list read;
  struct st_lock_list write_wait;
  struct st_lock_list write;
  /* write_lock_count is incremented for write locks and reset on read locks */
  uint32_t write_lock_count;
  uint32_t read_no_write_count;
  void (*get_status)(void*, int);	/* When one gets a lock */
  void (*copy_status)(void*,void*);
  void (*update_status)(void*);		/* Before release of write */
  void (*restore_status)(void*);         /* Before release of read */
  bool (*check_status)(void *);
} THR_LOCK;


bool init_thr_lock(void);		/* Must be called once/thread */
#define thr_lock_owner_init(owner, info_arg) (owner)->info= (info_arg)
void thr_lock_info_init(THR_LOCK_INFO *info);
void thr_lock_init(THR_LOCK *lock);
void thr_lock_delete(THR_LOCK *lock);
void thr_lock_data_init(THR_LOCK *lock,THR_LOCK_DATA *data,
			void *status_param);
enum enum_thr_lock_result thr_multi_lock(THR_LOCK_DATA **data,
                                         uint32_t count, THR_LOCK_OWNER *owner);
void thr_multi_unlock(THR_LOCK_DATA **data,uint32_t count);
void thr_abort_locks(THR_LOCK *lock);
bool thr_abort_locks_for_thread(THR_LOCK *lock, uint64_t thread);

} /* namespace drizzled */

#endif /* DRIZZLED_THR_LOCK_H */
