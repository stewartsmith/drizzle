/* -*- mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; -*-
 *  vim:expandtab:shiftwidth=2:tabstop=2:smarttab:
 *
 *  Copyright (C) 2008,2009 Sun Microsystems
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
#include <boost/date_time.hpp>
#include <boost/program_options.hpp>
#include <drizzled/module/option_map.h>
#include <libgearman/gearman.h>
#include <limits.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <cstdio>
#include <cerrno>

using namespace drizzled;
namespace po= boost::program_options;

/* TODO make this dynamic as needed */
static const int MAX_MSG_LEN= 32*1024;

static bool sysvar_logging_gearman_enable;
static char* sysvar_logging_gearman_host= NULL;
static char* sysvar_logging_gearman_function= NULL;

/* quote a string to be safe to include in a CSV line
   that means backslash quoting all commas, doublequotes, backslashes,
   and all the ASCII unprintable characters
   as long as we pass the high-bit bytes unchanged
   this is safe to do to a UTF8 string
   we dont allow overrunning the targetbuffer
   to avoid having a very long query overwrite memory

   TODO consider remapping the unprintables instead to "Printable
   Representation", the Unicode characters from the area U+2400 to
   U+2421 reserved for representing control characters when it is
   necessary to print or display them rather than have them perform
   their intended function.

*/
static unsigned char *quotify (const unsigned char *src, size_t srclen,
                               unsigned char *dst, size_t dstlen)
{
  static const char hexit[]= { '0', '1', '2', '3', '4', '5', '6', '7',
                               '8', '9', 'a', 'b', 'c', 'd', 'e', 'f' };
  size_t dst_ndx;  /* ndx down the dst */
  size_t src_ndx;  /* ndx down the src */

  assert(dst);
  assert(dstlen > 0);

  for (dst_ndx= 0,src_ndx= 0; src_ndx < srclen; src_ndx++)
    {

      /* Worst case, need 5 dst bytes for the next src byte.
         backslash x hexit hexit null
         so if not enough room, just terminate the string and return
      */
      if ((dstlen - dst_ndx) < 5)
        {
          dst[dst_ndx]= (unsigned char)0x00;
          return dst;
        }

      if (src[src_ndx] > 0x7f)
        {
          // pass thru high bit characters, they are non-ASCII UTF8 Unicode
          dst[dst_ndx++]= src[src_ndx];
        }
      else if (src[src_ndx] == 0x00)  // null
        {
          dst[dst_ndx++]= 0x5C; dst[dst_ndx++]= (unsigned char) '0';
        }
      else if (src[src_ndx] == 0x07)  // bell
        {
          dst[dst_ndx++]= 0x5C; dst[dst_ndx++]= (unsigned char) 'a';
        }
      else if (src[src_ndx] == 0x08)  // backspace
        {
          dst[dst_ndx++]= 0x5C; dst[dst_ndx++]= (unsigned char) 'b';
        }
      else if (src[src_ndx] == 0x09)  // horiz tab
        {
          dst[dst_ndx++]= 0x5C; dst[dst_ndx++]= (unsigned char) 't';
        }
      else if (src[src_ndx] == 0x0a)  // line feed
        {
          dst[dst_ndx++]= 0x5C; dst[dst_ndx++]= (unsigned char) 'n';
        }
      else if (src[src_ndx] == 0x0b)  // vert tab
        {
          dst[dst_ndx++]= 0x5C; dst[dst_ndx++]= (unsigned char) 'v';
        }
      else if (src[src_ndx] == 0x0c)  // formfeed
        {
          dst[dst_ndx++]= 0x5C; dst[dst_ndx++]= (unsigned char) 'f';
        }
      else if (src[src_ndx] == 0x0d)  // carrage return
        {
          dst[dst_ndx++]= 0x5C; dst[dst_ndx++]= (unsigned char) 'r';
        }
      else if (src[src_ndx] == 0x1b)  // escape
        {
          dst[dst_ndx++]= 0x5C; dst[dst_ndx++]= (unsigned char) 'e';
        }
      else if (src[src_ndx] == 0x22)  // quotation mark
        {
          dst[dst_ndx++]= 0x5C; dst[dst_ndx++]= 0x22;
        }
      else if (src[src_ndx] == 0x2C)  // comma
        {
          dst[dst_ndx++]= 0x5C; dst[dst_ndx++]= 0x2C;
        }
      else if (src[src_ndx] == 0x5C)  // backslash
        {
          dst[dst_ndx++]= 0x5C; dst[dst_ndx++]= 0x5C;
        }
      else if ((src[src_ndx] < 0x20) || (src[src_ndx] == 0x7F))  // other unprintable ASCII
        {
          dst[dst_ndx++]= 0x5C;
          dst[dst_ndx++]= (unsigned char) 'x';
          dst[dst_ndx++]= hexit[(src[src_ndx] >> 4) & 0x0f];
          dst[dst_ndx++]= hexit[src[src_ndx] & 0x0f];
        }
      else  // everything else
        {
          dst[dst_ndx++]= src[src_ndx];
        }
      dst[dst_ndx]= '\0';
    }
  return dst;
}

class LoggingGearman : public plugin::Logging
{

  int gearman_client_ok;
  gearman_client_st gearman_client;

public:

