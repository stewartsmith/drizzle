/* -*- mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; -*-
 *  vim:expandtab:shiftwidth=2:tabstop=2:smarttab:
 *
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

#include "config.h"

#include <string>
#include <vector>

#include "drizzled/session.h"
#include "drizzled/table_list.h"
#include "drizzled/table_share.h"
#include "drizzled/plugin/registry.h"
#include "drizzled/plugin/event_observer.h"


using namespace std;

namespace drizzled
{

namespace plugin
{

/* 
 * The base Event Observer class in which plugins register which events they
 * are interested in.
 */
class EventObserverList
{

public:
  
  void add_event_observer(multimap<unsigned int, EventObserver *> &observers, enum EventObserver::EventType event, EventObserver *eventObserver, int position)
  {
    unsigned int event_pos;
    
    if (position == 0)
      event_pos= INT32_MAX; // Set the event position to be in the middle.
    else
      event_pos= (unsigned int) position;
    
    /* If positioned then check if the position is already taken. */
    if (position) 
    {
      multimap<unsigned int, EventObserver *>::iterator it;
      
      if ( observers.find(event_pos) != observers.end() )
      {
        errmsg_printf(ERRMSG_LVL_WARN,
                    _("EventObserverList::add_event_observer() Duplicate event position %d for event '%s' from EventObserver plugin '%s'"),
                    position,
                    EventObserver::eventName(event), 
                    eventObserver->getName().c_str());
      }
    }
    
    observers.insert(pair<unsigned int, EventObserver *>(event_pos, eventObserver) );
  }

  virtual ~EventObserverList(){}
  
  /* Each event plugin registers itself with a shared table's event observer
   * for all events it is interested in regarding that table.
   */ 
  virtual bool add_observer(EventObserver *observer, enum EventObserver::EventType event, int position)= 0;
   
  /* Remove all observer from all lists. */
  virtual void clear_all_observers()= 0;  
  
   /* Get the observer list for an event type. Will return NULL if no observer exists.
    *
    * Note: Event observers are storted in a multimap object so that the order in which
    * they are called can be sorted based on the requested position. Lookups are never done
    * on the multimap, once filled it is used as a vector.
    */
  virtual multimap<unsigned int, EventObserver *> *getObservers(enum EventObserver::EventType event)= 0;
};


/*============================*/
static vector<EventObserver *> all_event_plugins;


//---------
bool EventObserver::addPlugin(EventObserver *handler)
{
  if (handler != NULL)
    all_event_plugins.push_back(handler);
  return false;
}

//---------
void EventObserver::removePlugin(EventObserver *handler)
{
  if (handler != NULL)
    all_event_plugins.erase(find(all_event_plugins.begin(), all_event_plugins.end(), handler));
}

//---------
void EventObserver::registerEvent(EventObserverList &observers, EventType event, int position)
{
  observers.add_observer(this, event, position);
}

/*========================================================*/
/*              Table Event Observer handling:            */
/*========================================================*/

/* Table event observer list: */
class TableEventObserverList: public EventObserverList
{
private:
  
  multimap<unsigned int, EventObserver *> pre_write_row_observers;
  multimap<unsigned int, EventObserver *> post_write_row_observers;
  multimap<unsigned int, EventObserver *> pre_update_row_observers;
  multimap<unsigned int, EventObserver *> post_update_row_observers;
  multimap<unsigned int, EventObserver *> pre_delete_row_observers;
  multimap<unsigned int, EventObserver *> post_delete_row_observers;
  
public:
  
  TableEventObserverList(){}
  ~TableEventObserverList(){}
  
/* Each event observer plugin registers itself with a shared table's event observer 
 * list for all events it is interested in regarding that table.
 */ 
  bool add_observer(EventObserver *observer, enum EventObserver::EventType event, int position)
  {
    switch(event) {
    case EventObserver::PRE_WRITE_ROW:
      add_event_observer(pre_write_row_observers, event, observer, position);
      break;
    
    case EventObserver::POST_WRITE_ROW:
      add_event_observer(post_write_row_observers, event, observer, position);
      break;
    
    case EventObserver::PRE_UPDATE_ROW:
      add_event_observer(pre_update_row_observers, event, observer, position);
      break;
    
    case EventObserver::POST_UPDATE_ROW:
      add_event_observer(post_update_row_observers, event, observer, position);
      break;
    
    case EventObserver::PRE_DELETE_ROW:
      add_event_observer(pre_delete_row_observers, event, observer, position);
      break;
    
    case EventObserver::POST_DELETE_ROW:
      add_event_observer(post_delete_row_observers, event, observer, position);
      break;
    
    default:
      errmsg_printf(ERRMSG_LVL_ERROR,
            _("TableEventObserverList::add_observer() Unsupported Event type '%s'"),
            EventObserver::eventName(event));
      return true;       
    }
    
    return false;       
  }

  
  /* Remove all observers from all lists. */
  void clear_all_observers()
  {
    pre_write_row_observers.clear();
    post_write_row_observers.clear();
    pre_update_row_observers.clear();
    post_update_row_observers.clear();
    pre_delete_row_observers.clear();
    post_delete_row_observers.clear();
  }
  
