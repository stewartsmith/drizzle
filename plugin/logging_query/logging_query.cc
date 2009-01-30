/* -*- mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; -*-
 *  vim:expandtab:shiftwidth=2:tabstop=2:smarttab:
 *
 *  Copyright (C) 2008,2009 Mark Atwood
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
#include <drizzled/plugin_logging.h>
#include <drizzled/gettext.h>
#include <drizzled/session.h>

/* TODO make this dynamic as needed */
static const int MAX_MSG_LEN= 32*1024;

static bool sysvar_logging_query_enable= false;
static char* sysvar_logging_query_filename= NULL;
/* TODO fix these to not be unsigned long one we have sensible sys_var system */
static unsigned long sysvar_logging_query_threshold_slow= 0;
static unsigned long sysvar_logging_query_threshold_big_resultset= 0;
static unsigned long sysvar_logging_query_threshold_big_examined= 0;

static int fd= -1;

/* stolen from mysys/my_getsystime
   until the Session has a good utime "now" we can use
   will have to use this instead */

#include <sys/time.h>
static uint64_t get_microtime()
{
#if defined(HAVE_GETHRTIME)
  return gethrtime()/1000;
#else
  uint64_t newtime;
  struct timeval t;
  /*
    The following loop is here because gettimeofday may fail on some systems
  */
  while (gettimeofday(&t, NULL) != 0) {}
  newtime= (uint64_t)t.tv_sec * 1000000 + t.tv_usec;
  return newtime;
#endif  /* defined(HAVE_GETHRTIME) */
}

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


/* we could just not have a pre entrypoint at all,
   and have logging_pre == NULL
   but we have this here for the sake of being an example */
bool logging_query_func_pre (Session *)
{
  return false;
}

bool logging_query_func_post (Session *session)
{
  char msgbuf[MAX_MSG_LEN];
  int msgbuf_len= 0;
  int wrv;

  assert(session != NULL);

  if (fd < 0)
    return false;

  /* Yes, we know that checking sysvar_logging_query_enable,
     sysvar_logging_query_threshold_big_resultset, and
     sysvar_logging_query_threshold_big_examined is not threadsafe,
     because some other thread might change these sysvars.  But we
     don't care.  We might start logging a little late as it spreads
     to other threads.  Big deal. */

  // return if not enabled or query was too fast or resultset was too small
  if (sysvar_logging_query_enable == false)
    return false;
  if (session->sent_row_count < sysvar_logging_query_threshold_big_resultset)
    return false;
  if (session->examined_row_count < sysvar_logging_query_threshold_big_examined)
    return false;

  // logging this is far too verbose
  if (session->command == COM_FIELD_LIST)
    return false;

  /* TODO, looks like connect_utime isnt being set in the session
     object.  We could store the time this plugin was loaded, but that
     would just be a dumb workaround. */
  /* TODO, the session object should have a "utime command completed"
     inside itself, so be more accurate, and so this doesnt have to
     keep calling current_utime, which can be slow */

  uint64_t t_mark= get_microtime();

  if ((t_mark - session->start_utime) < (sysvar_logging_query_threshold_slow))
    return false;

  // buffer to quotify the query
  unsigned char qs[255];

  // to avoid trying to printf %s something that is potentially NULL
  const char *dbs= (session->db) ? session->db : "";
  int dbl= 0;
  if (dbs != NULL)
    dbl= session->db_length;

  msgbuf_len=
    snprintf(msgbuf, MAX_MSG_LEN,
             "%"PRIu64",%"PRIu64",%"PRIu64",\"%.*s\",\"%s\",\"%.*s\","
             "%"PRIu64",%"PRIu64",%"PRIu64",%"PRIu64",%"PRIu64"\n",
             t_mark,
             session->thread_id,
             session->query_id,
             // dont need to quote the db name, always CSV safe
             dbl, dbs,
             // do need to quote the query
             quotify((unsigned char *)session->query,
                     session->query_length, qs, sizeof(qs)),
             // command_name is defined in drizzled/sql_parse.cc
             // dont need to quote the command name, always CSV safe
             (int)command_name[session->command].length,
             command_name[session->command].str,
             // counters are at end, to make it easier to add more
             (t_mark - session->connect_utime),
             (t_mark - session->start_utime),
             (t_mark - session->utime_after_lock),
             session->sent_row_count,
             session->examined_row_count);


  // a single write has a kernel thread lock, thus no need mutex guard this
  wrv= write(fd, msgbuf, msgbuf_len);
  assert(wrv == msgbuf_len);

  return false;
}

