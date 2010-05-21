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

/*
Read and write locks for Posix threads. All tread must acquire
all locks it needs through thr_multi_lock() to avoid dead-locks.
A lock consists of a master lock (THR_LOCK), and lock instances
(THR_LOCK_DATA).
Any thread can have any number of lock instances (read and write:s) on
any lock. All lock instances must be freed.
Locks are prioritized according to:

The current lock types are:

TL_READ	 		# Low priority read
TL_READ_WITH_SHARED_LOCKS
TL_READ_NO_INSERT	# Read without concurrent inserts
TL_WRITE_ALLOW_WRITE	# Write lock that allows other writers
TL_WRITE_ALLOW_READ	# Write lock, but allow reading
TL_WRITE_CONCURRENT_INSERT
			# Insert that can be mixed when selects
TL_WRITE		# High priority write
TL_WRITE_ONLY		# High priority write
			# Abort all new lock request with an error

Locks are prioritized according to:

WRITE_ALLOW_WRITE, WRITE_ALLOW_READ, WRITE_CONCURRENT_INSERT, WRITE_DELAYED,
WRITE_LOW_PRIORITY, READ, WRITE, READ_HIGH_PRIORITY and WRITE_ONLY

Locks in the same privilege level are scheduled in first-in-first-out order.

To allow concurrent read/writes locks, with 'WRITE_CONCURRENT_INSERT' one
should put a pointer to the following functions in the lock structure:
(If the pointer is zero (default), the function is not called)

check_status:
	 Before giving a lock of type TL_WRITE_CONCURRENT_INSERT,
         we check if this function exists and returns 0.
	 If not, then the lock is upgraded to TL_WRITE_LOCK
	 In MyISAM this is a simple check if the insert can be done
	 at the end of the datafile.
update_status:
	Before a write lock is released, this function is called.
	In MyISAM this functions updates the count and length of the datafile
get_status:
	When one gets a lock this functions is called.
	In MyISAM this stores the number of rows and size of the datafile
	for concurrent reads.

The lock algorithm allows one to have one TL_WRITE_ALLOW_READ,
TL_WRITE_CONCURRENT_INSERT lock at the same time as multiple read locks.

*/

#include "config.h"
#include "drizzled/internal/my_sys.h"
#include "drizzled/statistics_variables.h"

#include "thr_lock.h"
#include "drizzled/internal/m_string.h"
#include <errno.h>
#include <list>

#if TIME_WITH_SYS_TIME
# include <sys/time.h>
# include <time.h>
#else
# if HAVE_SYS_TIME_H
#  include <sys/time.h>
# else
#  include <time.h>
# endif
#endif

#include <drizzled/util/test.h>

using namespace std;

namespace drizzled
{

bool thr_lock_inited= false;
uint64_t table_lock_wait_timeout;
static enum thr_lock_type thr_upgraded_concurrent_insert_lock = TL_WRITE;


static list<THR_LOCK *> thr_lock_thread_list;          /* List of threads in use */

uint64_t max_write_lock_count= ~(uint64_t) 0L;

static inline pthread_cond_t *get_cond(void)
{
  return &my_thread_var->suspend;
}

/*
** For the future (now the thread specific cond is alloced by my_pthread.c)
*/

bool init_thr_lock()
{
  thr_lock_inited= true;

  return false;
}

static inline bool
thr_lock_owner_equal(THR_LOCK_OWNER *rhs, THR_LOCK_OWNER *lhs)
{
  return rhs == lhs;
}


	/* Initialize a lock */

void thr_lock_init(THR_LOCK *lock)
{
  memset(lock, 0, sizeof(*lock));
  pthread_mutex_init(&lock->mutex,MY_MUTEX_INIT_FAST);
  lock->read.last= &lock->read.data;
  lock->read_wait.last= &lock->read_wait.data;
  lock->write_wait.last= &lock->write_wait.data;
  lock->write.last= &lock->write.data;

  pthread_mutex_lock(&internal::THR_LOCK_lock);		/* Add to locks in use */
  thr_lock_thread_list.push_front(lock);
  pthread_mutex_unlock(&internal::THR_LOCK_lock);
}


void thr_lock_delete(THR_LOCK *lock)
{
  pthread_mutex_destroy(&lock->mutex);
  pthread_mutex_lock(&internal::THR_LOCK_lock);
  thr_lock_thread_list.remove(lock);
  pthread_mutex_unlock(&internal::THR_LOCK_lock);
}


void thr_lock_info_init(THR_LOCK_INFO *info)
{
  internal::st_my_thread_var *tmp= my_thread_var;
  info->thread= tmp->pthread_self;
  info->thread_id= tmp->id;
  info->n_cursors= 0;
}

