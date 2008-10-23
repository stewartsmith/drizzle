/* -*- mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; -*-
 *  vim:expandtab:shiftwidth=2:tabstop=2:smarttab:
 *
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

/* need to define DRIZZLE_SERVER to get inside the Session */
#define DRIZZLE_SERVER 1
#include <drizzled/server_includes.h>
#include <drizzled/plugin_errmsg.h>
#include <drizzled/gettext.h>

#include <stdio.h>  /* for vsnprintf */
#include <stdarg.h>  /* for va_list */
#include <unistd.h>  /* for write(2) */

/* todo, make this dynamic as needed */
#define MAX_MSG_LEN 8192

bool errmsg_stderr_func (Session *session __attribute__((unused)),
			 int priority __attribute__((unused)),
			 const char *format, va_list ap)
{
  char msgbuf[MAX_MSG_LEN];
  int prv, wrv;

  prv= vsnprintf(msgbuf, MAX_MSG_LEN, format, ap);
  if (prv < 0) return true;

  /* a single write has a OS level thread lock
     so there is no need to have mutexes guarding this write,
  */
  wrv= write(2, msgbuf, prv);
  if ((wrv < 0) || (wrv != prv)) return true;

  return false;
}

static int errmsg_stderr_plugin_init(void *p)
{
  errmsg_t *l= (errmsg_t *) p;

  l->errmsg_func= errmsg_stderr_func;

  return 0;
}

static int errmsg_stderr_plugin_deinit(void *p)
{
  errmsg_st *l= (errmsg_st *) p;

  l->errmsg_func= NULL;

  return 0;
}

mysql_declare_plugin(errmsg_stderr)
{
  DRIZZLE_LOGGER_PLUGIN,
  "errmsg_stderr",
  "0.1",
  "Mark Atwood <mark@fallenpegasus.com>",
  N_("Error Messages to stderr"),
  PLUGIN_LICENSE_GPL,
  errmsg_stderr_plugin_init,
  errmsg_stderr_plugin_deinit,
  NULL, /* status variables */
  NULL, /* system variables */
  NULL
}
mysql_declare_plugin_end;