   /* Get the observer list for an event type. */
  multimap<unsigned int, EventObserver *> *getObservers(enum EventObserver::EventType event)
  {
    switch(event) {
    case EventObserver::PRE_WRITE_ROW:
      return (pre_write_row_observers.empty()) ? NULL: &pre_write_row_observers;
      
    case EventObserver::POST_WRITE_ROW:
      return (post_write_row_observers.empty()) ? NULL: &post_write_row_observers;
      
    case EventObserver::PRE_UPDATE_ROW:
      return (pre_update_row_observers.empty()) ? NULL: &pre_update_row_observers;
      
    case EventObserver::POST_UPDATE_ROW:
      return (post_update_row_observers.empty()) ? NULL: &post_update_row_observers;
      
    case EventObserver::PRE_DELETE_ROW:
      return (pre_delete_row_observers.empty()) ? NULL: &pre_delete_row_observers;
      
    case EventObserver::POST_DELETE_ROW:
      return (post_delete_row_observers.empty()) ? NULL: &post_delete_row_observers;
      
    default:
      errmsg_printf(ERRMSG_LVL_ERROR,
                  _("TableEventObserverList::getWatchers() Unsupported Event type '%s'"),
                  EventObserver::eventName(event));
    }
    
    return NULL;
  }

};

//----------
/* For each EventObserver plugin call its registerTableEvents() meathod so that it can
 * register what events, if any, it is interested in on this table.
 */ 
class registerTableEventsIterate : public unary_function<EventObserver *, void>
{
  TableShare &table_share;
  TableEventObserverList &observers;
  
public:
  registerTableEventsIterate(TableShare &table_share_arg, TableEventObserverList &observers_arg): 
    table_share(table_share_arg), observers(observers_arg) {}
  inline result_type operator() (argument_type eventObserver)
  {
    eventObserver->registerTableEvents(table_share, observers);
  }
};

//----------
/* 
 * registerTableEventsDo() is called by drizzle to register all plugins that
 * may be interested in table events on the newly created TableShare object.
 */ 
void EventObserver::registerTableEventsDo(TableShare &table_share)
{
  TableEventObserverList *observers;
  
  observers= table_share.getTableObservers();
  
  if (observers == NULL) {
    observers= new TableEventObserverList();
    table_share.setTableObservers(observers);
  }
  
  observers->clear_all_observers();
  
  for_each(all_event_plugins.begin(), all_event_plugins.end(),
           registerTableEventsIterate(table_share, *observers));
  
}

//----------
/* Cleanup before freeing the TableShare object. */
void EventObserver::deregisterTableEventsDo(TableShare &table_share)
{
  TableEventObserverList *observers;
  
  observers= table_share.getTableObservers();

  if (observers) {
    table_share.setTableObservers(NULL);
    observers->clear_all_observers();
    delete observers;
  }
}


/*========================================================*/
/*              Schema Event Observer handling:           */
/*========================================================*/
/* Schema event observer list: */
class SchemaEventObserverList: public EventObserverList
{
private:
  
  multimap<unsigned int, EventObserver *> pre_drop_table_observers;
  multimap<unsigned int, EventObserver *> post_drop_table_observers;
  multimap<unsigned int, EventObserver *> pre_rename_table_observers;
  multimap<unsigned int, EventObserver *> post_rename_table_observers;
  
public:
  
