/* -*- mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; -*-
 *  vim:expandtab:shiftwidth=2:tabstop=2:smarttab:
 *
 *  Copyright (C) 2008, 2009 Sun Microsystems, Inc.
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

#include <config.h>
#include <drizzled/plugin.h>
#include <drizzled/plugin/logging.h>
#include <drizzled/gettext.h>
#include <drizzled/session.h>
#include <drizzled/session/times.h>
#include <drizzled/sql_parse.h>
#include PCRE_HEADER
#include <limits.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string>
#include <boost/format.hpp>
#include <boost/program_options.hpp>
#include <drizzled/module/option_map.h>
#include <cstdio>
#include <cerrno>

namespace po= boost::program_options;
using namespace drizzled;
using namespace std;

#define ESCAPE_CHAR      '\\'
#define SEPARATOR_CHAR   ','

namespace drizzle_plugin
{

static bool sysvar_logging_query_enable= false;
/* TODO fix these to not be unsigned long once we have sensible sys_var system */
static uint32_constraint sysvar_logging_query_threshold_slow;
static uint32_constraint sysvar_logging_query_threshold_big_resultset;
static uint32_constraint sysvar_logging_query_threshold_big_examined;

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

static void quotify(const string &src, string &dst)
{
  static const char hexit[]= { '0', '1', '2', '3', '4', '5', '6', '7',
			  '8', '9', 'a', 'b', 'c', 'd', 'e', 'f' };
  string::const_iterator src_iter;

  for (src_iter= src.begin(); src_iter < src.end(); ++src_iter)
  {
    if (static_cast<unsigned char>(*src_iter) > 0x7f)
    {
      dst.push_back(*src_iter);
    }
    else if (*src_iter == 0x00)  // null
    {
      dst.push_back(ESCAPE_CHAR); dst.push_back('0');
    }
    else if (*src_iter == 0x07)  // bell
    {
      dst.push_back(ESCAPE_CHAR); dst.push_back('a');
    }
    else if (*src_iter == 0x08)  // backspace
    {
      dst.push_back(ESCAPE_CHAR); dst.push_back('b');
    }
    else if (*src_iter == 0x09)  // horiz tab
    {
      dst.push_back(ESCAPE_CHAR); dst.push_back('t');
    }
    else if (*src_iter == 0x0a)  // line feed
    {
      dst.push_back(ESCAPE_CHAR); dst.push_back('n');
    }
    else if (*src_iter == 0x0b)  // vert tab
    {
      dst.push_back(ESCAPE_CHAR); dst.push_back('v');
    }
    else if (*src_iter == 0x0c)  // formfeed
    {
      dst.push_back(ESCAPE_CHAR); dst.push_back('f');
    }
    else if (*src_iter == 0x0d)  // carrage return
    {
      dst.push_back(ESCAPE_CHAR); dst.push_back('r');
    }
    else if (*src_iter == 0x1b)  // escape
    {
      dst.push_back(ESCAPE_CHAR); dst.push_back('e');
    }
    else if (*src_iter == 0x22)  // quotation mark
    {
      dst.push_back(ESCAPE_CHAR); dst.push_back(0x22);
    }
    else if (*src_iter == SEPARATOR_CHAR)
    {
      dst.push_back(ESCAPE_CHAR); dst.push_back(SEPARATOR_CHAR);
    }
    else if (*src_iter == ESCAPE_CHAR)
    {
      dst.push_back(ESCAPE_CHAR); dst.push_back(ESCAPE_CHAR);
    }
    else if ((*src_iter < 0x20) || (*src_iter == 0x7F))  // other unprintable ASCII
    {
      dst.push_back(ESCAPE_CHAR);
      dst.push_back('x');
      dst.push_back(hexit[(*src_iter >> 4) & 0x0f]);
      dst.push_back(hexit[*src_iter & 0x0f]);
    }
    else  // everything else
    {
      dst.push_back(*src_iter);
    }
  }
}


class Logging_query: public drizzled::plugin::Logging
{
  const std::string _filename;
  const std::string _query_pcre;
  int fd;
  pcre *re;
  pcre_extra *pe;

  /** Format of the output string */
  boost::format formatter;

public:

  Logging_query(const std::string &filename,
                const std::string &query_pcre) :
    drizzled::plugin::Logging("Logging_query"),
    _filename(filename),
    _query_pcre(query_pcre),
    fd(-1), re(NULL), pe(NULL),
    formatter("%1%,%2%,%3%,\"%4%\",\"%5%\",\"%6%\",%7%,%8%,"
              "%9%,%10%,%11%,%12%,%13%,%14%,\"%15%\"\n")
  {

    /* if there is no destination filename, dont bother doing anything */
    if (_filename.empty())
      return;

    fd= open(_filename.c_str(),
             O_WRONLY | O_APPEND | O_CREAT,
             S_IRUSR|S_IWUSR);

    if (fd < 0)
    {
      sql_perror( _("fail open()"), _filename);
      return;
    }

    if (not _query_pcre.empty())
    {
      const char *this_pcre_error;
      int this_pcre_erroffset;
      re= pcre_compile(_query_pcre.c_str(), 0, &this_pcre_error,
                       &this_pcre_erroffset, NULL);
      pe= pcre_study(re, 0, &this_pcre_error);
      /* TODO emit error messages if there is a problem */
    }
  }

