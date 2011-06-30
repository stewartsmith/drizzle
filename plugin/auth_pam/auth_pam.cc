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

/*
  Sections of this were taken/modified from mod_auth_path for Apache
  @TODO: License?
*/

#include <config.h>

#include <security/pam_appl.h>
#if !defined(__sun) && !defined(__FreeBSD__)
#include <security/pam_misc.h>
#endif

#include <drizzled/identifier.h>
#include <drizzled/plugin/authentication.h>

using namespace drizzled;

typedef struct {
    const char *name;
    const char *password;
} auth_pam_userinfo;

extern "C"
int auth_pam_talker(int num_msg,
#ifdef __sun
                    struct pam_message **msg,
#else
                    const struct pam_message **msg,
#endif
                    struct pam_response **resp,
                    void *appdata_ptr);

int auth_pam_talker(int num_msg,
#ifdef __sun
                    struct pam_message **msg,
#else
                    const struct pam_message **msg,
#endif
                    struct pam_response **resp,
                    void *appdata_ptr)
{
  auth_pam_userinfo *userinfo = (auth_pam_userinfo*)appdata_ptr;
  struct pam_response *response = 0;

  /* parameter sanity checking */
  if(not resp || not msg || not userinfo)
    return PAM_CONV_ERR;

  /* allocate memory to store response */
  response= (struct pam_response*)malloc(num_msg * sizeof(struct pam_response));

  /* copy values */
  for(int x= 0; x < num_msg; x++)
  {
    /* initialize to safe values */
    response[x].resp_retcode= 0;
    response[x].resp= 0;

    /* select response based on requested output style */
    switch(msg[x]->msg_style)
    {
    case PAM_PROMPT_ECHO_ON:
      /* on memory allocation failure, auth fails */
      response[x].resp = strdup(userinfo->name);
      break;
    case PAM_PROMPT_ECHO_OFF:
      response[x].resp = strdup(userinfo->password);
      break;
    default:
      free(response);
      return PAM_CONV_ERR;
    }
  }

  /* everything okay, set PAM response values */
  *resp = response;

  return PAM_SUCCESS;
}

class Auth_pam : public drizzled::plugin::Authentication
{
public:
  Auth_pam(std::string name_arg)
    : drizzled::plugin::Authentication(name_arg) {}
  virtual bool authenticate(const identifier::User &sctx,
                            const std::string &password)
  {
    int retval;
    auth_pam_userinfo userinfo= { NULL, NULL };
    struct pam_conv conv_info= { &auth_pam_talker, (void*)&userinfo };
    pam_handle_t *pamh= NULL;

    userinfo.name= sctx.username().c_str();
    userinfo.password= password.c_str();

    retval= pam_start("drizzle", userinfo.name, &conv_info, &pamh);

    if (retval == PAM_SUCCESS)
      retval= pam_authenticate(pamh, PAM_DISALLOW_NULL_AUTHTOK);

    if (retval == PAM_SUCCESS)
      retval= pam_acct_mgmt(pamh, PAM_DISALLOW_NULL_AUTHTOK);

    pam_end(pamh, retval);

    return (retval == PAM_SUCCESS) ? true: false;
  }
};


static Auth_pam *auth= NULL;

static int initialize(drizzled::module::Context &context)
{
  auth= new Auth_pam("auth_pam");
  context.add(auth);
  return 0;
}

DRIZZLE_DECLARE_PLUGIN
{
  DRIZZLE_VERSION_ID,
  "pam",
  "0.1",
  "Brian Aker",
  "PAM based authenication.",
  PLUGIN_LICENSE_GPL,
  initialize, /* Plugin Init */
  NULL,   /* depends */
  NULL    /* config options */
}
DRIZZLE_DECLARE_PLUGIN_END;
