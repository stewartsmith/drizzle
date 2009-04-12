/*
 -*- mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; -*-
 *  vim:expandtab:shiftwidth=2:tabstop=2:smarttab:
*/

#include <drizzled/server_includes.h>
#include <drizzled/session.h>
#include <drizzled/plugin/authentication.h>
#include <drizzled/gettext.h>

#include <curl/curl.h>

#include <string>

using namespace std;

static bool sysvar_auth_http_enable= false;
static char* sysvar_auth_http_url= NULL;

size_t curl_cb_read(void *ptr, size_t size, size_t nmemb, void *stream)
{
  (void) ptr;
  (void) stream;
  return (size * nmemb);
}


class Auth_http : public Authentication
{
  CURLcode rv;
  CURL *curl_handle;
public:
  Auth_http() : Authentication()
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
  }

  virtual bool authenticate(Session *session, const char *password)
  {
    long http_response_code;

    if (sysvar_auth_http_enable == false)
      return true;

    assert(session->security_ctx.user.c_str());
    assert(password);


    // set the parameters: url, username, password
    rv= curl_easy_setopt(curl_handle, CURLOPT_URL, sysvar_auth_http_url);
#if defined(HAVE_CURLOPT_USERNAME)

    rv= curl_easy_setopt(curl_handle, CURLOPT_USERNAME,
                         session->security_ctx.user.c_str());
    rv= curl_easy_setopt(curl_handle, CURLOPT_PASSWORD, password);

#else

    string userpwd= session->security_ctx.user;
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

static int initialize(PluginRegistry &registry)
{
  auth= new Auth_http();
  registry.add(auth);

  return 0;
}

static int finalize(PluginRegistry &registry)
{
  if (auth)
  {
    registry.remove(auth);
    delete auth;
  }

  return 0;
}

static DRIZZLE_SYSVAR_BOOL(
  enable,
  sysvar_auth_http_enable,
  PLUGIN_VAR_NOCMDARG,
  N_("Enable HTTP Auth check"),
  NULL, /* check func */
  NULL, /* update func */
  false /* default */);


static DRIZZLE_SYSVAR_STR(
  url,
  sysvar_auth_http_url,
  PLUGIN_VAR_READONLY,
  N_("URL for HTTP Auth check"),
  NULL, /* check func */
  NULL, /* update func*/
  "http://localhost/" /* default */);

static struct st_mysql_sys_var* auth_http_system_variables[]= {
  DRIZZLE_SYSVAR(enable),
  DRIZZLE_SYSVAR(url),
  NULL
};


drizzle_declare_plugin(auth_http)
{
  "auth_http",
  "0.1",
  "Mark Atwood",
  "HTTP based authenication.",
  PLUGIN_LICENSE_GPL,
  initialize, /* Plugin Init */
  finalize, /* Plugin Deinit */
  NULL,   /* status variables */
  auth_http_system_variables,
  NULL    /* config options */
}
drizzle_declare_plugin_end;
