/* -*- mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; -*-
 *  vim:expandtab:shiftwidth=2:tabstop=2:smarttab:
 *
 *  Copyright (C) 2009 Sun Microsystems
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

#include "config.h"
#include <drizzled/plugin/logging.h>
#include <drizzled/gettext.h>
#include <drizzled/session.h>

#ifdef __sun
# include <syslog.h>
# include <plugin/logging_syslog/names.h>
#else
# define SYSLOG_NAMES 1
# include <syslog.h>
#endif

#include <stdarg.h>
#include <limits.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>


using namespace drizzled;

static bool sysvar_logging_syslog_enable= false;
static char* sysvar_logging_syslog_ident= NULL;
static char* sysvar_logging_syslog_facility= NULL;
static char* sysvar_logging_syslog_priority= NULL;
static ulong sysvar_logging_syslog_threshold_slow= 0;
static ulong sysvar_logging_syslog_threshold_big_resultset= 0;
static ulong sysvar_logging_syslog_threshold_big_examined= 0;

/* stolen from mysys/my_getsystime
   until the Session has a good utime "now" we can use
   will have to use this instead */

static uint64_t get_microtime()
{
#if defined(HAVE_GETHRTIME)
  return gethrtime()/1000;
#else
  uint64_t newtime;
  struct timeval t;
  /* loop is because gettimeofday may fail on some systems */
  while (gettimeofday(&t, NULL) != 0) {}
  newtime= (uint64_t)t.tv_sec * 1000000 + t.tv_usec;
  return newtime;
#endif
}

class Logging_syslog: public drizzled::plugin::Logging
{

  int syslog_facility;
  int syslog_priority;

public:

  Logging_syslog()
    : drizzled::plugin::Logging("Logging_syslog"),
      syslog_facility(-1), syslog_priority(-1)
  {

    for (int ndx= 0; facilitynames[ndx].c_name; ndx++)
    {
      if (strcasecmp(facilitynames[ndx].c_name, sysvar_logging_syslog_facility) == 0)
      {
        syslog_facility= facilitynames[ndx].c_val;
        break;
      }
    }
    if (syslog_facility == -1)
    {
      errmsg_printf(ERRMSG_LVL_WARN,
                    _("syslog facility \"%s\" not known, using \"local0\""),
                    sysvar_logging_syslog_facility);
      syslog_facility= LOG_LOCAL0;
    }

    for (int ndx= 0; prioritynames[ndx].c_name; ndx++)
    {
      if (strcasecmp(prioritynames[ndx].c_name, sysvar_logging_syslog_priority) == 0)
      {
        syslog_priority= prioritynames[ndx].c_val;
        break;
      }
    }
    if (syslog_priority == -1)
    {
      errmsg_printf(ERRMSG_LVL_WARN,
                    _("syslog priority \"%s\" not known, using \"info\""),
                    sysvar_logging_syslog_priority);
      syslog_priority= LOG_INFO;
    }

    openlog(sysvar_logging_syslog_ident,
            LOG_PID, syslog_facility);
  }

  ~Logging_syslog()
  {
    closelog();
  }

  virtual bool post (Session *session)
  {
    assert(session != NULL);
  
    // return if not enabled or query was too fast or resultset was too small
    if (sysvar_logging_syslog_enable == false)
      return false;
    if (session->sent_row_count < sysvar_logging_syslog_threshold_big_resultset)
      return false;
    if (session->examined_row_count < sysvar_logging_syslog_threshold_big_examined)
      return false;
  
    /* TODO, the session object should have a "utime command completed"
       inside itself, so be more accurate, and so this doesnt have to
       keep calling current_utime, which can be slow */
  
    uint64_t t_mark= get_microtime();
  
    if ((t_mark - session->start_utime) < sysvar_logging_syslog_threshold_slow)
      return false;
  
    /* to avoid trying to printf %s something that is potentially NULL */
  
    const char *dbs= session->db.empty() ? "" : session->db.c_str();
  
    const char *qys= (! session->getQueryString().empty()) ? session->getQueryString().c_str() : "";
    int qyl= 0;
    if (qys)
      qyl= session->getQueryLength();
    
    syslog(syslog_priority,
           "thread_id=%ld query_id=%ld"
           " db=\"%.*s\""
           " query=\"%.*s\""
           " command=\"%.*s\""
           " t_connect=%lld t_start=%lld t_lock=%lld"
           " rows_sent=%ld rows_examined=%ld"
           " tmp_table=%ld total_warn_count=%ld\n",
           (unsigned long) session->thread_id,
           (unsigned long) session->getQueryId(),
           (int)session->db.length(), dbs,
           qyl, qys,
           (int) command_name[session->command].length,
           command_name[session->command].str,
           (unsigned long long) (t_mark - session->getConnectMicroseconds()),
           (unsigned long long) (t_mark - session->start_utime),
           (unsigned long long) (t_mark - session->utime_after_lock),
           (unsigned long) session->sent_row_count,
           (unsigned long) session->examined_row_count,
           (unsigned long) session->tmp_table,
           (unsigned long) session->total_warn_count);
  
    return false;
  }
};

