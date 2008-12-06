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

#include <drizzled/server_includes.h>
#include <drizzled/replicator.h>
#include <drizzled/gettext.h>
#include <drizzled/session.h>

int replicator_initializer(st_plugin_int *plugin)
{
  replicator_t *p;

  p= (replicator_t *) malloc(sizeof(replicator_t));
  if (p == NULL) return 1;
  memset(p, 0, sizeof(replicator_t));

  plugin->data= (void *)p;

  if (plugin->plugin->init)
    {
      if (plugin->plugin->init((void *)p))
        {
          /* TRANSLATORS: The leading word "replicator" is the name
             of the plugin api, and so should not be translated. */
          sql_print_error(_("replicator plugin '%s' init() failed"),
                          plugin->name.str);
          goto err;
        }
    }
  return 0;

 err:
  free(p);
  return 1;
}

int replicator_finalizer(st_plugin_int *plugin)
{ 
  replicator_t *p= (replicator_t *) plugin->data;

  if (plugin->plugin->deinit)
    {
      if (plugin->plugin->deinit((void *)p))
        {
          /* TRANSLATORS: The leading word "replicator" is the name
             of the plugin api, and so should not be translated. */
          sql_print_error(_("replicator plugin '%s' deinit() failed"),
                          plugin->name.str);
        }
    }

  if (p) free(p);

  return 0;
}

/* This gets called by plugin_foreach once for each loaded replicator plugin */
static bool replicator_session_iterate (Session *session, plugin_ref plugin, void *)
{
  replicator_t *repl= plugin_data(plugin, replicator_t *);
  void *session_data;

  /* call this loaded replicator plugin's replicator_func1 function pointer */
  if (repl && repl->session_init)
    {
      session_data= repl->session_init(session);
      if (session_data)
        {
          /* TRANSLATORS: The leading word "replicator" is the name
             of the plugin api, and so should not be translated. */
          sql_print_error(_("replicator plugin '%s' replicator_session_init() failed"),
                          (char *)plugin_name(plugin));
          return true;
        }
    }

  session->setReplicationData(session_data);

  return false;
}

/*
  This call is called once at the begining of each transaction.
*/
bool replicator_session_init(Session *session)
{
  bool foreach_rv;

  /* 
    call replicator_session_iterate
    once for each loaded replicator plugin 
  */
  foreach_rv= plugin_foreach(session, replicator_session_iterate,
                             DRIZZLE_REPLICATOR_PLUGIN, NULL);
  return foreach_rv;
}

/* The plugin_foreach() iterator requires that we
   convert all the parameters of a plugin api entry point
   into just one single void ptr, plus the session.
   So we will take all the additional paramters of replicator_do2,
   and marshall them into a struct of this type, and
   then just pass in a pointer to it.
*/
enum repl_row_exec_t{
  repl_insert,
  repl_update,
  repl_delete
};

typedef struct replicator_row_parms_st
{
  repl_row_exec_t type;
  Table *table;
  const unsigned char *before;
  const unsigned char *after;
} replicator_row_parms_st;


/* This gets called by plugin_foreach once for each loaded replicator plugin */
static bool replicator_do_row_iterate (Session *session, plugin_ref plugin, void *p)
{
  replicator_t *repl= plugin_data(plugin, replicator_t *);
  replicator_row_parms_st *params= (replicator_row_parms_st *) p;

  switch (params->type) {
  case repl_insert:
    if (repl && repl->row_insert)
    {
      if (repl->row_insert(session, params->table))
      {
        /* TRANSLATORS: The leading word "replicator" is the name
          of the plugin api, and so should not be translated. */
        sql_print_error(_("replicator plugin '%s' row_insert() failed"),
                        (char *)plugin_name(plugin));

        return true;
      }
    }
    break;
  case repl_update:
    if (repl && repl->row_update)
    {
      if (repl->row_update(session, params->table, params->before, params->after))
      {
        /* TRANSLATORS: The leading word "replicator" is the name
          of the plugin api, and so should not be translated. */
        sql_print_error(_("replicator plugin '%s' row_update() failed"),
                        (char *)plugin_name(plugin));

        return true;
      }
    }
    break;
  case repl_delete:
    if (repl && repl->row_delete)
    {
      if (repl->row_delete(session, params->table))
      {
        /* TRANSLATORS: The leading word "replicator" is the name
          of the plugin api, and so should not be translated. */
        sql_print_error(_("replicator plugin '%s' row_delete() failed"),
                        (char *)plugin_name(plugin));

        return true;
      }
    }
    break;
  }
  return false;
}

/* This is the replicator_do2 entry point.
   This gets called by the rest of the Drizzle server code */
static bool replicator_do_row (Session *session,  replicator_row_parms_st *params)
{
  bool foreach_rv;

  foreach_rv= plugin_foreach(session, replicator_do_row_iterate,
                             DRIZZLE_REPLICATOR_PLUGIN, (void *) &params);
  return foreach_rv;
}

bool replicator_write_row(Session *session, Table *table)
{
  replicator_row_parms_st param;

  param.type= repl_insert;
  param.table= table;
  param.after= NULL;
  param.before= NULL;

  return replicator_do_row(session, &param);
}

bool replicator_update_row(Session *session, Table *table, 
                           const unsigned char *before, 
                           const unsigned char *after)
{
  replicator_row_parms_st param;

  param.type= repl_update;
  param.table= table;
  param.after= after;
  param.before= before;

  return replicator_do_row(session, &param);
}

bool replicator_delete_row(Session *session, Table *table)
{
  replicator_row_parms_st param;

  param.type= repl_delete;
  param.table= table;
  param.after= NULL;
  param.before= NULL;

  return replicator_do_row(session, &param);
}

