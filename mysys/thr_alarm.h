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

/* Prototypes when using thr_alarm library functions */

#ifndef _thr_alarm_h
#define _thr_alarm_h
#ifdef	__cplusplus
extern "C" {
#endif

#ifndef USE_ALARM_THREAD
#define USE_ONE_SIGNAL_HAND		/* One must call process_alarm */
#endif
#ifdef HAVE_rts_threads
#undef USE_ONE_SIGNAL_HAND
#define USE_ALARM_THREAD
#define THR_SERVER_ALARM SIGUSR1
#else
#define THR_SERVER_ALARM SIGALRM
#endif

typedef struct st_alarm_info
{
  uint32_t next_alarm_time;
  uint32_t active_alarms;
  uint32_t max_used_alarms;
} ALARM_INFO;

void thr_alarm_info(ALARM_INFO *info);

#if defined(DONT_USE_THR_ALARM)

#define USE_ALARM_THREAD
#undef USE_ONE_SIGNAL_HAND

typedef bool thr_alarm_t;
typedef bool ALARM;

#define thr_alarm_init(A) (*(A))=0
#define thr_alarm_in_use(A) (*(A) != 0)
#define thr_end_alarm(A)
#define thr_alarm(A,B,C) ((*(A)=1)-1)
/* The following should maybe be (*(A)) */
#define thr_got_alarm(A) 0
#define init_thr_alarm(A)
#define thr_alarm_kill(A)
#define resize_thr_alarm(N)
#define end_thr_alarm(A)

#else

typedef int thr_alarm_entry;
#define thr_got_alarm(thr_alarm) (**(thr_alarm))

typedef thr_alarm_entry* thr_alarm_t;

typedef struct st_alarm {
  uint32_t expire_time;
  thr_alarm_entry alarmed;		/* set when alarm is due */
  pthread_t thread;
  my_thread_id thread_id;
  bool malloced;
} ALARM;

extern uint32_t thr_client_alarm;
extern pthread_t alarm_thread;

#define thr_alarm_init(A) (*(A))=0
#define thr_alarm_in_use(A) (*(A)!= 0)
void init_thr_alarm(uint32_t max_alarm);
void resize_thr_alarm(uint32_t max_alarms);
bool thr_alarm(thr_alarm_t *alarmed, uint32_t sec, ALARM *buff);
void thr_alarm_kill(my_thread_id thread_id);
void thr_end_alarm(thr_alarm_t *alarmed);
void end_thr_alarm(bool free_structures);
RETSIGTYPE process_alarm(int);
#ifndef thr_got_alarm
bool thr_got_alarm(thr_alarm_t *alrm);
#endif


#endif /* DONT_USE_THR_ALARM */

#ifdef	__cplusplus
}
#endif /* __cplusplus */
#endif /* _thr_alarm_h */