	/* Initialize a lock instance */

void thr_lock_data_init(THR_LOCK *lock,THR_LOCK_DATA *data, void *param)
{
  data->lock= lock;
  data->type= TL_UNLOCK;
  data->owner= NULL;                               /* no owner yet */
  data->status_param= param;
  data->cond= NULL;
}


static inline bool
have_old_read_lock(THR_LOCK_DATA *data, THR_LOCK_OWNER *owner)
{
  for ( ; data ; data=data->next)
  {
    if (thr_lock_owner_equal(data->owner, owner))
      return true;					/* Already locked by thread */
  }
  return false;
}

static void wake_up_waiters(THR_LOCK *lock);


static enum enum_thr_lock_result
wait_for_lock(struct st_lock_list *wait, THR_LOCK_DATA *data,
              bool in_wait_list)
{
  internal::st_my_thread_var *thread_var= my_thread_var;
  pthread_cond_t *cond= &thread_var->suspend;
  struct timespec wait_timeout;
  enum enum_thr_lock_result result= THR_LOCK_ABORTED;
  bool can_deadlock= test(data->owner->info->n_cursors);

  if (!in_wait_list)
  {
    (*wait->last)=data;				/* Wait for lock */
    data->prev= wait->last;
    wait->last= &data->next;
  }

  status_var_increment(current_global_counters.locks_waited);

  /* Set up control struct to allow others to abort locks */
  thread_var->current_mutex= &data->lock->mutex;
  thread_var->current_cond=  cond;
  data->cond= cond;

  if (can_deadlock)
    set_timespec(wait_timeout, table_lock_wait_timeout);
  while (!thread_var->abort || in_wait_list)
  {
    int rc= (can_deadlock ?
             pthread_cond_timedwait(cond, &data->lock->mutex,
                                    &wait_timeout) :
             pthread_cond_wait(cond, &data->lock->mutex));
    /*
      We must break the wait if one of the following occurs:
      - the connection has been aborted (!thread_var->abort), but
        this is not a delayed insert thread (in_wait_list). For a delayed
        insert thread the proper action at shutdown is, apparently, to
        acquire the lock and complete the insert.
      - the lock has been granted (data->cond is set to NULL by the granter),
        or the waiting has been aborted (additionally data->type is set to
        TL_UNLOCK).
      - the wait has timed out (rc == ETIMEDOUT)
      Order of checks below is important to not report about timeout
      if the predicate is true.
    */
    if (data->cond == 0)
    {
      break;
    }
    if (rc == ETIMEDOUT || rc == ETIME)
    {
      result= THR_LOCK_WAIT_TIMEOUT;
      break;
    }
  }
  if (data->cond || data->type == TL_UNLOCK)
  {
    if (data->cond)                             /* aborted or timed out */
    {
      if (((*data->prev)=data->next))		/* remove from wait-list */
	data->next->prev= data->prev;
      else
	wait->last=data->prev;
      data->type= TL_UNLOCK;                    /* No lock */
      wake_up_waiters(data->lock);
    }
  }
  else
  {
    result= THR_LOCK_SUCCESS;
    if (data->lock->get_status)
      (*data->lock->get_status)(data->status_param, 0);
  }
  pthread_mutex_unlock(&data->lock->mutex);

  /* The following must be done after unlock of lock->mutex */
  pthread_mutex_lock(&thread_var->mutex);
  thread_var->current_mutex= NULL;
  thread_var->current_cond= NULL;
  pthread_mutex_unlock(&thread_var->mutex);
  return(result);
}


static enum enum_thr_lock_result
thr_lock(THR_LOCK_DATA *data, THR_LOCK_OWNER *owner,
         enum thr_lock_type lock_type)
{
  THR_LOCK *lock=data->lock;
  enum enum_thr_lock_result result= THR_LOCK_SUCCESS;
  struct st_lock_list *wait_queue;
  THR_LOCK_DATA *lock_owner;

