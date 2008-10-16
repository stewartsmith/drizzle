/* 
 -*- mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; -*-
 *  vim:expandtab:shiftwidth=2:tabstop=2:smarttab:

 *  Copyright (C) 2008 Mark Atwood
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

/* need to define DRIZZLE_SERVER to get inside the THD */
#define DRIZZLE_SERVER 1
#include <drizzled/server_includes.h>
#include <drizzled/plugin_logging.h>

/* todo, make this dynamic as needed */
#define MAX_MSG_LEN (32*1024)

static char* logging_query_filename= NULL;

static int fd= -1;

// copied from drizzled/sql_parse.cc
const LEX_STRING command_name[]={
  { C_STRING_WITH_LEN("Sleep") },
  { C_STRING_WITH_LEN("Quit") },
  { C_STRING_WITH_LEN("InitDB") },
  { C_STRING_WITH_LEN("Query") },
  { C_STRING_WITH_LEN("FieldList") },
  { C_STRING_WITH_LEN("CreateDB") },
  { C_STRING_WITH_LEN("DropDB") },
  { C_STRING_WITH_LEN("Refresh") },
  { C_STRING_WITH_LEN("Shutdown") },
  { C_STRING_WITH_LEN("Processlist") },
  { C_STRING_WITH_LEN("Connect") },
  { C_STRING_WITH_LEN("Kill") },
  { C_STRING_WITH_LEN("Ping") },
  { C_STRING_WITH_LEN("Time") },
  { C_STRING_WITH_LEN("ChangeUser") },
  { C_STRING_WITH_LEN("BinlogDump") },
  { C_STRING_WITH_LEN("ConnectOut") },
  { C_STRING_WITH_LEN("RegisterSlave") },
  { C_STRING_WITH_LEN("SetOption") },
  { C_STRING_WITH_LEN("Daemon") },
  { C_STRING_WITH_LEN("Error") }
};


bool logging_query_func_pre (THD *thd)
{
  char msgbuf[MAX_MSG_LEN];
  int msgbuf_len= 0;
  int wrv;

  if (fd < 0) 
    return false;

  assert(thd != NULL);

  msgbuf_len=
    snprintf(msgbuf, MAX_MSG_LEN,
	     "log bgn thread_id=%ld query_id=%ld command=%.*s"
	     " db=\"%.*s\" query=\"%.*s\"\n",
	     (unsigned long) thd->thread_id,
	     (unsigned long) thd->query_id,
	     (uint32_t)command_name[thd->command].length,
	     command_name[thd->command].str,
	     thd->db_length, thd->db,
	     thd->query_length, thd->query);
  /* a single write has a OS level thread lock
     so there is no need to have mutexes guarding this write,
  */
  wrv= write(fd, msgbuf, msgbuf_len);
  assert(wrv == msgbuf_len);

  return false;
}

bool logging_query_func_post (THD *thd)
{
  char msgbuf[MAX_MSG_LEN];
  int msgbuf_len= 0;
  int wrv;

  if (fd < 0) return false;

  assert(thd != NULL);

  msgbuf_len=
    snprintf(msgbuf, MAX_MSG_LEN,
	     "log end thread_id=%ld query_id=%ld command=%.*s"
	     " rows.sent=%ld rows.exam=%u\n",
	     (unsigned long) thd->thread_id, 
	     (unsigned long) thd->query_id,
	     (uint32_t)command_name[thd->command].length,
	     command_name[thd->command].str,
	     (unsigned long) thd->sent_row_count,
	     (uint32_t) thd->examined_row_count);
  /* a single write has a OS level thread lock
     so there is no need to have mutexes guarding this write,
  */
  wrv= write(fd, msgbuf, msgbuf_len);
  assert(wrv == msgbuf_len);


  return false;
}

static int logging_query_plugin_init(void *p)
{
  logging_t *l= (logging_t *) p;

  if (logging_query_filename == NULL)
  {
    /* no destination filename was specified via system variables
       return now, dont set the callback pointers 
    */
    return 0;
  }

  fd= open(logging_query_filename, O_WRONLY | O_APPEND | O_CREAT,
           S_IRUSR|S_IWUSR);
  if (fd < 0) 
  {
    fprintf(stderr, "fail open fn=%s er=%s\n",
	    logging_query_filename,
	    strerror(errno));

    /* todo
       we should return an error here, so the plugin doesnt load
       but this causes Drizzle to crash
       so until that is fixed,
       just return a success,
       but leave the function pointers as NULL and the fd as -1
    */
    return 0;
  }

  l->logging_pre= logging_query_func_pre;
  l->logging_post= logging_query_func_post;

  return 0;
}

static int logging_query_plugin_deinit(void *p)
{
  logging_st *l= (logging_st *) p;

  if (fd >= 0) 
  {
    close(fd);
    fd= -1;
  }

  l->logging_pre= NULL;
  l->logging_post= NULL;

  return 0;
}

static DRIZZLE_SYSVAR_STR(filename, logging_query_filename,
  PLUGIN_VAR_READONLY,
  "File to log queries to.",
  NULL, NULL, NULL);

static struct st_mysql_sys_var* logging_query_system_variables[]= {
  DRIZZLE_SYSVAR(filename),
  NULL
};

mysql_declare_plugin(logging_query)
{
  DRIZZLE_LOGGER_PLUGIN,
  "logging_query",
  "0.1",
  "Mark Atwood <mark@fallenpegasus.com>",
  "Log queries",
  PLUGIN_LICENSE_GPL,
  logging_query_plugin_init,
  logging_query_plugin_deinit,
  NULL,   /* status variables */
  logging_query_system_variables,
  NULL
}
mysql_declare_plugin_end;
