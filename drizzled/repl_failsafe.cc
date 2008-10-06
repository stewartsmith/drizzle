/* Copyright (C) 2001-2006 MySQL AB & Sasha

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

/**
  @file

  All of the functions defined in this file which are not used (the ones to
  handle failsafe) are not used; their code has not been updated for more
  than one year now so should be considered as BADLY BROKEN. Do not enable
  it. The used functions (to handle LOAD DATA FROM MASTER, plus some small
  functions like register_slave()) are working.
*/
#include <drizzled/server_includes.h>

#ifdef HAVE_REPLICATION

#include "repl_failsafe.h"
#include "sql_repl.h"
#include "rpl_mi.h"
#include "rpl_filter.h"
#include "log_event.h"

#define SLAVE_LIST_CHUNK 128
#define SLAVE_ERRMSG_SIZE (FN_REFLEN+64)


RPL_STATUS rpl_status=RPL_NULL;
pthread_mutex_t LOCK_rpl_status;
pthread_cond_t COND_rpl_status;
HASH slave_list;

const char *rpl_role_type[] = {"MASTER","SLAVE",NULL};
TYPELIB rpl_role_typelib = {array_elements(rpl_role_type)-1,"",
			    rpl_role_type, NULL};

const char* rpl_status_type[]=
{
  "AUTH_MASTER","ACTIVE_SLAVE","IDLE_SLAVE", "LOST_SOLDIER","TROOP_SOLDIER",
  "RECOVERY_CAPTAIN","NULL",NULL
};
TYPELIB rpl_status_typelib= {array_elements(rpl_status_type)-1,"",
			     rpl_status_type, NULL};


void change_rpl_status(RPL_STATUS from_status, RPL_STATUS to_status)
{
  pthread_mutex_lock(&LOCK_rpl_status);
  if (rpl_status == from_status || rpl_status == RPL_ANY)
    rpl_status = to_status;
  pthread_cond_signal(&COND_rpl_status);
  pthread_mutex_unlock(&LOCK_rpl_status);
}


#define get_object(p, obj, msg) \
{\
  uint len = (uint)*p++;  \
  if (p + len > p_end || len >= sizeof(obj)) \
  {\
    errmsg= msg;\
    goto err; \
  }\
  strmake(obj,(char*) p,len); \
  p+= len; \
}\


static inline int cmp_master_pos(Slave_log_event* sev, LEX_MASTER_INFO* mi)
{
  return cmp_master_pos(sev->master_log, sev->master_pos, mi->log_file_name,
			mi->pos);
}


void unregister_slave(THD* thd, bool only_mine, bool need_mutex)
{
  if (thd->server_id)
  {
    if (need_mutex)
      pthread_mutex_lock(&LOCK_slave_list);

    SLAVE_INFO* old_si;
    if ((old_si = (SLAVE_INFO*)hash_search(&slave_list,
					   (uchar*)&thd->server_id, 4)) &&
	(!only_mine || old_si->thd == thd))
    hash_delete(&slave_list, (uchar*)old_si);

    if (need_mutex)
      pthread_mutex_unlock(&LOCK_slave_list);
  }
}


/**
  Register slave in 'slave_list' hash table.

  @return
    0	ok
  @return
    1	Error.   Error message sent to client
*/

int register_slave(THD* thd, uchar* packet, uint packet_length)
{
  int res;
  SLAVE_INFO *si;
  uchar *p= packet, *p_end= packet + packet_length;
  const char *errmsg= "Wrong parameters to function register_slave";

  if (!(si = (SLAVE_INFO*)my_malloc(sizeof(SLAVE_INFO), MYF(MY_WME))))
    goto err2;

  thd->server_id= si->server_id= uint4korr(p);
  p+= 4;
  get_object(p,si->host, "Failed to register slave: too long 'report-host'");
  get_object(p,si->user, "Failed to register slave: too long 'report-user'");
  get_object(p,si->password, "Failed to register slave; too long 'report-password'");
  if (p+10 > p_end)
    goto err;
  si->port= uint2korr(p);
  p += 2;
  si->rpl_recovery_rank= uint4korr(p);
  p += 4;
  if (!(si->master_id= uint4korr(p)))
    si->master_id= server_id;
  si->thd= thd;

  pthread_mutex_lock(&LOCK_slave_list);
  unregister_slave(thd,0,0);
  res= my_hash_insert(&slave_list, (uchar*) si);
  pthread_mutex_unlock(&LOCK_slave_list);
  return res;

err:
  free(si);
  my_message(ER_UNKNOWN_ERROR, errmsg, MYF(0)); /* purecov: inspected */
err2:
  return 1;
}

extern "C" uint32_t
*slave_list_key(SLAVE_INFO* si, size_t *len,
		bool not_used __attribute__((unused)))
{
  *len = 4;
  return &si->server_id;
}

extern "C" void slave_info_free(void *s)
{
  free(s);
}

void init_slave_list()
{
  hash_init(&slave_list, system_charset_info, SLAVE_LIST_CHUNK, 0, 0,
	    (hash_get_key) slave_list_key, (hash_free_key) slave_info_free, 0);
  pthread_mutex_init(&LOCK_slave_list, MY_MUTEX_INIT_FAST);
}

void end_slave_list()
{
  /* No protection by a mutex needed as we are only called at shutdown */
  if (hash_inited(&slave_list))
  {
    hash_free(&slave_list);
    pthread_mutex_destroy(&LOCK_slave_list);
  }
}


bool show_slave_hosts(THD* thd)
{
  List<Item> field_list;
  Protocol *protocol= thd->protocol;

  field_list.push_back(new Item_return_int("Server_id", 10,
					   DRIZZLE_TYPE_LONG));
  field_list.push_back(new Item_empty_string("Host", 20));
  if (opt_show_slave_auth_info)
  {
    field_list.push_back(new Item_empty_string("User",20));
    field_list.push_back(new Item_empty_string("Password",20));
  }
  field_list.push_back(new Item_return_int("Port", 7, DRIZZLE_TYPE_LONG));
  field_list.push_back(new Item_return_int("Rpl_recovery_rank", 7,
					   DRIZZLE_TYPE_LONG));
  field_list.push_back(new Item_return_int("Master_id", 10,
					   DRIZZLE_TYPE_LONG));

  if (protocol->send_fields(&field_list,
                            Protocol::SEND_NUM_ROWS | Protocol::SEND_EOF))
    return(true);

  pthread_mutex_lock(&LOCK_slave_list);

  for (uint i = 0; i < slave_list.records; ++i)
  {
    SLAVE_INFO* si = (SLAVE_INFO*) hash_element(&slave_list, i);
    protocol->prepare_for_resend();
    protocol->store((uint32_t) si->server_id);
    protocol->store(si->host, &my_charset_bin);
    if (opt_show_slave_auth_info)
    {
      protocol->store(si->user, &my_charset_bin);
      protocol->store(si->password, &my_charset_bin);
    }
    protocol->store((uint32_t) si->port);
    protocol->store((uint32_t) si->rpl_recovery_rank);
    protocol->store((uint32_t) si->master_id);
    if (protocol->write())
    {
      pthread_mutex_unlock(&LOCK_slave_list);
      return(true);
    }
  }
  pthread_mutex_unlock(&LOCK_slave_list);
  my_eof(thd);
  return(false);
}

#endif /* HAVE_REPLICATION */

