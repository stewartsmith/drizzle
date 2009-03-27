/* -*- mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; -*-
 *  vim:expandtab:shiftwidth=2:tabstop=2:smarttab:
 *
 *  Copyright (C) 2008 Mark Atwood, Toru Maesaka
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
#include <drizzled/qcache.h>
#include <drizzled/gettext.h>

int qcache_initializer(st_plugin_int *plugin)
{
  qcache_t *p;

  p= new qcache_t;
  if (p == NULL) return 1;
  memset(p, 0, sizeof(qcache_t));

  plugin->data= (void *)p;

  if (plugin->plugin->init)
  {
    if (plugin->plugin->init((void *)p))
    {
      errmsg_printf(ERRMSG_LVL_ERROR, _("qcache plugin '%s' init() failed"),
                    plugin->name.str);
      goto err;
    }
  }

  return 0;

err:
  delete p;
  return 1;
}

int qcache_finalizer(st_plugin_int *plugin)
{
  qcache_t *p= (qcache_t *) plugin->data;

  if (plugin->plugin->deinit)
  {
    if (plugin->plugin->deinit((void *)p))
    {
      errmsg_printf(ERRMSG_LVL_ERROR, _("qcache plugin '%s' deinit() failed"),
                    plugin->name.str);
    }
  }

  if (p) delete p;
  return 0;
}

/*
  The plugin_foreach() iterator requires that we convert all the parameters
  of a plugin API entry point into a single void pointer, plus the session.
  So we need the following structure for qcache_invalidate_db() which requires
  multiple arguments.
*/
typedef struct db_invalidation_parms
{
  const char *dbname;
  bool transactional;
} db_invalidation_parms_t;


/*
  Following functions:

    qcache_try_fetch_and_send_iterate();
    qcache_set_iterate();
    qcache_invalidate_table_iterate();
    qcache_invalidate_db_iterate();
    qcache_flush_iterate();

  are called by plugin_foreach() _once_ for each Query Cache plugin.
*/

static bool qcache_try_fetch_and_send_iterate(Session *session, 
                                              plugin_ref plugin, void *p)
{
  qcache_t *l= plugin_data(plugin, qcache_t *);
  bool is_transactional = (bool *)p;

  if (l && l->qcache_try_fetch_and_send)
  {
    if (l->qcache_try_fetch_and_send(session, is_transactional))
    {
      errmsg_printf(ERRMSG_LVL_ERROR,
                    _("qcache plugin '%s' try_fetch_and_send() failed"),
                    (char *)plugin_name(plugin));
      return true;
    }
  }
  return false;
}

static bool qcache_set_iterate(Session *session, plugin_ref plugin, void *p)
{
  qcache_t *l = plugin_data(plugin, qcache_t *);
  bool transactional = (bool *)p;

  if (l && l->qcache_set)
  {
    if (l->qcache_set(session, transactional))
    {
      errmsg_printf(ERRMSG_LVL_ERROR, _("qcache plugin '%s' set() failed"),
                    (char *)plugin_name(plugin));
      return true;
    }
  }
  return false;
}

static bool qcache_invalidate_table_iterate(Session *session,
                                            plugin_ref plugin, void *p)
{
  qcache_t *l = plugin_data(plugin, qcache_t *);
  bool transactional = (bool *)p;

  if (l && l->qcache_invalidate_table)
  {
    if (l->qcache_invalidate_table(session, transactional))
    {
      errmsg_printf(ERRMSG_LVL_ERROR,
                    _("qcache plugin '%s' invalidate_table() failed"),
                    (char *)plugin_name(plugin));
      return true;
    }
  }
  return false;
}

static bool qcache_invalidate_db_iterate(Session *session,
                                         plugin_ref plugin, void *p)
{
  qcache_t *l = plugin_data(plugin, qcache_t *);
  db_invalidation_parms_t *parms = (db_invalidation_parms_t *)p;

  if (l && l->qcache_invalidate_db)
  {
    if (l->qcache_invalidate_db(session, parms->dbname, parms->transactional))
    {
      errmsg_printf(ERRMSG_LVL_ERROR,
                    _("qcache plugin '%s' invalidate_db() failed"),
                    (char *)plugin_name(plugin));
      return true;
    }
  }
  return false;
}

static bool qcache_flush_iterate(Session *session, plugin_ref plugin, void *p)
{
  qcache_t *l = plugin_data(plugin, qcache_t *);
  
  if (p) return true; /* flush has no parameters, return failure */

  if (l && l->qcache_flush)
  {
    if (l->qcache_flush(session))
    {
      errmsg_printf(ERRMSG_LVL_ERROR, _("qcache plugin '%s' flush() failed"),
                    (char *)plugin_name(plugin));
      return true;
    }
  }
  return false;
}


/*
  Following functions:

    drizzle_qcache_try_fetch_and_send();
    drizzle_qcache_set();
    drizzle_qcache_invalidate_table();
    drizzle_qcache_invalidate_db();
    drizzle_qcache_flush();

  are the entry points to the query cache plugin that is called by the
  rest of the Drizzle server code.
*/

bool drizzle_qcache_try_fetch_and_send(Session *session, bool transactional)
{
  bool foreach_rv;
  foreach_rv= plugin_foreach(session,
                             qcache_try_fetch_and_send_iterate,
                             DRIZZLE_QCACHE_PLUGIN,
                             (void *) &transactional);
  return foreach_rv;
}

bool drizzle_qcache_set(Session *session, bool transactional)
{
  bool foreach_rv;
  foreach_rv= plugin_foreach(session,
                             qcache_set_iterate,
                             DRIZZLE_QCACHE_PLUGIN,
                             (void *) &transactional);
  return foreach_rv;
}

bool drizzle_qcache_invalidate_table(Session *session, bool transactional)
{
  bool foreach_rv;
  foreach_rv= plugin_foreach(session,
                             qcache_invalidate_table_iterate,
                             DRIZZLE_QCACHE_PLUGIN,
                             (void *) &transactional);
  return foreach_rv;
}

bool drizzle_qcache_invalidate_db(Session *session, const char *dbname,
                                  bool transactional)
{
  bool foreach_rv;
  db_invalidation_parms_t parms;

  /* marshall the parameters */
  parms.dbname = dbname;
  parms.transactional = transactional;

  foreach_rv= plugin_foreach(session,
                             qcache_invalidate_db_iterate,
                             DRIZZLE_QCACHE_PLUGIN,
                             (void *) &parms);
  return foreach_rv;
}

bool drizzle_qcache_flush(Session *session)
{
  bool foreach_rv;
  foreach_rv= plugin_foreach(session,
                             qcache_flush_iterate,
                             DRIZZLE_QCACHE_PLUGIN,
                             NULL);
  return foreach_rv;
}
