/*
 *  Copyright (C) 2010 PrimeBase Technologies GmbH, Germany
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

/**
 * @details
 *
 * This plugin is an example events plugin that just prints some info for
 * the events that it is tracking. 
 *  
set global hello_events_enable = ON;
set global hello_events_watch_databases = "x";   
set global hello_events_watch_tables = "x,y";

 */

#include "config.h"
#include <string>

#include "drizzled/session.h"
#include "hello_events.h"

using namespace drizzled;
using namespace plugin;
using namespace std;

#define PLUGIN_NAME "hello_events1"

static bool sysvar_hello_events_enabled= true;
static HelloEvents *hello_events= NULL;
static char *sysvar_table_list= NULL;
static char *sysvar_db_list= NULL;

/*
 * Event observer positions are used to set the order in which
 * event observers are called in the case that more than one
 * plugin is interested in the same event. You should only specify
 * the order if it really matters because if more than one plugin 
 * request the same calling position only the first one gets it and
 * the others will not be registered for the event. For this reason
 * your plugin should always provide a way to reposition the event
 * observer to resolve such conflicts.
 *
 * If position matters you will always initialy ask for the first position (1)
 * or the last position (-1) in the calling order, for example it makes no sence 
 * to initially ask to be called in position 13.
 */
static int sysvar_pre_write_position= 1;      // Call this event observer first.
static int sysvar_pre_update_position= 1;
static int sysvar_post_drop_db_position= -1;  // I want my event observer to be called last. No reason, I just do!


//==================================
// My table event observers: 
static bool preWriteRow(PreWriteRowEventData &data)
{

  fprintf(stderr, PLUGIN_NAME" EVENT preWriteRow(%s)\n", data.table.getTableName());
  return false;
}

//---
static void postWriteRow(PostWriteRowEventData &data)
{
  fprintf(stderr, PLUGIN_NAME" EVENT postWriteRow(%s) err = %d\n", data.table.getTableName(), data.err);
}

//---
static bool preDeleteRow(PreDeleteRowEventData &data)
{
  fprintf(stderr, PLUGIN_NAME" EVENT preDeleteRow(%s)\n", data.table.getTableName());
  return false;
}

//---
static void postDeleteRow(PostDeleteRowEventData &data)
{
  fprintf(stderr, PLUGIN_NAME" EVENT postDeleteRow(%s) err = %d\n", data.table.getTableName(), data.err);
}

//---
static bool preUpdateRow(PreUpdateRowEventData &data)
{
  fprintf(stderr, PLUGIN_NAME" EVENT preUpdateRow(%s)\n", data.table.getTableName());
  return false;
}

//---
static void postUpdateRow(PostUpdateRowEventData &data)
{
  fprintf(stderr, PLUGIN_NAME" EVENT postUpdateRow(%s) err = %d\n", data.table.getTableName(), data.err);
}

//==================================
// My schema event observers: 
static void postDropTable(PostDropTableEventData &data)
{
  fprintf(stderr, PLUGIN_NAME" EVENT postDropTable(%s) err = %d\n", data.table.getTableName().c_str(), data.err);
}

//---
static void postRenameTable(PostRenameTableEventData &data)
{
  fprintf(stderr, PLUGIN_NAME" EVENT postRenameTable(%s, %s) err = %d\n", data.from.getTableName().c_str(), data.to.getTableName().c_str(), data.err);
}

//---
static void postCreateDatabase(PostCreateDatabaseEventData &data)
{
  fprintf(stderr, PLUGIN_NAME" EVENT postCreateDatabase(%s) err = %d\n", data.db.c_str(), data.err);
}

//---
static void postDropDatabase(PostDropDatabaseEventData &data)
{
  fprintf(stderr, PLUGIN_NAME" EVENT postDropDatabase(%s) err = %d\n", data.db.c_str(), data.err);
}

//==================================
/* This is where I register which table events my pluggin is interested in.*/
void HelloEvents::registerTableEvents(TableShare &table_share, EventObserverList &observers)
{
  if ((is_enabled == false) 
    || !isTableInteresting(table_share.getTableName())
    || !isDatabaseInteresting(table_share.getSchemaName()))
    return;
    
  registerEvent(observers, PRE_WRITE_ROW, sysvar_pre_write_position); // I want to be called first if passible
  registerEvent(observers, POST_WRITE_ROW);
  registerEvent(observers, PRE_UPDATE_ROW, sysvar_pre_update_position);
  registerEvent(observers, POST_UPDATE_ROW);
  registerEvent(observers, PRE_DELETE_ROW);
  registerEvent(observers, POST_DELETE_ROW);
}

//==================================
/* This is where I register which schema events my pluggin is interested in.*/
void HelloEvents::registerSchemaEvents(const std::string &db, EventObserverList &observers)
{
  if ((is_enabled == false) 
    || !isDatabaseInteresting(db))
    return;
    
  registerEvent(observers, POST_DROP_TABLE);
  registerEvent(observers, POST_RENAME_TABLE);
}

//==================================
/* This is where I register which session events my pluggin is interested in.*/
void HelloEvents::registerSessionEvents(Session &session, EventObserverList &observers)
{
  if ((is_enabled == false) 
    || !isSessionInteresting(session))
    return;
    
  registerEvent(observers, POST_CREATE_DATABASE);
  registerEvent(observers, POST_DROP_DATABASE, sysvar_post_drop_db_position);
}


