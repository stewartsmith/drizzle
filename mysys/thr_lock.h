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

/* For use with thr_lock:s */

#ifndef _thr_lock_h
#define _thr_lock_h
#ifdef	__cplusplus
extern "C" {
#endif

#include <mysys/my_pthread.h>
#include <mysys/my_list.h>
#include <mysys/definitions.h>

struct st_thr_lock;
extern uint32_t locks_immediate,locks_waited ;
extern pthread_mutex_t THR_LOCK_lock;


extern uint64_t max_write_lock_count;
extern uint64_t table_lock_wait_timeout;
extern bool thr_lock_inited;
extern enum thr_lock_type thr_upgraded_concurrent_insert_lock;

/*
  A description of the thread which owns the lock. The address
  of an instance of this structure is used to uniquely identify the thread.
*/

typedef struct st_thr_lock_info
{
  pthread_t thread;
  my_thread_id thread_id;
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

/* A helper type for transactional locking. */
struct st_table_lock_info
{
  enum thr_lock_type lock_type;
  int           lock_timeout;
  bool          lock_transactional;
};

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
enum enum_thr_lock_result thr_lock(THR_LOCK_DATA *data,
                                   THR_LOCK_OWNER *owner,
                                   enum thr_lock_type lock_type);
void thr_unlock(THR_LOCK_DATA *data);
enum enum_thr_lock_result thr_multi_lock(THR_LOCK_DATA **data,
                                         uint32_t count, THR_LOCK_OWNER *owner);
void thr_multi_unlock(THR_LOCK_DATA **data,uint32_t count);
void thr_abort_locks(THR_LOCK *lock, bool upgrade_lock);
bool thr_abort_locks_for_thread(THR_LOCK *lock, my_thread_id thread);
bool thr_upgrade_write_delay_lock(THR_LOCK_DATA *data);
void    thr_downgrade_write_lock(THR_LOCK_DATA *data,
                                 enum thr_lock_type new_lock_type);
bool thr_reschedule_write_lock(THR_LOCK_DATA *data);
#ifdef	__cplusplus
}
#endif
#endif /* _thr_lock_h */