  SchemaEventObserverList(){}
  ~SchemaEventObserverList(){}
  
/* Each event plugin registers itself with a schema's event observer
 * for all events it is interested in regarding that schema.
 */ 
  bool add_observer(EventObserver *observer, enum EventObserver::EventType event, int position)
  {
    switch(event) {
    case EventObserver::PRE_DROP_TABLE:
      add_event_observer(pre_drop_table_observers, event, observer, position);
      break;
    
    case EventObserver::POST_DROP_TABLE:
      add_event_observer(post_drop_table_observers, event, observer, position);
      break;
    
    case EventObserver::PRE_RENAME_TABLE:
      add_event_observer(pre_rename_table_observers, event, observer, position);
      break;
      
    case EventObserver::POST_RENAME_TABLE:
      add_event_observer(post_rename_table_observers, event, observer, position);
      break;
      
    default:
      errmsg_printf(ERRMSG_LVL_ERROR,
                  _("SchemaEventObserverList::add_observer() Unsupported Event type '%s'"),
                  EventObserver::eventName(event));
      return true;       
    }
    
    return false;       
  }

  
  /* Remove all observers from all lists. */
  void clear_all_observers()
  {
    pre_drop_table_observers.clear();
    post_drop_table_observers.clear();
    pre_rename_table_observers.clear();
    post_rename_table_observers.clear();
  }
  
   /* Get the observer list for an event type. */
  multimap<unsigned int, EventObserver *> *getObservers(enum EventObserver::EventType event)
  {
    switch(event) {
    case EventObserver::PRE_DROP_TABLE:
      return (pre_drop_table_observers.empty()) ? NULL: &pre_drop_table_observers;
    
    case EventObserver::POST_DROP_TABLE:
      return (post_drop_table_observers.empty()) ? NULL: &post_drop_table_observers;
    
    case EventObserver::PRE_RENAME_TABLE:
      return (pre_rename_table_observers.empty()) ? NULL: &pre_rename_table_observers;
      
    case EventObserver::POST_RENAME_TABLE:
      return (post_rename_table_observers.empty()) ? NULL: &post_rename_table_observers;
      
    default:
      errmsg_printf(ERRMSG_LVL_ERROR,
                  _("SchemaEventObserverList::getObservers() Unknown EventObserver type '%d'"),
                  event);
    }
    
    return NULL;
  }

};

//----------
/* For each EventObserver plugin call its registerSchemaEvents() meathod so that it can
 * register what events, if any, it is interested in on the schema.
 */ 
class registerSchemaEventsIterate : public unary_function<EventObserver *, void>
{
  const std::string &db;
  SchemaEventObserverList &observers;
public:
  registerSchemaEventsIterate(const std::string &db_arg, SchemaEventObserverList &observers_arg) :     
    db(db_arg),
    observers(observers_arg){}
    
  inline result_type operator() (argument_type eventObserver)
  {
    eventObserver->registerSchemaEvents(db, observers);
  }
};

//----------
/* 
 * registerSchemaEventsDo() is called by drizzle to register all plugins that
 * may be interested in schema events on the database.
 */ 
void EventObserver::registerSchemaEventsDo(Session &session, const std::string &db)
{
  SchemaEventObserverList *observers;
  
  observers= session.getSchemaObservers(db);
  
  if (observers == NULL) 
  {
    observers= new SchemaEventObserverList();
    session.setSchemaObservers(db, observers);
  }
  
  for_each(all_event_plugins.begin(), all_event_plugins.end(),
           registerSchemaEventsIterate(db, *observers));
  
}

//----------
/* Cleanup before freeing the Session object. */
void EventObserver::deregisterSchemaEventsDo(Session &session, const std::string &db)
{
  SchemaEventObserverList *observers;
  
  observers= session.getSchemaObservers(db);

  if (observers) 
  {
    session.setSchemaObservers(db, NULL);
    observers->clear_all_observers();
    delete observers;
  }
}

/*========================================================*/
/*             Session Event Observer handling:           */
/*========================================================*/
/* Session event observer list: */
class SessionEventObserverList: public EventObserverList
{
private:
  
  multimap<unsigned int, EventObserver *> pre_create_db_observers;
  multimap<unsigned int, EventObserver *> post_create_db_observers;
  multimap<unsigned int, EventObserver *> pre_drop_db_observers;
  multimap<unsigned int, EventObserver *> post_drop_db_observers;
  
public:
  
