/* -*- mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; -*-
 *  vim:expandtab:shiftwidth=2:tabstop=2:smarttab:
 *
 *  Copyright (C) 2009 Sun Microsystems, Inc.
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

#include <curl/curl.h>

#include <string>
#include <cassert>
#include <boost/program_options.hpp>
#include <drizzled/module/option_map.h>
#include <drizzled/identifier.h>
#include <drizzled/plugin/authentication.h>
#include <drizzled/gettext.h>
namespace po= boost::program_options;
using namespace drizzled;
using namespace std;

static size_t curl_cb_read(void *ptr, size_t size, size_t nmemb, void *stream)
{
  (void) ptr;
  (void) stream;
  return (size * nmemb);
}


class Auth_http : public drizzled::plugin::Authentication
{
  CURLcode rv;
  CURL *curl_handle;
  const std::string auth_url;
public:
  Auth_http(std::string name_arg, const std::string &url_arg) :
    drizzled::plugin::Authentication(name_arg),
    auth_url(url_arg)
  {
    // we are trusting that plugin initializers are called singlethreaded at startup
    // if something else also calls curl_global_init() in a threadrace while we are here,
    // we will crash the server. 
    curl_handle= curl_easy_init();

    // turn off curl stuff that might mess us up
    rv= curl_easy_setopt(curl_handle, CURLOPT_VERBOSE, 0);
    rv= curl_easy_setopt(curl_handle, CURLOPT_NOPROGRESS, 1);
    rv= curl_easy_setopt(curl_handle, CURLOPT_NOSIGNAL, 1);

    // do a HEAD instead of a default GET
    rv= curl_easy_setopt(curl_handle, CURLOPT_NOBODY, 1);

    // set the read callback.  this shouldnt get called, because we are doing a HEAD
    rv= curl_easy_setopt(curl_handle, CURLOPT_READFUNCTION, curl_cb_read);
  }

  ~Auth_http()
  {
    curl_easy_cleanup(curl_handle);
    curl_global_cleanup();
  }

  virtual bool authenticate(const identifier::User &sctx, const string &password)
  {
    long http_response_code;

    assert(sctx.username().c_str());

    // set the parameters: url, username, password
    rv= curl_easy_setopt(curl_handle, CURLOPT_URL, auth_url.c_str());
#if defined(HAVE_CURLOPT_USERNAME)

    rv= curl_easy_setopt(curl_handle, CURLOPT_USERNAME,
                         sctx.username().c_str());
    rv= curl_easy_setopt(curl_handle, CURLOPT_PASSWORD, password.c_str());

#else

    string userpwd(sctx.username());
    userpwd.append(":");
    userpwd.append(password);
    rv= curl_easy_setopt(curl_handle, CURLOPT_USERPWD, userpwd.c_str());

#endif /* defined(HAVE_CURLOPT_USERNAME) */

    // do it
    rv= curl_easy_perform(curl_handle);

    // what did we get? goes into http_response_code
    rv= curl_easy_getinfo(curl_handle, CURLINFO_RESPONSE_CODE, &http_response_code);

    // so here is an interesting question.
    // return true if the response_code is 2XX, or return false if its 4XX
    // for now, return false for 401, true otherwise
    // this means that if the url breaks, then anyone can log in
    // this might be the wrong thing

    if (http_response_code == 401)
      return false;
    return true;
  }
};

Auth_http* auth= NULL;

static int initialize(drizzled::module::Context &context)
{
  const module::option_map &vm= context.getOptions();

  /* 
   * Per libcurl manual, in multi-threaded applications, curl_global_init() should
   * be called *before* curl_easy_init()...which is called in Auto_http's 
   * constructor.
   */
  if (curl_global_init(CURL_GLOBAL_NOTHING) != 0)
    return 1;

  const string auth_url(vm["url"].as<string>());
  if (auth_url.size() == 0)
  {
    errmsg_printf(error::ERROR,
                  _("auth_http plugin loaded but required option url not "
                    "specified. Against which URL are you intending on "
                    "authenticating?\n"));
    return 1;
  }

  auth= new Auth_http("auth_http", auth_url);
  context.add(auth);
  context.registerVariable(new sys_var_const_string_val("url", auth_url));

  return 0;
}

static void init_options(drizzled::module::option_context &context)
{
  context("url", po::value<string>()->default_value(""),
          N_("URL for HTTP Auth check"));
} 


DRIZZLE_DECLARE_PLUGIN
{
  DRIZZLE_VERSION_ID,
  "auth-http",
  "0.1",
  "Mark Atwood",
  "HTTP based authenication.",
  PLUGIN_LICENSE_GPL,
  initialize, /* Plugin Init */
  NULL,
  init_options    /* config options */
}
DRIZZLE_DECLARE_PLUGIN_END;
