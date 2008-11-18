/*
 -*- mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; -*-
 *  vim:expandtab:shiftwidth=2:tabstop=2:smarttab:
  Sections of this where taken/modified from mod_auth_path for Apache 
*/

#define DRIZZLE_SERVER 1
#include <drizzled/server_includes.h>
#include <drizzled/session.h>
#include <drizzled/plugin_authentication.h>
#include <security/pam_appl.h>
#ifndef __sun
#include <security/pam_misc.h>
#endif

typedef struct {
    const char *name;
    const char *password;
} auth_pam_userinfo;

static int auth_pam_talker(int num_msg,
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
  int x;

  /* parameter sanity checking */
  if(!resp || !msg || !userinfo)
    return PAM_CONV_ERR;

  /* allocate memory to store response */
  response= (struct pam_response*)malloc(num_msg * sizeof(struct pam_response));
  if(!response)
    return PAM_CONV_ERR;

  /* copy values */
  for(x= 0; x < num_msg; x++) 
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
      if(response)
        free(response);
      return PAM_CONV_ERR;
    }
  }

  /* everything okay, set PAM response values */
  *resp = response;

  return PAM_SUCCESS;
}

static bool authenticate(Session *session, const char *password)
{
  int retval;
  auth_pam_userinfo userinfo= { NULL, NULL };
  struct pam_conv conv_info= { &auth_pam_talker, (void*)&userinfo };
  pam_handle_t *pamh= NULL;

  userinfo.name= session->main_security_ctx.user;
  userinfo.password= password;

  retval= pam_start("check_user", userinfo.name, &conv_info, &pamh);

  if (retval == PAM_SUCCESS)
    retval= pam_authenticate(pamh, PAM_DISALLOW_NULL_AUTHTOK);

  if (retval == PAM_SUCCESS)
    retval= pam_acct_mgmt(pamh, PAM_DISALLOW_NULL_AUTHTOK);

  pam_end(pamh, retval);

  return (retval == PAM_SUCCESS) ? true: false;
}

static int initialize(void *p)
{
  authentication_st *auth= (authentication_st *)p;
  
  auth->authenticate= authenticate;

  return 0;
}

static int finalize(void *p)
{
  (void)p;

  return 0;
}

mysql_declare_plugin(auth_pam)
{
  DRIZZLE_AUTH_PLUGIN,
  "pam",
  "0.1",
  "Brian Aker",
  "PAM based authenication.",
  PLUGIN_LICENSE_GPL,
  initialize, /* Plugin Init */
  finalize, /* Plugin Deinit */
  NULL,   /* status variables */
  NULL,   /* system variables */
  NULL    /* config options */
}
mysql_declare_plugin_end;