  data->next=0;
  data->cond=0;					/* safety */
  data->type=lock_type;
  data->owner= owner;                           /* Must be reset ! */
  pthread_mutex_lock(&lock->mutex);
  if ((int) lock_type <= (int) TL_READ_NO_INSERT)
  {
    /* Request for READ lock */
    if (lock->write.data)
    {
      /* We can allow a read lock even if there is already a write lock
	 on the table in one the following cases:
	 - This thread alread have a write lock on the table
	 - The write lock is TL_WRITE_ALLOW_READ or TL_WRITE_DELAYED
           and the read lock is TL_READ_HIGH_PRIORITY or TL_READ
         - The write lock is TL_WRITE_CONCURRENT_INSERT or TL_WRITE_ALLOW_WRITE
	   and the read lock is not TL_READ_NO_INSERT
      */

      if (thr_lock_owner_equal(data->owner, lock->write.data->owner) ||
	  (lock->write.data->type <= TL_WRITE_CONCURRENT_INSERT &&
	   (((int) lock_type <= (int) TL_READ_WITH_SHARED_LOCKS) ||
	    (lock->write.data->type != TL_WRITE_CONCURRENT_INSERT &&
	     lock->write.data->type != TL_WRITE_ALLOW_READ))))
      {						/* Already got a write lock */
	(*lock->read.last)=data;		/* Add to running FIFO */
	data->prev=lock->read.last;
	lock->read.last= &data->next;
	if (lock_type == TL_READ_NO_INSERT)
	  lock->read_no_write_count++;
	if (lock->get_status)
	  (*lock->get_status)(data->status_param, 0);
        status_var_increment(current_global_counters.locks_immediate);
	goto end;
      }
      if (lock->write.data->type == TL_WRITE_ONLY)
      {
	/* We are not allowed to get a READ lock in this case */
	data->type=TL_UNLOCK;
        result= THR_LOCK_ABORTED;               /* Can't wait for this one */
	goto end;
      }
    }
    else if (!lock->write_wait.data ||
	     lock->write_wait.data->type <= TL_WRITE_DEFAULT ||
	     have_old_read_lock(lock->read.data, data->owner))
    {						/* No important write-locks */
      (*lock->read.last)=data;			/* Add to running FIFO */
      data->prev=lock->read.last;
      lock->read.last= &data->next;
      if (lock->get_status)
	(*lock->get_status)(data->status_param, 0);
      if (lock_type == TL_READ_NO_INSERT)
	lock->read_no_write_count++;
      status_var_increment(current_global_counters.locks_immediate);
      goto end;
    }
    /*
      We're here if there is an active write lock or no write
      lock but a high priority write waiting in the write_wait queue.
      In the latter case we should yield the lock to the writer.
    */
    wait_queue= &lock->read_wait;
  }
  else						/* Request for WRITE lock */
  {
    if (lock_type == TL_WRITE_CONCURRENT_INSERT && ! lock->check_status)
      data->type=lock_type= thr_upgraded_concurrent_insert_lock;

    if (lock->write.data)			/* If there is a write lock */
    {
      if (lock->write.data->type == TL_WRITE_ONLY)
      {
        /* Allow lock owner to bypass TL_WRITE_ONLY. */
        if (!thr_lock_owner_equal(data->owner, lock->write.data->owner))
        {
          /* We are not allowed to get a lock in this case */
          data->type=TL_UNLOCK;
          result= THR_LOCK_ABORTED;               /* Can't wait for this one */
          goto end;
        }
      }

      /*
	The following test will not work if the old lock was a
	TL_WRITE_ALLOW_WRITE, TL_WRITE_ALLOW_READ or TL_WRITE_DELAYED in
	the same thread, but this will never happen within MySQL.
      */
      if (thr_lock_owner_equal(data->owner, lock->write.data->owner) ||
	  (lock_type == TL_WRITE_ALLOW_WRITE &&
	   !lock->write_wait.data &&
	   lock->write.data->type == TL_WRITE_ALLOW_WRITE))
      {
	/*
          We have already got a write lock or all locks are
          TL_WRITE_ALLOW_WRITE
        */

	(*lock->write.last)=data;	/* Add to running fifo */
	data->prev=lock->write.last;
	lock->write.last= &data->next;
	if (data->lock->get_status)
	  (*data->lock->get_status)(data->status_param, 0);
        status_var_increment(current_global_counters.locks_immediate);
	goto end;
      }
    }
    else
    {
      if (!lock->write_wait.data)
      {						/* no scheduled write locks */
        bool concurrent_insert= 0;
	if (lock_type == TL_WRITE_CONCURRENT_INSERT)
        {
          concurrent_insert= 1;
          if ((*lock->check_status)(data->status_param))
          {
            concurrent_insert= 0;
            data->type=lock_type= thr_upgraded_concurrent_insert_lock;
          }
        }

	if (!lock->read.data ||
	    (lock_type <= TL_WRITE_CONCURRENT_INSERT &&
	     ((lock_type != TL_WRITE_CONCURRENT_INSERT &&
	       lock_type != TL_WRITE_ALLOW_WRITE) ||
	      !lock->read_no_write_count)))
	{
	  (*lock->write.last)=data;		/* Add as current write lock */
	  data->prev=lock->write.last;
	  lock->write.last= &data->next;
	  if (data->lock->get_status)
	    (*data->lock->get_status)(data->status_param, concurrent_insert);
          status_var_increment(current_global_counters.locks_immediate);
	  goto end;
	}
      }
    }
    wait_queue= &lock->write_wait;
  }
  /*
    Try to detect a trivial deadlock when using cursors: attempt to
    lock a table that is already locked by an open cursor within the
    same connection. lock_owner can be zero if we succumbed to a high
    priority writer in the write_wait queue.
  */
  lock_owner= lock->read.data ? lock->read.data : lock->write.data;
  if (lock_owner && lock_owner->owner->info == owner->info)
  {
    result= THR_LOCK_DEADLOCK;
    goto end;
  }
  /* Can't get lock yet;  Wait for it */
  return(wait_for_lock(wait_queue, data, 0));
end:
  pthread_mutex_unlock(&lock->mutex);
  return(result);
}


static void free_all_read_locks(THR_LOCK *lock, bool using_concurrent_insert)
{
  THR_LOCK_DATA *data=lock->read_wait.data;

  /* move all locks from read_wait list to read list */
  (*lock->read.last)=data;
  data->prev=lock->read.last;
  lock->read.last=lock->read_wait.last;

  /* Clear read_wait list */
  lock->read_wait.last= &lock->read_wait.data;

  do
  {
    pthread_cond_t *cond=data->cond;
    if ((int) data->type == (int) TL_READ_NO_INSERT)
    {
      if (using_concurrent_insert)
      {
	/*
	  We can't free this lock;
	  Link lock away from read chain back into read_wait chain
	*/
	if (((*data->prev)=data->next))
	  data->next->prev=data->prev;
	else
	  lock->read.last=data->prev;
	*lock->read_wait.last= data;
	data->prev= lock->read_wait.last;
	lock->read_wait.last= &data->next;
	continue;
      }
      lock->read_no_write_count++;
    }
    data->cond=0;				/* Mark thread free */
    pthread_cond_signal(cond);
  } while ((data=data->next));
  *lock->read_wait.last=0;
  if (!lock->read_wait.data)
    lock->write_lock_count=0;
}