  SessionEventObserverList(){}
  ~SessionEventObserverList(){}
  
/* Each event plugin registers itself with a shared table's event observer
 * for all events it is interested in regarding that table.
 */ 
  bool add_observer(EventObserver *observer, enum EventObserver::EventType event, int position)
  {
    switch(event) {
    case EventObserver::PRE_CREATE_DATABASE:
      add_event_observer(pre_create_db_observers, event, observer, position);
      break;
    
    case EventObserver::POST_CREATE_DATABASE:
      add_event_observer(post_create_db_observers, event, observer, position);
      break;

    case EventObserver::PRE_DROP_DATABASE:
      add_event_observer(pre_drop_db_observers, event, observer, position);
      break;
    
    case EventObserver::POST_DROP_DATABASE:
      add_event_observer(post_drop_db_observers, event, observer, position);
      break;

    default:
      errmsg_printf(ERRMSG_LVL_ERROR,
                  _("SessionEventObserverList::add_observer() Unsupported Event type '%s'"),
                  EventObserver::eventName(event));
      return true;       
    }
    
    return false;       
  }

  
  /* Remove all observers from all lists. */
  void clear_all_observers()
  {
    pre_create_db_observers.clear();
    post_create_db_observers.clear();
    pre_drop_db_observers.clear();
    post_drop_db_observers.clear();
  }
  
   /* Get the observer list for an event type. */
  multimap<unsigned int, EventObserver *> *getObservers(enum EventObserver::EventType event)
  {
    switch(event) {
    case EventObserver::PRE_CREATE_DATABASE:
      return (pre_create_db_observers.empty()) ? NULL: &pre_create_db_observers;
    
    case EventObserver::POST_CREATE_DATABASE:
      return (post_create_db_observers.empty()) ? NULL: &post_create_db_observers;
    
    case EventObserver::PRE_DROP_DATABASE:
      return (pre_drop_db_observers.empty()) ? NULL: &pre_drop_db_observers;
    
    case EventObserver::POST_DROP_DATABASE:
      return (post_drop_db_observers.empty()) ? NULL: &post_drop_db_observers;
    
   default:
      errmsg_printf(ERRMSG_LVL_ERROR,
                  _("SchemaEventObserverList::getObservers() Unknown Event type '%d'"),
                  event);
    }
    
    return NULL;
  }

};

//----------
/* For each EventObserver plugin call its registerSessionEvents() meathod so that it can
 * register what events, if any, it is interested in on this session.
 */ 
class registerSessionEventsIterate : public unary_function<EventObserver *, void>
{
  Session &session;
  SessionEventObserverList &observers;
public:
  registerSessionEventsIterate(Session &session_arg, SessionEventObserverList &observers_arg) : 
  session(session_arg), observers(observers_arg) {}
  inline result_type operator() (argument_type eventObserver)
  {
    eventObserver->registerSessionEvents(session, observers);
  }
};

//----------
/* 
 * registerSessionEventsDo() is called by drizzle to register all plugins that
 * may be interested in session events on the newly created session.
 */ 
void EventObserver::registerSessionEventsDo(Session &session)
{
  SessionEventObserverList *observers;
  
  observers= session.getSessionObservers();
  
  if (observers == NULL) {
    observers= new SessionEventObserverList();
    session.setSessionObservers(observers);
  }
  
  observers->clear_all_observers();
  
  for_each(all_event_plugins.begin(), all_event_plugins.end(),
           registerSessionEventsIterate(session, *observers));
  
}

//----------
/* Cleanup before freeing the session object. */
void EventObserver::deregisterSessionEventsDo(Session &session)
{
  SessionEventObserverList *observers;
  
  observers= session.getSessionObservers();

  if (observers) {
    session.setSessionObservers(NULL);
    observers->clear_all_observers();
    delete observers;
  }
}

  
/* Event observer list iterator: */
//----------
class EventIterate : public unary_function<pair<unsigned int, EventObserver *>, bool>
{
  EventData &data;
  
public:
  EventIterate(EventData &data_arg) :
    unary_function<pair<unsigned int, EventObserver *>, bool>(),
    data(data_arg)
    {}

  inline result_type operator()(argument_type handler)
  {
    bool result= handler.second->observeEvent(data);
    if (result)
    {
      /* TRANSLATORS: The leading word "EventObserver" is the name
         of the plugin api, and so should not be translated. */
      errmsg_printf(ERRMSG_LVL_ERROR,
                    _("EventIterate event handler '%s' failed for event '%s'"),
                    handler.second->getName().c_str(), handler.second->eventName(data.event));
      
    }
    return result;
  }
};