static Logging_syslog *handler= NULL;

static int logging_syslog_plugin_init(drizzled::plugin::Context &context)
{
  handler= new Logging_syslog();
  context.add(handler);

  return 0;
}

static DRIZZLE_SYSVAR_BOOL(
  enable,
  sysvar_logging_syslog_enable,
  PLUGIN_VAR_NOCMDARG,
  N_("Enable logging to syslog"),
  NULL, /* check func */
  NULL, /* update func */
  false /* default */);

static DRIZZLE_SYSVAR_STR(
  ident,
  sysvar_logging_syslog_ident,
  PLUGIN_VAR_READONLY,
  N_("Syslog Ident"),
  NULL, /* check func */
  NULL, /* update func*/
  "drizzled" /* default */);

static DRIZZLE_SYSVAR_STR(
  facility,
  sysvar_logging_syslog_facility,
  PLUGIN_VAR_READONLY,
  N_("Syslog Facility"),
  NULL, /* check func */
  NULL, /* update func*/
  "local0" /* default */);  // local0 is what PostGreSQL uses by default

static DRIZZLE_SYSVAR_STR(
  priority,
  sysvar_logging_syslog_priority,
  PLUGIN_VAR_READONLY,
  N_("Syslog Priority"),
  NULL, /* check func */
  NULL, /* update func*/
  "info" /* default */);

static DRIZZLE_SYSVAR_ULONG(
  threshold_slow,
  sysvar_logging_syslog_threshold_slow,
  PLUGIN_VAR_OPCMDARG,
  N_("Threshold for logging slow queries, in microseconds"),
  NULL, /* check func */
  NULL, /* update func */
  0, /* default */
  0, /* min */
  ULONG_MAX, /* max */
  0 /* blksiz */);

static DRIZZLE_SYSVAR_ULONG(
  threshold_big_resultset,
  sysvar_logging_syslog_threshold_big_resultset,
  PLUGIN_VAR_OPCMDARG,
  N_("Threshold for logging big queries, for rows returned"),
  NULL, /* check func */
  NULL, /* update func */
  0, /* default */
  0, /* min */
  ULONG_MAX, /* max */
  0 /* blksiz */);

static DRIZZLE_SYSVAR_ULONG(
  threshold_big_examined,
  sysvar_logging_syslog_threshold_big_examined,
  PLUGIN_VAR_OPCMDARG,
  N_("Threshold for logging big queries, for rows examined"),
  NULL, /* check func */
  NULL, /* update func */
  0, /* default */
  0, /* min */
  ULONG_MAX, /* max */
  0 /* blksiz */);

static drizzle_sys_var* logging_syslog_system_variables[]= {
  DRIZZLE_SYSVAR(enable),
  DRIZZLE_SYSVAR(ident),
  DRIZZLE_SYSVAR(facility),
  DRIZZLE_SYSVAR(priority),
  DRIZZLE_SYSVAR(threshold_slow),
  DRIZZLE_SYSVAR(threshold_big_resultset),
  DRIZZLE_SYSVAR(threshold_big_examined),
  NULL
};

DRIZZLE_DECLARE_PLUGIN
{
  DRIZZLE_VERSION_ID,
  "logging_syslog",
  "0.2",
  "Mark Atwood <mark@fallenpegasus.com>",
  N_("Log to syslog"),
  PLUGIN_LICENSE_GPL,
  logging_syslog_plugin_init,
  logging_syslog_system_variables,
  NULL
}
DRIZZLE_DECLARE_PLUGIN_END;