  LoggingGearman()
    : plugin::Logging("LoggingGearman"),
      gearman_client_ok(0)
  {
    gearman_return_t ret;

    if (sysvar_logging_gearman_enable == false)
      return;

    if (sysvar_logging_gearman_host == NULL)
      return;


    if (gearman_client_create(&gearman_client) == NULL)
    {
      char errmsg[STRERROR_MAX];
      strerror_r(errno, errmsg, sizeof(errmsg));
      errmsg_printf(ERRMSG_LVL_ERROR, _("fail gearman_client_create(): %s"),
                    errmsg);
      return;
    }

    /* TODO, be able to override the port */
    /* TODO, be able send to multiple servers */
    ret= gearman_client_add_server(&gearman_client,
                                   sysvar_logging_gearman_host, 0);
    if (ret != GEARMAN_SUCCESS)
    {
      errmsg_printf(ERRMSG_LVL_ERROR, _("fail gearman_client_add_server(): %s"),
                    gearman_client_error(&gearman_client));
      return;
    }

    gearman_client_ok= 1;

  }

  ~LoggingGearman()
  {
    if (gearman_client_ok)
    {
      gearman_client_free(&gearman_client);
    }
  }

  virtual bool post(Session *session)
  {
    char msgbuf[MAX_MSG_LEN];
    int msgbuf_len= 0;
  
    assert(session != NULL);

    /* in theory, we should return "true", meaning that the plugin isn't happy,
       but that crashes the server, so for now, we just lie a little bit
    */

    if (!gearman_client_ok)
        return false;
  
    /* TODO, the session object should have a "utime command completed"
       inside itself, so be more accurate, and so this doesnt have to
       keep calling current_utime, which can be slow */
  
    boost::posix_time::ptime mytime(boost::posix_time::microsec_clock::local_time());
    boost::posix_time::ptime epoch(boost::gregorian::date(1970,1,1));
    uint64_t t_mark= (mytime-epoch).total_microseconds();
  

    // buffer to quotify the query
    unsigned char qs[255];
  
    // to avoid trying to printf %s something that is potentially NULL
    const char *dbs= session->db.empty() ? "" : session->db.c_str();
  
    msgbuf_len=
      snprintf(msgbuf, MAX_MSG_LEN,
               "%"PRIu64",%"PRIu64",%"PRIu64",\"%.*s\",\"%s\",\"%.*s\","
               "%"PRIu64",%"PRIu64",%"PRIu64",%"PRIu64",%"PRIu64","
               "%"PRIu32",%"PRIu32",%"PRIu32",\"%s\"",
               t_mark,
               session->thread_id,
               session->getQueryId(),
               // dont need to quote the db name, always CSV safe
               (int)session->db.length(), dbs,
               // do need to quote the query
               quotify((const unsigned char *)session->getQueryString().c_str(),
                       session->getQueryLength(), qs, sizeof(qs)),
               // command_name is defined in drizzled/sql_parse.cc
               // dont need to quote the command name, always CSV safe
               (int)command_name[session->command].length,
               command_name[session->command].str,
               // counters are at end, to make it easier to add more
               (t_mark - session->getConnectMicroseconds()),
               (t_mark - session->start_utime),
               (t_mark - session->utime_after_lock),
               session->sent_row_count,
               session->examined_row_count,
               session->tmp_table,
               session->total_warn_count,
               session->getServerId(),
               glob_hostname
               );
  
    char job_handle[GEARMAN_JOB_HANDLE_SIZE];
  
    (void) gearman_client_do_background(&gearman_client,
                                        sysvar_logging_gearman_function,
                                        NULL,
                                        (void *) msgbuf,
                                        (size_t) msgbuf_len,
                                        job_handle);
  
    return false;
  }
};

static LoggingGearman *handler= NULL;

static int logging_gearman_plugin_init(module::Context &context)
{
  handler= new LoggingGearman();
  context.add(handler);

  return 0;
}

static void init_options(drizzled::module::option_context &context)
{
  context("enable",
          po::value<bool>(&sysvar_logging_gearman_enable)->default_value(false)->zero_tokens(),
          N_("Enable logging to a gearman server"));
}

static DRIZZLE_SYSVAR_BOOL(
                           enable,
                           sysvar_logging_gearman_enable,
                           PLUGIN_VAR_NOCMDARG,
                           N_("Enable logging to a gearman server"),
                           NULL, /* check func */
                           NULL, /* update func */
                           false /* default */);

static DRIZZLE_SYSVAR_STR(
                          host,
                          sysvar_logging_gearman_host,
                          PLUGIN_VAR_READONLY,
                          N_("Hostname for logging to a Gearman server"),
                          NULL, /* check func */
                          NULL, /* update func*/
                          "localhost" /* default */);

static DRIZZLE_SYSVAR_STR(
                          function,
                          sysvar_logging_gearman_function,
                          PLUGIN_VAR_READONLY,
                          N_("Gearman Function to send logging to"),
                          NULL, /* check func */
                          NULL, /* update func*/
                          "drizzlelog" /* default */);

static drizzle_sys_var* logging_gearman_system_variables[]= {
  DRIZZLE_SYSVAR(enable),
  DRIZZLE_SYSVAR(host),
  DRIZZLE_SYSVAR(function),
  NULL
};

DRIZZLE_DECLARE_PLUGIN
{
  DRIZZLE_VERSION_ID,
    "logging-gearman",
    "0.1",
    "Mark Atwood <mark@fallenpegasus.com>",
    N_("Log queries to a Gearman server"),
    PLUGIN_LICENSE_GPL,
    logging_gearman_plugin_init,
    logging_gearman_system_variables,
    init_options
}
DRIZZLE_DECLARE_PLUGIN_END;