  ~Logging_query()
  {
    if (fd >= 0)
    {
      close(fd);
    }

    if (pe != NULL)
    {
      pcre_free(pe);
    }

    if (re != NULL)
    {
      pcre_free(re);
    }
  }

  virtual bool post (Session *session)
  {
    size_t wrv;

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
    if (session->sent_row_count < sysvar_logging_query_threshold_big_resultset.get())
      return false;
    if (session->examined_row_count < sysvar_logging_query_threshold_big_examined.get())
      return false;

    /*
      TODO, the session object should have a "utime command completed"
      inside itself, so be more accurate, and so this doesnt have to
      keep calling current_utime, which can be slow.
    */
    uint64_t t_mark= session->times.getCurrentTimestamp(false);

    if (session->times.getElapsedTime() < sysvar_logging_query_threshold_slow.get())
      return false;

    Session::QueryString query_string(session->getQueryString());
    if (query_string == NULL)
    {
      return false;
    }

    if (re)
    {
      int this_pcre_rc;
      this_pcre_rc= pcre_exec(re, pe, query_string->c_str(), query_string->length(), 0, 0, NULL, 0);
      if (this_pcre_rc < 0)
        return false;
    }

    // buffer to quotify the query
    string qs;

    // Since quotify() builds the quoted string incrementally, we can
    // avoid some reallocating if we reserve some space up front.
    qs.reserve(query_string->length());

    quotify(*query_string, qs);

    // to avoid trying to printf %s something that is potentially NULL
    util::string::ptr schema(session->schema());
    formatter % t_mark
              % session->thread_id
              % session->getQueryId()
              % (schema ? schema->c_str() : "")
              % qs
              % getCommandName(session->command)
              % (t_mark - session->times.getConnectMicroseconds())
              % session->times.getElapsedTime()
              % (t_mark - session->times.utime_after_lock)
              % session->sent_row_count
              % session->examined_row_count
              % session->tmp_table
              % session->total_warn_count
              % session->getServerId()
              % getServerHostname();

    string msgbuf= formatter.str();

    // a single write has a kernel thread lock, thus no need mutex guard this
    wrv= write(fd, msgbuf.c_str(), msgbuf.length());
    assert(wrv == msgbuf.length());

    return false;
  }
};

static int logging_query_plugin_init(drizzled::module::Context &context)
{
  const module::option_map &vm= context.getOptions();

  if (vm.count("filename"))
  {
    context.add(new Logging_query(vm["filename"].as<string>(),
                                  vm["pcre"].as<string>()));
    context.registerVariable(new sys_var_bool_ptr("enable", &sysvar_logging_query_enable));
    context.registerVariable(new sys_var_const_string_val("filename", vm["filename"].as<string>()));
    context.registerVariable(new sys_var_const_string_val("pcre", vm["pcre"].as<string>()));
    context.registerVariable(new sys_var_constrained_value<uint32_t>("threshold_slow", sysvar_logging_query_threshold_slow));
    context.registerVariable(new sys_var_constrained_value<uint32_t>("threshold_big_resultset", sysvar_logging_query_threshold_big_resultset));
    context.registerVariable(new sys_var_constrained_value<uint32_t>("threshold_big_examined", sysvar_logging_query_threshold_big_examined));
  }

  return 0;
}

static void init_options(drizzled::module::option_context &context)
{
  context("enable",
          po::value<bool>(&sysvar_logging_query_enable)->default_value(false)->zero_tokens(),
          _("Enable logging to CSV file"));
  context("filename",
          po::value<string>(),
          _("File to log to"));
  context("pcre",
          po::value<string>()->default_value(""),
          _("PCRE to match the query against"));
  context("threshold-slow",
          po::value<uint32_constraint>(&sysvar_logging_query_threshold_slow)->default_value(0),
          _("Threshold for logging slow queries, in microseconds"));
  context("threshold-big-resultset",
          po::value<uint32_constraint>(&sysvar_logging_query_threshold_big_resultset)->default_value(0),
          _("Threshold for logging big queries, for rows returned"));
  context("threshold-big-examined",
          po::value<uint32_constraint>(&sysvar_logging_query_threshold_big_examined)->default_value(0),
          _("Threshold for logging big queries, for rows examined"));
}

} /* namespace drizzle_plugin */

DRIZZLE_DECLARE_PLUGIN
{
  DRIZZLE_VERSION_ID,
  "logging-query",
  "0.2",
  "Mark Atwood <mark@fallenpegasus.com>",
  N_("Log queries to a CSV file"),
  PLUGIN_LICENSE_GPL,
  drizzle_plugin::logging_query_plugin_init,
  NULL,
  drizzle_plugin::init_options
}
DRIZZLE_DECLARE_PLUGIN_END;