//==================================
/* The event observer.*/
bool HelloEvents::observeEvent(EventData &data)
{
  bool result= false;
  
  switch (data.event) {
  case POST_DROP_TABLE:
    postDropTable((PostDropTableEventData &)data);
    break;
    
  case POST_RENAME_TABLE:
    postRenameTable((PostRenameTableEventData &)data);
    break;
    
  case PRE_WRITE_ROW:
     result = preWriteRow((PreWriteRowEventData &)data);
    break;
    
  case POST_WRITE_ROW:
    postWriteRow((PostWriteRowEventData &)data);
    break;     
       
  case PRE_UPDATE_ROW:
    result = preUpdateRow((PreUpdateRowEventData &)data);
    break;
             
  case POST_UPDATE_ROW:
     postUpdateRow((PostUpdateRowEventData &)data);
    break;     
    
  case PRE_DELETE_ROW:
    result = preDeleteRow((PreDeleteRowEventData &)data);
    break;

  case POST_DELETE_ROW:
    postDeleteRow((PostDeleteRowEventData &)data);
    break;

  case POST_CREATE_DATABASE:
    postCreateDatabase((PostCreateDatabaseEventData &)data);
    break;

  case POST_DROP_DATABASE:
    postDropDatabase((PostDropDatabaseEventData &)data);
    break;

  default:
    fprintf(stderr, "HelloEvents: Unexpected event '%s'\n", EventObserver::eventName(data.event));
 
  }
  
  return false;
}

//==================================
// Some custom things for my plugin:


/* Plugin initialization and system variables */

static void enable(Session *,
                   drizzle_sys_var *,
                   void *var_ptr,
                   const void *save)
{
  if (hello_events)
  {
    if (*(bool *)save != false)
    {
      hello_events->enable();
      *(bool *) var_ptr= (bool) true;
    }
    else
    {
      hello_events->disable();
      *(bool *) var_ptr= (bool) false;
    }
  }
}


static void set_db_list(Session *,
                   drizzle_sys_var *, 
                   void *var_ptr,     
                   const void *save)
{
  if (hello_events)
  {
    hello_events->setDatabasesOfInterest(*(const char **) save);
    *(const char **) var_ptr= hello_events->getDatabasesOfInterest();
  }
}

static void set_table_list(Session *,
                   drizzle_sys_var *, 
                   void *var_ptr,     
                   const void *save)
{
  if (hello_events)
  {
    hello_events->setTablesOfInterest(*(const char **) save);
    *(const char **) var_ptr= hello_events->getTablesOfInterest();
  }
}


static int init(Context &context)
{
  hello_events= new HelloEvents(PLUGIN_NAME);

  context.add(hello_events);

  if (sysvar_hello_events_enabled)
  {
    hello_events->enable();
  }

  return 0;
}

static DRIZZLE_SYSVAR_STR(watch_databases,
                           sysvar_db_list,
                           PLUGIN_VAR_OPCMDARG,
                           N_("A comma delimited list of databases to watch"),
                           NULL, /* check func */
                           set_db_list, /* update func */
                           "" /* default */);

static DRIZZLE_SYSVAR_STR(watch_tables,
                           sysvar_table_list,
                           PLUGIN_VAR_OPCMDARG,
                           N_("A comma delimited list of tables to watch"),
                           NULL, /* check func */
                           set_table_list, /* update func */
                           "" /* default */);

static DRIZZLE_SYSVAR_BOOL(enable,
                           sysvar_hello_events_enabled,
                           PLUGIN_VAR_NOCMDARG,
                           N_("Enable Example Events Plugin"),
                           NULL, /* check func */
                           enable, /* update func */
                           false /* default */);

static DRIZZLE_SYSVAR_INT(pre_write_position,
                           sysvar_pre_write_position,
                           PLUGIN_VAR_NOCMDARG,
                           N_("Pre write row event observer call position"),
                           NULL, /* check func */
                           NULL, /* update func */
                           1, /* default */
                           1, /* min */
                           INT32_MAX -1, /* max */
                           0 /* blk */);

static DRIZZLE_SYSVAR_INT(pre_update_position,
                           sysvar_pre_update_position,
                           PLUGIN_VAR_NOCMDARG,
                           N_("Pre update row event observer call position"),
                           NULL, /* check func */
                           NULL, /* update func */
                           1, /* default */
                           1, /* min */
                           INT32_MAX -1, /* max */
                           0 /* blk */);

static DRIZZLE_SYSVAR_INT(post_drop_db_position,
                           sysvar_post_drop_db_position,
                           PLUGIN_VAR_NOCMDARG,
                           N_("Post drop database event observer call position"),
                           NULL, /* check func */
                           NULL, /* update func */
                           -1, /* default */
                           INT32_MAX +1, /* min */
                           -1, /* max */
                           0 /* blk */);

static drizzle_sys_var* system_var[]= {
  DRIZZLE_SYSVAR(watch_databases),
  DRIZZLE_SYSVAR(watch_tables),
  DRIZZLE_SYSVAR(enable),
  DRIZZLE_SYSVAR(pre_write_position),
  DRIZZLE_SYSVAR(pre_update_position),
  DRIZZLE_SYSVAR(post_drop_db_position),
  NULL
};

DRIZZLE_DECLARE_PLUGIN
{
  DRIZZLE_VERSION_ID,
  PLUGIN_NAME,
  "0.1",
  "Barry Leslie",
  N_("An example events Plugin"),
  PLUGIN_LICENSE_BSD,
  init,   /* Plugin Init      */
  system_var, /* system variables */
  NULL    /* config options   */
}
DRIZZLE_DECLARE_PLUGIN_END;
