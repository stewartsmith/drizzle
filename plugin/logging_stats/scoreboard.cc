/*
 * Copyright (c) 2010, Joseph Daly <skinny.moey@gmail.com>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 *   * Redistributions of source code must retain the above copyright notice,
 *     this list of conditions and the following disclaimer.
 *   * Redistributions in binary form must reproduce the above copyright notice,
 *     this list of conditions and the following disclaimer in the documentation
 *     and/or other materials provided with the distribution.
 *   * Neither the name of Joseph Daly nor the names of its contributors
 *     may be used to endorse or promote products derived from this software
 *     without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 *
 */

#include "config.h"
#include "scoreboard.h"

#include <math.h>

using namespace drizzled;
using namespace std;

Scoreboard::Scoreboard(uint32_t in_number_sessions, uint32_t in_number_buckets)
  :
    number_sessions(in_number_sessions),
    number_buckets(in_number_buckets)
{

  /* calculate the number of elements in each bucket */
  uint32_t number_per_bucket= static_cast<uint32_t> ( ceil( static_cast<double>(number_sessions) / static_cast<double>(number_buckets) ) );

  /* populate the vector of scoreboard vectors */
  for (uint32_t j= 0; j < number_buckets; j++)
  {
    vector<ScoreboardSlot* > *scoreboard_vector= new vector<ScoreboardSlot* >();

    /* preallocate the individual vectors */
    vector<ScoreboardSlot* >::iterator scoreboard_vector_iterator= scoreboard_vector->begin();
    for (uint32_t h= 0; h < number_per_bucket; h++)
    {
      ScoreboardSlot *scoreboard_slot= new ScoreboardSlot();
      scoreboard_vector_iterator= scoreboard_vector->insert(scoreboard_vector_iterator, scoreboard_slot);
    }  
    scoreboard_vector->resize(number_per_bucket);


    /* insert the vector into the vector of scoreboard vectors */
    vector<vector<ScoreboardSlot* >* >::iterator vector_of_scoreboard_vectors_iterator= 
      vector_of_scoreboard_vectors.begin();

    vector_of_scoreboard_vectors_iterator= 
      vector_of_scoreboard_vectors.insert(vector_of_scoreboard_vectors_iterator, scoreboard_vector); 
  }
  vector_of_scoreboard_vectors.resize(number_buckets);
  
  /* populate the scoreboard locks vector each ScoreboardSlot vector gets a lock */
  vector<pthread_rwlock_t* >::iterator vector_of_scoreboard_locks_iterator= vector_of_scoreboard_locks.begin();
  for (uint32_t k= 0; k < number_buckets; k++)
  {
    pthread_rwlock_t* lock= new pthread_rwlock_t();
    vector_of_scoreboard_locks_iterator= 
      vector_of_scoreboard_locks.insert(vector_of_scoreboard_locks_iterator, lock);   
  } 
  vector_of_scoreboard_locks.resize(number_buckets);
}

Scoreboard::~Scoreboard()
{
  vector<vector<ScoreboardSlot* >* >::iterator v_of_scoreboard_v_begin_it= vector_of_scoreboard_vectors.begin();
  vector<vector<ScoreboardSlot* >* >::iterator v_of_scoreboard_v_end_it= vector_of_scoreboard_vectors.end();

  for (; v_of_scoreboard_v_begin_it != v_of_scoreboard_v_end_it; ++v_of_scoreboard_v_begin_it)
  {
    vector<ScoreboardSlot* > *scoreboard_vector= *v_of_scoreboard_v_begin_it; 

    vector<ScoreboardSlot* >::iterator scoreboard_vector_it= scoreboard_vector->begin();
    vector<ScoreboardSlot* >::iterator scoreboard_vector_end= scoreboard_vector->end();
    for (; scoreboard_vector_it != scoreboard_vector_end; ++scoreboard_vector_it)
    {
      delete *scoreboard_vector_it; 
    }
    
    scoreboard_vector->clear();
    delete scoreboard_vector;
  } // vector_of_scoreboard_vectors is not on the stack and does not deletion
  
  vector<pthread_rwlock_t* >::iterator vector_of_scoreboard_locks_it= vector_of_scoreboard_locks.begin();
  vector<pthread_rwlock_t* >::iterator vector_of_scoreboard_locks_end= vector_of_scoreboard_locks.end();

  for (; vector_of_scoreboard_locks_it != vector_of_scoreboard_locks_end; ++vector_of_scoreboard_locks_it)
  {
    delete *vector_of_scoreboard_locks_it;
  }
}