static int logging_query_plugin_init(void *p)
{
  logging_t *l= static_cast<logging_t *>(p);

  if (sysvar_logging_query_filename == NULL)
  {
    /* no destination filename was specified via system variables
       return now, dont set the callback pointers
    */
    return 0;
  }

  fd= open(sysvar_logging_query_filename,
           O_WRONLY | O_APPEND | O_CREAT,
           S_IRUSR|S_IWUSR);
  if (fd < 0)
  {
    errmsg_printf(ERRMSG_LVL_ERROR, _("fail open() fn=%s er=%s\n"),
                  sysvar_logging_query_filename,
                  strerror(errno));

    /* TODO
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

static DRIZZLE_SYSVAR_BOOL(
  enable,
  sysvar_logging_query_enable,
  PLUGIN_VAR_NOCMDARG,
  N_("Enable logging to CSV file"),
  NULL, /* check func */
  NULL, /* update func */
  false /* default */);

static DRIZZLE_SYSVAR_STR(
  filename,
  sysvar_logging_query_filename,
  PLUGIN_VAR_READONLY,
  N_("File to log to"),
  NULL, /* check func */
  NULL, /* update func*/
  NULL /* default */);

static DRIZZLE_SYSVAR_ULONG(
  threshold_slow,
  sysvar_logging_query_threshold_slow,
  PLUGIN_VAR_OPCMDARG,
  N_("Threshold for logging slow queries, in microseconds"),
  NULL, /* check func */
  NULL, /* update func */
  0, /* default */
  0, /* min */
  UINT32_MAX, /* max */
  0 /* blksiz */);

static DRIZZLE_SYSVAR_ULONG(
  threshold_big_resultset,
  sysvar_logging_query_threshold_big_resultset,
  PLUGIN_VAR_OPCMDARG,
  N_("Threshold for logging big queries, for rows returned"),
  NULL, /* check func */
  NULL, /* update func */
  0, /* default */
  0, /* min */
  UINT32_MAX, /* max */
  0 /* blksiz */);

static DRIZZLE_SYSVAR_ULONG(
  threshold_big_examined,
  sysvar_logging_query_threshold_big_examined,
  PLUGIN_VAR_OPCMDARG,
  N_("Threshold for logging big queries, for rows examined"),
  NULL, /* check func */
  NULL, /* update func */
  0, /* default */
  0, /* min */
  UINT32_MAX, /* max */
  0 /* blksiz */);

static struct st_mysql_sys_var* logging_query_system_variables[]= {
  DRIZZLE_SYSVAR(enable),
  DRIZZLE_SYSVAR(filename),
  DRIZZLE_SYSVAR(threshold_slow),
  DRIZZLE_SYSVAR(threshold_big_resultset),
  DRIZZLE_SYSVAR(threshold_big_examined),
  NULL
};

drizzle_declare_plugin(logging_query)
{
  DRIZZLE_LOGGER_PLUGIN,
  "logging_query",
  "0.2",
  "Mark Atwood <mark@fallenpegasus.com>",
  N_("Log queries to a CSV file"),
  PLUGIN_LICENSE_GPL,
  logging_query_plugin_init,
  logging_query_plugin_deinit,
  NULL,   /* status variables */
  logging_query_system_variables,
  NULL
}
drizzle_declare_plugin_end;