	/* Unlock lock and free next thread on same lock */

static void thr_unlock(THR_LOCK_DATA *data)
{
  THR_LOCK *lock=data->lock;
  enum thr_lock_type lock_type=data->type;
  pthread_mutex_lock(&lock->mutex);

  if (((*data->prev)=data->next))		/* remove from lock-list */
    data->next->prev= data->prev;
  else if (lock_type <= TL_READ_NO_INSERT)
    lock->read.last=data->prev;
  else
    lock->write.last=data->prev;
  if (lock_type >= TL_WRITE_CONCURRENT_INSERT)
  {
    if (lock->update_status)
      (*lock->update_status)(data->status_param);
  }
  else
  {
    if (lock->restore_status)
      (*lock->restore_status)(data->status_param);
  }
  if (lock_type == TL_READ_NO_INSERT)
    lock->read_no_write_count--;
  data->type=TL_UNLOCK;				/* Mark unlocked */
  wake_up_waiters(lock);
  pthread_mutex_unlock(&lock->mutex);
  return;
}


/**
  @brief  Wake up all threads which pending requests for the lock
          can be satisfied.

  @param  lock  Lock for which threads should be woken up

*/

static void wake_up_waiters(THR_LOCK *lock)
{
  THR_LOCK_DATA *data;
  enum thr_lock_type lock_type;

  if (!lock->write.data)			/* If no active write locks */
  {
    data=lock->write_wait.data;
    if (!lock->read.data)			/* If no more locks in use */
    {
      /* Release write-locks with TL_WRITE or TL_WRITE_ONLY priority first */
      if (data &&
	  (!lock->read_wait.data || lock->read_wait.data->type <= TL_READ_WITH_SHARED_LOCKS))
      {
	if (lock->write_lock_count++ > max_write_lock_count)
	{
	  /* Too many write locks in a row;  Release all waiting read locks */
	  lock->write_lock_count=0;
	  if (lock->read_wait.data)
	  {
	    free_all_read_locks(lock,0);
	    goto end;
	  }
	}
	for (;;)
	{
	  if (((*data->prev)=data->next))	/* remove from wait-list */
	    data->next->prev= data->prev;
	  else
	    lock->write_wait.last=data->prev;
	  (*lock->write.last)=data;		/* Put in execute list */
	  data->prev=lock->write.last;
	  data->next=0;
	  lock->write.last= &data->next;
	  if (data->type == TL_WRITE_CONCURRENT_INSERT &&
	      (*lock->check_status)(data->status_param))
	    data->type=TL_WRITE;			/* Upgrade lock */
	  {
	    pthread_cond_t *cond=data->cond;
	    data->cond=0;				/* Mark thread free */
	    pthread_cond_signal(cond);	/* Start waiting thread */
	  }
	  if (data->type != TL_WRITE_ALLOW_WRITE ||
	      !lock->write_wait.data ||
	      lock->write_wait.data->type != TL_WRITE_ALLOW_WRITE)
	    break;
	  data=lock->write_wait.data;		/* Free this too */
	}
	if (data->type >= TL_WRITE)
          goto end;
	/* Release possible read locks together with the write lock */
      }
      if (lock->read_wait.data)
	free_all_read_locks(lock,
			    data &&
			    (data->type == TL_WRITE_CONCURRENT_INSERT ||
			     data->type == TL_WRITE_ALLOW_WRITE));
    }
    else if (data &&
	     (lock_type=data->type) <= TL_WRITE_CONCURRENT_INSERT &&
	     ((lock_type != TL_WRITE_CONCURRENT_INSERT &&
	       lock_type != TL_WRITE_ALLOW_WRITE) ||
	      !lock->read_no_write_count))
    {
      /*
	For DELAYED, ALLOW_READ, WRITE_ALLOW_WRITE or CONCURRENT_INSERT locks
	start WRITE locks together with the READ locks
      */
      if (lock_type == TL_WRITE_CONCURRENT_INSERT &&
	  (*lock->check_status)(data->status_param))
      {
	data->type=TL_WRITE;			/* Upgrade lock */
	if (lock->read_wait.data)
	  free_all_read_locks(lock,0);
	goto end;
      }
      do {
	pthread_cond_t *cond=data->cond;
	if (((*data->prev)=data->next))		/* remove from wait-list */
	  data->next->prev= data->prev;
	else
	  lock->write_wait.last=data->prev;
	(*lock->write.last)=data;		/* Put in execute list */
	data->prev=lock->write.last;
	lock->write.last= &data->next;
	data->next=0;				/* Only one write lock */
	data->cond=0;				/* Mark thread free */
	pthread_cond_signal(cond);	/* Start waiting thread */
      } while (lock_type == TL_WRITE_ALLOW_WRITE &&
	       (data=lock->write_wait.data) &&
	       data->type == TL_WRITE_ALLOW_WRITE);
      if (lock->read_wait.data)
	free_all_read_locks(lock,
			    (lock_type == TL_WRITE_CONCURRENT_INSERT ||
			     lock_type == TL_WRITE_ALLOW_WRITE));
    }
    else if (!data && lock->read_wait.data)
      free_all_read_locks(lock,0);
  }
end:
  return;
}


/*
** Get all locks in a specific order to avoid dead-locks
** Sort acording to lock position and put write_locks before read_locks if
** lock on same lock.
*/


#define LOCK_CMP(A,B) ((unsigned char*) (A->lock) - (uint32_t) ((A)->type) < (unsigned char*) (B->lock)- (uint32_t) ((B)->type))

static void sort_locks(THR_LOCK_DATA **data,uint32_t count)
{
  THR_LOCK_DATA **pos,**end,**prev,*tmp;

  /* Sort locks with insertion sort (fast because almost always few locks) */

  for (pos=data+1,end=data+count; pos < end ; pos++)
  {
    tmp= *pos;
    if (LOCK_CMP(tmp,pos[-1]))
    {
      prev=pos;
      do {
	prev[0]=prev[-1];
      } while (--prev != data && LOCK_CMP(tmp,prev[-1]));
      prev[0]=tmp;
    }
  }
}


enum enum_thr_lock_result
thr_multi_lock(THR_LOCK_DATA **data, uint32_t count, THR_LOCK_OWNER *owner)
{
  THR_LOCK_DATA **pos,**end;
  if (count > 1)
    sort_locks(data,count);
  /* lock everything */
  for (pos=data,end=data+count; pos < end ; pos++)
  {
    enum enum_thr_lock_result result= thr_lock(*pos, owner, (*pos)->type);
    if (result != THR_LOCK_SUCCESS)
    {						/* Aborted */
      thr_multi_unlock(data,(uint32_t) (pos-data));
      return(result);
    }
  }
  /*
    Ensure that all get_locks() have the same status
    If we lock the same table multiple times, we must use the same
    status_param!
  */
#if !defined(DONT_USE_RW_LOCKS)
  if (count > 1)
  {
    THR_LOCK_DATA *last_lock= end[-1];
    pos=end-1;
    do
    {
      pos--;
      if (last_lock->lock == (*pos)->lock &&
	  last_lock->lock->copy_status)
      {
	if (last_lock->type <= TL_READ_NO_INSERT)
	{
	  THR_LOCK_DATA **read_lock;
	  /*
	    If we are locking the same table with read locks we must ensure
	    that all tables share the status of the last write lock or
	    the same read lock.
	  */
	  for (;
	       (*pos)->type <= TL_READ_NO_INSERT &&
		 pos != data &&
		 pos[-1]->lock == (*pos)->lock ;
	       pos--) ;

	  read_lock = pos+1;
	  do
	  {
	    (last_lock->lock->copy_status)((*read_lock)->status_param,
					   (*pos)->status_param);
	  } while (*(read_lock++) != last_lock);
	  last_lock= (*pos);			/* Point at last write lock */
	}
	else
	  (*last_lock->lock->copy_status)((*pos)->status_param,
					  last_lock->status_param);
      }
      else
	last_lock=(*pos);
    } while (pos != data);
  }
#endif
  return(THR_LOCK_SUCCESS);
}