ScoreboardSlot* Scoreboard::findScoreboardSlotToLog(Session *session)
{
  /* our bucket */
  uint32_t bucket_number= session->getSessionId() % number_buckets; 

  /* our vector corresponding to bucket_number */
  vector<ScoreboardSlot* > *scoreboard_vector= vector_of_scoreboard_vectors.at(bucket_number); 

  /* out lock corresponding to bucket_number */
  pthread_rwlock_t *LOCK_scoreboard_vector= vector_of_scoreboard_locks.at(bucket_number);

  pthread_rwlock_wrlock(LOCK_scoreboard_vector);
  ScoreboardSlot *scoreboard_slot= NULL;
  int32_t our_slot= UNINITIALIZED;
  int32_t open_slot= UNINITIALIZED;

  uint32_t current_slot= 0;
  for (vector<ScoreboardSlot *>::iterator it= scoreboard_vector->begin();
       it != scoreboard_vector->end(); ++it, current_slot++)
  {
    scoreboard_slot= *it;

    if (scoreboard_slot->isInUse() == true)
    {
      /* Check if this session is the one using this slot */
      if (scoreboard_slot->getSessionId() == session->getSessionId())
      {
        our_slot= current_slot;
        break;
      }
      else
      {
        continue;
      }
    }
    else
    {
      /* save off the open slot */
      if (open_slot == UNINITIALIZED)
      {
        open_slot= current_slot;
      }
      continue;
    }
  }

  if (our_slot != UNINITIALIZED)
  {
    pthread_rwlock_unlock(LOCK_scoreboard_vector);
  }
  else if (open_slot != UNINITIALIZED)
  {
    scoreboard_slot= scoreboard_vector->at(open_slot);
    scoreboard_slot->setInUse(true);
    scoreboard_slot->setSessionId(session->getSessionId());
    scoreboard_slot->setUser(session->getSecurityContext().getUser());
    scoreboard_slot->setIp(session->getSecurityContext().getIp());
    pthread_rwlock_unlock(LOCK_scoreboard_vector);
  }
  else
  {
    pthread_rwlock_unlock(LOCK_scoreboard_vector);
    /* there was no available slot for this session */
    return NULL;
  }

  return scoreboard_slot;
}

ScoreboardSlot* Scoreboard::findAndResetScoreboardSlot(Session *session)
{
  /* our bucket */
  uint32_t bucket_number= session->getSessionId() % number_buckets;

  /* our vector corresponding to bucket_number */
  vector<ScoreboardSlot* > *scoreboard_vector= vector_of_scoreboard_vectors.at(bucket_number);

  /* out lock corresponding to bucket_number */
  pthread_rwlock_t *LOCK_scoreboard_vector= vector_of_scoreboard_locks.at(bucket_number);

  pthread_rwlock_wrlock(LOCK_scoreboard_vector);

  ScoreboardSlot *scoreboard_slot;
  ScoreboardSlot *return_scoreboard_slot= NULL;

  for (vector<ScoreboardSlot *>::iterator it= scoreboard_vector->begin();
       it != scoreboard_vector->end(); ++it)
  {
    scoreboard_slot= *it;

    if (scoreboard_slot->getSessionId() == session->getSessionId())
    {
      return_scoreboard_slot = new ScoreboardSlot(*scoreboard_slot);
      scoreboard_slot->reset(); 
      break;
    }
  }
  pthread_rwlock_unlock(LOCK_scoreboard_vector);   

  return return_scoreboard_slot;
}