bool EventObserver::callEventObservers(EventData &data)
{
  multimap<unsigned int, EventObserver *> *observers= NULL;
 
  switch (data.event_classs) {
  case SessionEvent:
    {
      SessionEventData *session_data= (SessionEventData *)&data;
      observers= session_data->session.getSessionObservers()->getObservers(data.event);
    }
    break;
    
  case SchemaEvent: 
    {
      SchemaEventData *schema_data= (SchemaEventData *)&data;
      SchemaEventObserverList *schema_observers;
     
      /* Get the Schema event observers for the session.
       * If this is the first time a Schema event has occurred
       * for this database in this session then no observers
       * will be resistered yet in which case they must be resistered now.
       */
      std::string db(schema_data->db);
      schema_observers= schema_data->session.getSchemaObservers(db);
      if (!schema_observers) 
      {
        registerSchemaEventsDo(schema_data->session, db);
        schema_observers= schema_data->session.getSchemaObservers(db);
      }

      observers= schema_observers->getObservers(data.event);
    }
    break;
    
  case TableEvent:
    {
      TableEventData *table_data= (TableEventData *)&data;
      observers= table_data->table.getTableObservers()->getObservers(data.event);
    }
    break;
  }
  
  if (observers == NULL)
    return false; // Nobody was interested in the event. :(

  /* Use find_if instead of foreach so that we can collect return codes */
  multimap<unsigned int, EventObserver *>::iterator iter=
    find_if(observers->begin(), observers->end(),
            EventIterate(data)); 
  /* If iter is == end() here, that means that all of the plugins returned
   * false, which in this case means they all succeeded. Since we want to 
   * return false on success, we return the value of the two being !=.
   */
  return iter != observers->end();
}

/*==========================================================*/
/* Static meathods called by drizzle to notify interested plugins 
 * of a schema event,
 */
bool EventObserver::preDropTableDo(Session &session, TableIdentifier &table)
{
  PreDropTableEventData eventData(session, table);
  return callEventObservers(eventData);
}

bool EventObserver::postDropTableDo(Session &session, TableIdentifier &table, int err)
{
  PostDropTableEventData eventData(session, table, err);
  return callEventObservers(eventData);
}

bool EventObserver::preRenameTableDo(Session &session, TableIdentifier &from, TableIdentifier &to)
{
  PreRenameTableEventData eventData(session, from, to);
  return callEventObservers(eventData);
}

bool EventObserver::postRenameTableDo(Session &session, TableIdentifier &from, TableIdentifier &to, int err)
{
  PostRenameTableEventData eventData(session, from, to, err);
  return callEventObservers(eventData);
}

/*==========================================================*/
/* Static meathods called by drizzle to notify interested plugins 
 * of a table event,
 */
bool EventObserver::preWriteRowDo(Session &session, TableShare &table_share, unsigned char *buf)
{
  PreWriteRowEventData eventData(session, table_share, buf);
  return callEventObservers(eventData);
}

bool EventObserver::postWriteRowDo(Session &session, TableShare &table_share, const unsigned char *buf, int err)
{
  PostWriteRowEventData eventData(session, table_share, buf, err);
  return callEventObservers(eventData);
}

bool EventObserver::preDeleteRowDo(Session &session, TableShare &table_share, const unsigned char *buf)
{
  PreDeleteRowEventData eventData(session, table_share, buf);
  return callEventObservers(eventData);
}

bool EventObserver::postDeleteRowDo(Session &session, TableShare &table_share, const unsigned char *buf, int err)
{
  PostDeleteRowEventData eventData(session, table_share, buf, err);
  return callEventObservers(eventData);
}

bool EventObserver::preUpdateRowDo(Session &session, TableShare &table_share, const unsigned char *old_data, unsigned char *new_data)
{
  PreUpdateRowEventData eventData(session, table_share, old_data, new_data);
  return callEventObservers(eventData);
}

bool EventObserver::postUpdateRowDo(Session &session, TableShare &table_share, const unsigned char *old_data, unsigned char *new_data, int err)
{
  PostUpdateRowEventData eventData(session, table_share, old_data, new_data, err);
  return callEventObservers(eventData);
}

/*==========================================================*/
/* Static meathods called by drizzle to notify interested plugins 
 * of a session event,
 */
bool EventObserver::preCreateDatabaseDo(Session &session, const std::string &db)
{
  PreCreateDatabaseEventData eventData(session, db);
  return callEventObservers(eventData);
}

bool EventObserver::postCreateDatabaseDo(Session &session, const std::string &db, int err)
{
  PostCreateDatabaseEventData eventData(session, db, err);
  return callEventObservers(eventData);
}

bool EventObserver::preDropDatabaseDo(Session &session, const std::string &db)
{
  PreDropDatabaseEventData eventData(session, db);
  return callEventObservers(eventData);
}

bool EventObserver::postDropDatabaseDo(Session &session, const std::string &db, int err)
{
  PostDropDatabaseEventData eventData(session, db, err);
  return callEventObservers(eventData);
}


} /* namespace plugin */
} /* namespace drizzled */