  /* free all locks */

void thr_multi_unlock(THR_LOCK_DATA **data,uint32_t count)
{
  THR_LOCK_DATA **pos,**end;

  for (pos=data,end=data+count; pos < end ; pos++)
  {
    if ((*pos)->type != TL_UNLOCK)
      thr_unlock(*pos);
  }
  return;
}

/*
  Abort all threads waiting for a lock. The lock will be upgraded to
  TL_WRITE_ONLY to abort any new accesses to the lock
*/

void thr_abort_locks(THR_LOCK *lock)
{
  THR_LOCK_DATA *data;
  pthread_mutex_lock(&lock->mutex);

  for (data=lock->read_wait.data; data ; data=data->next)
  {
    data->type= TL_UNLOCK;			/* Mark killed */
    /* It's safe to signal the cond first: we're still holding the mutex. */
    pthread_cond_signal(data->cond);
    data->cond= NULL;				/* Removed from list */
  }
  for (data=lock->write_wait.data; data ; data=data->next)
  {
    data->type=TL_UNLOCK;
    pthread_cond_signal(data->cond);
    data->cond= NULL;
  }
  lock->read_wait.last= &lock->read_wait.data;
  lock->write_wait.last= &lock->write_wait.data;
  lock->read_wait.data=lock->write_wait.data=0;
  if (lock->write.data)
    lock->write.data->type=TL_WRITE_ONLY;
  pthread_mutex_unlock(&lock->mutex);
  return;
}


/*
  Abort all locks for specific table/thread combination

  This is used to abort all locks for a specific thread
*/

bool thr_abort_locks_for_thread(THR_LOCK *lock, uint64_t thread_id)
{
  THR_LOCK_DATA *data;
  bool found= false;

  pthread_mutex_lock(&lock->mutex);
  for (data= lock->read_wait.data; data ; data= data->next)
  {
    if (data->owner->info->thread_id == thread_id)
    {
      data->type= TL_UNLOCK;			/* Mark killed */
      /* It's safe to signal the cond first: we're still holding the mutex. */
      found= true;
      pthread_cond_signal(data->cond);
      data->cond= 0;				/* Removed from list */

      if (((*data->prev)= data->next))
	data->next->prev= data->prev;
      else
	lock->read_wait.last= data->prev;
    }
  }
  for (data= lock->write_wait.data; data ; data= data->next)
  {
    if (data->owner->info->thread_id == thread_id)
    {
      data->type= TL_UNLOCK;
      found= true;
      pthread_cond_signal(data->cond);
      data->cond= NULL;

      if (((*data->prev)= data->next))
	data->next->prev= data->prev;
      else
	lock->write_wait.last= data->prev;
    }
  }
  wake_up_waiters(lock);
  pthread_mutex_unlock(&lock->mutex);
  return(found);
}

} /* namespace drizzled */
