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

#include <drizzled/server_includes.h>
#include CSTDINT_H
#include <drizzled/functions/lock.h>
#include <mysys/hash.h>
#include <drizzled/session.h>

/*
** User level locks
*/

pthread_mutex_t LOCK_user_locks;
static HASH hash_user_locks;

class User_level_lock
{
  unsigned char *key;
  size_t key_length;

public:
  int count;
  bool locked;
  pthread_cond_t cond;
  my_thread_id thread_id;
  void set_thread(Session *session) { thread_id= session->thread_id; }

  User_level_lock(const unsigned char *key_arg,uint32_t length, ulong id)
    :key_length(length),count(1),locked(1), thread_id(id)
  {
    key= (unsigned char*) my_memdup(key_arg,length,MYF(0));
    pthread_cond_init(&cond,NULL);
    if (key)
    {
      if (my_hash_insert(&hash_user_locks,(unsigned char*) this))
      {
        free(key);
        key=0;
      }
    }
  }
  ~User_level_lock()
  {
    if (key)
    {
      hash_delete(&hash_user_locks,(unsigned char*) this);
      free(key);
    }
    pthread_cond_destroy(&cond);
  }
  inline bool initialized() { return key != 0; }

  friend void item_user_lock_release(User_level_lock *ull);
  friend unsigned char *ull_get_key(const User_level_lock *ull, size_t *length,
                            bool not_used);
};

unsigned char *ull_get_key(const User_level_lock *ull, size_t *length,
                   bool not_used __attribute__((unused)))
{
  *length= ull->key_length;
  return ull->key;
}


void item_user_lock_release(User_level_lock *ull)
{
  ull->locked=0;
  ull->thread_id= 0;
  if (--ull->count)
    pthread_cond_signal(&ull->cond);
  else
    delete ull;
}



/**
  Check a user level lock.

  Sets null_value=true on error.

  @retval
    1           Available
  @retval
    0           Already taken, or error
*/

int64_t Item_func_is_free_lock::val_int()
{
  assert(fixed == 1);
  String *res=args[0]->val_str(&value);
  User_level_lock *ull;

  null_value=0;
  if (!res || !res->length())
  {
    null_value=1;
    return 0;
  }

  pthread_mutex_lock(&LOCK_user_locks);
  ull= (User_level_lock *) hash_search(&hash_user_locks, (unsigned char*) res->ptr(),
                                       (size_t) res->length());
  pthread_mutex_unlock(&LOCK_user_locks);
  if (!ull || !ull->locked)
    return 1;
  return 0;
}

int64_t Item_func_is_used_lock::val_int()
{
  assert(fixed == 1);
  String *res=args[0]->val_str(&value);
  User_level_lock *ull;

  null_value=1;
  if (!res || !res->length())
    return 0;

  pthread_mutex_lock(&LOCK_user_locks);
  ull= (User_level_lock *) hash_search(&hash_user_locks, (unsigned char*) res->ptr(),
                                       (size_t) res->length());
  pthread_mutex_unlock(&LOCK_user_locks);
  if (!ull || !ull->locked)
    return 0;

  null_value=0;
  return ull->thread_id;
}


