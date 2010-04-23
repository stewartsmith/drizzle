/*
 * Copyright (c) 2010, Joseph Daly <skinny.moey@gmail.com>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 *   * Redistributions of source code must retain the above copyright notice,
 *     this list of conditions and the following disclaimer.
 *   * Redistributions in binary form must reproduce the above copyright notice,
 *     this list of conditions and the following disclaimer in the documentation
 *     and/or other materials provided with the distribution.
 *   * Neither the name of Joseph Daly nor the names of its contributors
 *     may be used to endorse or promote products derived from this software
 *     without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
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

#include <string>

#include "config.h"
#include "drizzled/session.h"
#include "hello_events.h"

using namespace drizzled;
using namespace plugin;
using namespace std;

static bool sysvar_hello_events_enabled= true;
static HelloEvents *hello_events= NULL;
static char *sysvar_table_list= NULL;
static char *sysvar_db_list= NULL;


//==================================
// My table event observers: 
static bool preWriteRow(PreWriteRowEventData *data)
{

  fprintf(stderr, "EVENT preWriteRow(%s)\n", data->table->getTableName());
  return false;
}

//---
static void postWriteRow(PostWriteRowEventData *data)
{
  fprintf(stderr, "EVENT postWriteRow(%s) err = %d\n", data->table->getTableName(), data->err);
}

//---
static bool preDeleteRow(PreDeleteRowEventData *data)
{
  fprintf(stderr, "EVENT preDeleteRow(%s)\n", data->table->getTableName());
  return false;
}

//---
static void postDeleteRow(PostDeleteRowEventData *data)
{
  fprintf(stderr, "EVENT postDeleteRow(%s) err = %d\n", data->table->getTableName(), data->err);
}

//---
static bool preUpdateRow(PreUpdateRowEventData *data)
{
  fprintf(stderr, "EVENT preUpdateRow(%s)\n", data->table->getTableName());
  return false;
}

//---
static void postUpdateRow(PostUpdateRowEventData *data)
{
  fprintf(stderr, "EVENT postUpdateRow(%s) err = %d\n", data->table->getTableName(), data->err);
}

//==================================
// My schema event observers: 
static void postDropTable(PostDropTableEventData *data)
{
  fprintf(stderr, "EVENT postDropTable(%s) err = %d\n", data->table->getTableName().c_str(), data->err);
}

//---
static void postRenameTable(PostRenameTableEventData *data)
{
  fprintf(stderr, "EVENT postRenameTable(%s, %s) err = %d\n", data->from->getTableName().c_str(), data->to->getTableName().c_str(), data->err);
}

//---
static void postCreateDatabase(PostCreateDatabaseEventData *data)
{
  fprintf(stderr, "EVENT postCreateDatabase(%s) err = %d\n", data->db, data->err);
}

//---
static void postDropDatabase(PostDropDatabaseEventData *data)
{
  fprintf(stderr, "EVENT postDropDatabase(%s) err = %d\n", data->db, data->err);
}

//==================================
/* This is where I register which table events my pluggin is interested in.*/
void HelloEvents::registerTableEvents(TableShare *table_share, EventObservers *observers)
{
  if ((!is_enabled) || (table_share == NULL) 
    || !isTableInteresting(table_share->getTableName())
    || !isDatabaseInteresting(table_share->getSchemaName()))
    return;
    
  registerEvent(observers, PRE_WRITE_ROW);
  registerEvent(observers, POST_WRITE_ROW);
  registerEvent(observers, PRE_UPDATE_ROW);
  registerEvent(observers, POST_UPDATE_ROW);
  registerEvent(observers, PRE_DELETE_ROW);
  registerEvent(observers, POST_DELETE_ROW);
}

//==================================
/* This is where I register which schema events my pluggin is interested in.*/
void HelloEvents::registerSchemaEvents(const std::string *db, EventObservers *observers)
{
  if ((!is_enabled) 
    || !isDatabaseInteresting(db->c_str()))
    return;
    
  registerEvent(observers, POST_DROP_TABLE);
  registerEvent(observers, POST_RENAME_TABLE);
}

//==================================
/* This is where I register which session events my pluggin is interested in.*/
void HelloEvents::registerSessionEvents(Session *session, EventObservers *observers)
{
  if ((!is_enabled) 
    || !isSessionInteresting(session))
    return;
    
  registerEvent(observers, POST_CREATE_DATABASE);
  registerEvent(observers, POST_DROP_DATABASE);
}


//==================================
/* The event observer.*/
bool HelloEvents::observeEvent(EventData *data)
{
  bool result = false;
  
  switch (data->event)
  {
      case POST_DROP_TABLE:
        postDropTable((PostDropTableEventData *)data);
        break;
        
      case POST_RENAME_TABLE:
        postRenameTable((PostRenameTableEventData *)data);
        break;
        
      case PRE_WRITE_ROW:
         result = preWriteRow((PreWriteRowEventData *)data);
        break;
        
      case POST_WRITE_ROW:
        postWriteRow((PostWriteRowEventData *)data);
        break;     
           
      case PRE_UPDATE_ROW:
        result = preUpdateRow((PreUpdateRowEventData *)data);
        break;
                 
      case POST_UPDATE_ROW:
         postUpdateRow((PostUpdateRowEventData *)data);
        break;     
        
      case PRE_DELETE_ROW:
        result = preDeleteRow((PreDeleteRowEventData *)data);
        break;

      case POST_DELETE_ROW:
        postDeleteRow((PostDeleteRowEventData *)data);
        break;

      case POST_CREATE_DATABASE:
        postCreateDatabase((PostCreateDatabaseEventData *)data);
        break;

      case POST_DROP_DATABASE:
        postDropDatabase((PostDropDatabaseEventData *)data);
        break;

      default:
        fprintf(stderr, "HelloEvents: Unexpected event '%s'\n", Event::eventName(data->event));
 
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
  hello_events= new HelloEvents("hello_events");

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

static drizzle_sys_var* system_var[]= {
  DRIZZLE_SYSVAR(watch_databases),
  DRIZZLE_SYSVAR(watch_tables),
  DRIZZLE_SYSVAR(enable),
  NULL
};

DRIZZLE_DECLARE_PLUGIN
{
  DRIZZLE_VERSION_ID,
  "hello_events",
  "0.1",
  "Barry Leslie",
  N_("An example events Plugin"),
  PLUGIN_LICENSE_BSD,
  init,   /* Plugin Init      */
  system_var, /* system variables */
  NULL    /* config options   */
}
DRIZZLE_DECLARE_PLUGIN_END;
