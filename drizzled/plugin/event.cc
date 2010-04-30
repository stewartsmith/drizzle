/* -*- mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; -*-
 *  vim:expandtab:shiftwidth=2:tabstop=2:smarttab:
 *
 *  Copyright (C) 2008 Sun Microsystems
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
#include "drizzled/plugin/event.h"


using namespace std;

namespace drizzled
{

namespace plugin
{

/* 
 * The base Event Observer class in which plugins register which events they
 * are interested in.
 */
class EventObservers
{

public:
  
  void add_event(multimap<unsigned int, Event *> &observers, enum Event::EventType event, Event *eventObserver, int position)
  {
    unsigned int event_pos;
    
    if (position == 0)
      event_pos= INT32_MAX; // Set the event position to be in the middle.
    else
      event_pos= (unsigned int) position;
    
    /* If positioned then check if the position is already taken. */
    if (position) 
    {
      multimap<unsigned int, Event *>::iterator it;
      
      if ( observers.find(event_pos) != observers.end() )
      {
        errmsg_printf(ERRMSG_LVL_WARN,
                    _("EventObservers::add_event() Duplicate event position %d for event '%s' from Event plugin '%s'"),
                    position,
                    Event::eventName(event), 
                    eventObserver->getName().c_str());
      }
    }
    
    observers.insert(pair<unsigned int, Event *>(event_pos, eventObserver) );
  }

  virtual ~EventObservers(){}
  
/* Each event plugin registers itself with a shared table's event observer
 * for all events it is interested in regarding that table.
 */ 
  virtual bool add_observer(Event *observer, enum Event::EventType event, int position)= 0;
   
  /* Remove all observer from all lists. */
  virtual void clear_all_observers()= 0;  
  
   /* Get the observer list for an event type. */
  virtual multimap<unsigned int, Event *> *getObservers(enum Event::EventType event)= 0;
};


/*============================*/
static vector<Event *> all_event_plugins;


//---------
bool Event::addPlugin(Event *handler)
{

  if (handler != NULL)
    all_event_plugins.push_back(handler);
  return false;
}

//---------
void Event::removePlugin(Event *handler)
{
  if (handler != NULL)
    all_event_plugins.erase(find(all_event_plugins.begin(), all_event_plugins.end(), handler));
}

//---------
void Event::registerEvent(EventObservers *observers, EventType event, int position)
{
  observers->add_observer(this, event, position);
}

/*========================================================*/
/*              Table Event Observer handling:            */
/*========================================================*/

/* Table event Observers: */
class TableEventObservers: public EventObservers
{
private:
  
  multimap<unsigned int, Event *> pre_write_row_observers;
  multimap<unsigned int, Event *> post_write_row_observers;
  multimap<unsigned int, Event *> pre_update_row_observers;
  multimap<unsigned int, Event *> post_update_row_observers;
  multimap<unsigned int, Event *> pre_delete_row_observers;
  multimap<unsigned int, Event *> post_delete_row_observers;
  
public:
  
  TableEventObservers(){}
  ~TableEventObservers(){}
  
/* Each event plugin registers itself with a shared table's event observer
 * for all events it is interested in regarding that table.
 */ 
  bool add_observer(Event *observer, enum Event::EventType event, int position)
  {
    switch(event) {
    case Event::PRE_WRITE_ROW:
      add_event(pre_write_row_observers, event, observer, position);
      break;
    
    case Event::POST_WRITE_ROW:
      add_event(post_write_row_observers, event, observer, position);
      break;
    
    case Event::PRE_UPDATE_ROW:
      add_event(pre_update_row_observers, event, observer, position);
      break;
    
    case Event::POST_UPDATE_ROW:
      add_event(post_update_row_observers, event, observer, position);
      break;
    
    case Event::PRE_DELETE_ROW:
      add_event(pre_delete_row_observers, event, observer, position);
      break;
    
    case Event::POST_DELETE_ROW:
      add_event(post_delete_row_observers, event, observer, position);
      break;
    
    default:
      errmsg_printf(ERRMSG_LVL_ERROR,
            _("TableEventObservers::add_observer() Unsupported Event type '%s'"),
            Event::eventName(event));
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
  multimap<unsigned int, Event *> *getObservers(enum Event::EventType event)
  {
    switch(event) {
    case Event::PRE_WRITE_ROW:
      return (pre_write_row_observers.empty()) ? NULL: &pre_write_row_observers;
      
    case Event::POST_WRITE_ROW:
      return (post_write_row_observers.empty()) ? NULL: &post_write_row_observers;
      
    case Event::PRE_UPDATE_ROW:
      return (pre_update_row_observers.empty()) ? NULL: &pre_update_row_observers;
      
    case Event::POST_UPDATE_ROW:
      return (post_update_row_observers.empty()) ? NULL: &post_update_row_observers;
      
    case Event::PRE_DELETE_ROW:
      return (pre_delete_row_observers.empty()) ? NULL: &pre_delete_row_observers;
      
    case Event::POST_DELETE_ROW:
      return (post_delete_row_observers.empty()) ? NULL: &post_delete_row_observers;
      
    default:
      errmsg_printf(ERRMSG_LVL_ERROR,
                  _("TableEventObservers::getWatchers() Unsupported Event type '%s'"),
                  Event::eventName(event));
    }
    
    return NULL;
  }

};

//----------
/* For each Event plugin call its registerTableEvents() meathod so that it can
 * register what events, if any, it is interested in on this table.
 */ 
class registerTableEventsIterate : public unary_function<Event *, void>
{
  TableShare *table_share;
  TableEventObservers *observers;
  
public:
  registerTableEventsIterate(TableShare *table_share_arg, TableEventObservers *observers_arg): 
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
void Event::registerTableEventsDo(TableShare *table_share)
{
  TableEventObservers *observers;
  
  observers= table_share->getTableObservers();
  
  if (observers == NULL) {
    observers= new TableEventObservers();
    table_share->setTableObservers(observers);
  }
  
  observers->clear_all_observers();
  
  for_each(all_event_plugins.begin(), all_event_plugins.end(),
           registerTableEventsIterate(table_share, observers));
  
}

//----------
/* Cleanup before freeing the TableShare object. */
void Event::deregisterTableEventsDo(TableShare *table_share)
{
  TableEventObservers *observers;
  
  observers= table_share->getTableObservers();

  if (observers) {
    table_share->setTableObservers(NULL);
    observers->clear_all_observers();
    delete observers;
  }
}


/*========================================================*/
/*              Schema Event Observer handling:           */
/*========================================================*/
/* Schema event Observers: */
class SchemaEventObservers: public EventObservers
{
private:
  
  multimap<unsigned int, Event *> pre_drop_table_observers;
  multimap<unsigned int, Event *> post_drop_table_observers;
  multimap<unsigned int, Event *> pre_rename_table_observers;
  multimap<unsigned int, Event *> post_rename_table_observers;
  
public:
  
  SchemaEventObservers(){}
  ~SchemaEventObservers(){}
  
/* Each event plugin registers itself with a schema's event observer
 * for all events it is interested in regarding that schema.
 */ 
  bool add_observer(Event *observer, enum Event::EventType event, int position)
  {
    switch(event) {
    case Event::PRE_DROP_TABLE:
      add_event(pre_drop_table_observers, event, observer, position);
      break;
    
    case Event::POST_DROP_TABLE:
      add_event(post_drop_table_observers, event, observer, position);
      break;
    
    case Event::PRE_RENAME_TABLE:
      add_event(pre_rename_table_observers, event, observer, position);
      break;
      
    case Event::POST_RENAME_TABLE:
      add_event(post_rename_table_observers, event, observer, position);
      break;
      
    default:
      errmsg_printf(ERRMSG_LVL_ERROR,
                  _("SchemaEventObservers::add_observer() Unsupported Event type '%s'"),
                  Event::eventName(event));
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
  multimap<unsigned int, Event *> *getObservers(enum Event::EventType event)
  {
    switch(event) {
    case Event::PRE_DROP_TABLE:
      return (pre_drop_table_observers.empty()) ? NULL: &pre_drop_table_observers;
    
    case Event::POST_DROP_TABLE:
      return (post_drop_table_observers.empty()) ? NULL: &post_drop_table_observers;
    
    case Event::PRE_RENAME_TABLE:
      return (pre_rename_table_observers.empty()) ? NULL: &pre_rename_table_observers;
      
    case Event::POST_RENAME_TABLE:
      return (post_rename_table_observers.empty()) ? NULL: &post_rename_table_observers;
      
    default:
      errmsg_printf(ERRMSG_LVL_ERROR,
                  _("SchemaEventObservers::getObservers() Unknown Event type '%d'"),
                  event);
    }
    
    return NULL;
  }

};

//----------
/* For each Event plugin call its registerSchemaEvents() meathod so that it can
 * register what events, if any, it is interested in on the schema.
 */ 
class registerSchemaEventsIterate : public unary_function<Event *, void>
{
  const std::string *db;
  SchemaEventObservers *observers;
public:
  registerSchemaEventsIterate(const std::string *db_arg, SchemaEventObservers *observers_arg) :     
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
void Event::registerSchemaEventsDo(Session *session, const std::string *db)
{
  SchemaEventObservers *observers;
  
  observers= session->getSchemaObservers(db);
  
  if (observers == NULL) 
  {
    observers= new SchemaEventObservers();
    session->setSchemaObservers(db, observers);
  }
  
  for_each(all_event_plugins.begin(), all_event_plugins.end(),
           registerSchemaEventsIterate(db, observers));
  
}

//----------
/* Cleanup before freeing the Session object. */
void Event::deregisterSchemaEventsDo(Session *session, const std::string *db)
{
  SchemaEventObservers *observers;
  
  observers= session->getSchemaObservers(db);

  if (observers) 
  {
    session->setSchemaObservers(db, NULL);
    observers->clear_all_observers();
    delete observers;
  }
}

/*========================================================*/
/*             Session Event Observer handling:           */
/*========================================================*/
/* Session event Observers: */
class SessionEventObservers: public EventObservers
{
private:
  
  multimap<unsigned int, Event *> pre_create_db_observers;
  multimap<unsigned int, Event *> post_create_db_observers;
  multimap<unsigned int, Event *> pre_drop_db_observers;
  multimap<unsigned int, Event *> post_drop_db_observers;
  
public:
  
  SessionEventObservers(){}
  ~SessionEventObservers(){}
  
/* Each event plugin registers itself with a shared table's event observer
 * for all events it is interested in regarding that table.
 */ 
  bool add_observer(Event *observer, enum Event::EventType event, int position)
  {
    switch(event) {
    case Event::PRE_CREATE_DATABASE:
      add_event(pre_create_db_observers, event, observer, position);
      break;
    
    case Event::POST_CREATE_DATABASE:
      add_event(post_create_db_observers, event, observer, position);
      break;

    case Event::PRE_DROP_DATABASE:
      add_event(pre_drop_db_observers, event, observer, position);
      break;
    
    case Event::POST_DROP_DATABASE:
      add_event(post_drop_db_observers, event, observer, position);
      break;

    default:
      errmsg_printf(ERRMSG_LVL_ERROR,
                  _("SessionEventObservers::add_observer() Unsupported Event type '%s'"),
                  Event::eventName(event));
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
  multimap<unsigned int, Event *> *getObservers(enum Event::EventType event)
  {
    switch(event) {
    case Event::PRE_CREATE_DATABASE:
      return (pre_create_db_observers.empty()) ? NULL: &pre_create_db_observers;
    
    case Event::POST_CREATE_DATABASE:
      return (post_create_db_observers.empty()) ? NULL: &post_create_db_observers;
    
    case Event::PRE_DROP_DATABASE:
      return (pre_drop_db_observers.empty()) ? NULL: &pre_drop_db_observers;
    
    case Event::POST_DROP_DATABASE:
      return (post_drop_db_observers.empty()) ? NULL: &post_drop_db_observers;
    
   default:
      errmsg_printf(ERRMSG_LVL_ERROR,
                  _("SchemaEventObservers::getObservers() Unknown Event type '%d'"),
                  event);
    }
    
    return NULL;
  }

};

//----------
/* For each Event plugin call its registerSessionEvents() meathod so that it can
 * register what events, if any, it is interested in on this session.
 */ 
class registerSessionEventsIterate : public unary_function<Event *, void>
{
  Session *session;
  SessionEventObservers *observers;
public:
  registerSessionEventsIterate(Session *session_arg, SessionEventObservers *observers_arg) : 
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
void Event::registerSessionEventsDo(Session *session)
{
  SessionEventObservers *observers;
  
  observers= session->getSessionObservers();
  
  if (observers == NULL) {
    observers= new SessionEventObservers();
    session->setSessionObservers(observers);
  }
  
  observers->clear_all_observers();
  
  for_each(all_event_plugins.begin(), all_event_plugins.end(),
           registerSessionEventsIterate(session, observers));
  
}

//----------
/* Cleanup before freeing the session object. */
void Event::deregisterSessionEventsDo(Session *session)
{
  SessionEventObservers *observers;
  
  observers= session->getSessionObservers();

  if (observers) {
    session->setSessionObservers(NULL);
    observers->clear_all_observers();
    delete observers;
  }
}

  
/* Event observer list iterator: */
//----------
class EventIterate : public unary_function<pair<unsigned int, Event *>, bool>
{
  EventData *data;
  
public:
  EventIterate(EventData *data_arg) :
    unary_function<pair<unsigned int, Event *>, bool>(),
    data(data_arg)
    {}

  inline result_type operator()(argument_type handler)
  {
    bool result= handler.second->observeEvent(data);
    if (result)
    {
      /* TRANSLATORS: The leading word "Event" is the name
         of the plugin api, and so should not be translated. */
      errmsg_printf(ERRMSG_LVL_ERROR,
                    _("EventIterate event handler '%s' failed for event '%s'"),
                    handler.second->getName().c_str(), handler.second->eventName(data->event));
      
      if (data->cannot_fail)
        result= false;
    }
    return result;
  }
};


bool Event::callEventObservers(EventData *data)
{
  multimap<unsigned int, Event *> *observers= NULL;
 
  switch (data->event_classs) {
  case SessionEvent:
    {
      SessionEventData *session_data= (SessionEventData *)data;
      observers= session_data->session->getSessionObservers()->getObservers(data->event);
    }
    break;
    
  case SchemaEvent: 
    {
      SchemaEventData *schema_data= (SchemaEventData *)data;
      SchemaEventObservers *schema_observers;
     
      /* Get the Schema event observers for the session.
       * If this is the first time a Schema event has occurred
       * for this database in this session then no observers
       * will be resistered yet in which case they must be resistered now.
       */
      std::string db(schema_data->db);
      schema_observers= schema_data->session->getSchemaObservers(&db);
      if (!schema_observers) 
      {
        registerSchemaEventsDo(schema_data->session, &db);
        schema_observers= schema_data->session->getSchemaObservers(&db);
      }

      observers= schema_observers->getObservers(data->event);
    }
    break;
    
  case TableEvent:
    {
      TableEventData *table_data= (TableEventData *)data;
      observers= table_data->table->getTableObservers()->getObservers(data->event);
    }
    break;
  }
  
  if (observers == NULL)
    return false; // Nobody was interested in the event. :(

  /* Use find_if instead of foreach so that we can collect return codes */
  multimap<unsigned int, Event *>::iterator iter=
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
 * of a schema an event,
 */
bool Event::preDropTableDo(Session *session, TableIdentifier *table)
{
  PreDropTableEventData eventData(session, table);
  return callEventObservers(&eventData);
}

void Event::postDropTableDo(Session *session, TableIdentifier *table, int err)
{
  PostDropTableEventData eventData(session, table, err);
  callEventObservers(&eventData);
}

bool Event::preRenameTableDo(Session *session, TableIdentifier *from, TableIdentifier *to)
{
  PreRenameTableEventData eventData(session, from, to);
  return callEventObservers(&eventData);
}

void Event::postRenameTableDo(Session *session, TableIdentifier *from, TableIdentifier *to, int err)
{
  PostRenameTableEventData eventData(session, from, to, err);
  callEventObservers(&eventData);
}

/*==========================================================*/
/* Static meathods called by drizzle to notify interested plugins 
 * of a table an event,
 */
bool Event::preWriteRowDo(Session *session, TableShare *table_share, unsigned char *buf)
{
  PreWriteRowEventData eventData(session, table_share, buf);
  return callEventObservers(&eventData);
}

void Event::postWriteRowDo(Session *session, TableShare *table_share, const unsigned char *buf, int err)
{
  PostWriteRowEventData eventData(session, table_share, buf, err);
  callEventObservers(&eventData);
}

bool Event::preDeleteRowDo(Session *session, TableShare *table_share, const unsigned char *buf)
{
  PreDeleteRowEventData eventData(session, table_share, buf);
  return callEventObservers(&eventData);
}

void Event::postDeleteRowDo(Session *session, TableShare *table_share, const unsigned char *buf, int err)
{
  PostDeleteRowEventData eventData(session, table_share, buf, err);
  callEventObservers(&eventData);
}

bool Event::preUpdateRowDo(Session *session, TableShare *table_share, const unsigned char *old_data, unsigned char *new_data)
{
  PreUpdateRowEventData eventData(session, table_share, old_data, new_data);
  return callEventObservers(&eventData);
}

void Event::postUpdateRowDo(Session *session, TableShare *table_share, const unsigned char *old_data, unsigned char *new_data, int err)
{
  PostUpdateRowEventData eventData(session, table_share, old_data, new_data, err);
  callEventObservers(&eventData);
}

/*==========================================================*/
/* Static meathods called by drizzle to notify interested plugins 
 * of a table an event,
 */
bool Event::preCreateDatabaseDo(Session *session, const char *db)
{
  PreCreateDatabaseEventData eventData(session, db);
  return callEventObservers(&eventData);
}

void Event::postCreateDatabaseDo(Session *session, const char *db, int err)
{
  PostCreateDatabaseEventData eventData(session, db, err);
  callEventObservers(&eventData);
}

bool Event::preDropDatabaseDo(Session *session, const char *db)
{
  PreDropDatabaseEventData eventData(session, db);
  return callEventObservers(&eventData);
}

void Event::postDropDatabaseDo(Session *session, const char *db, int err)
{
  PostDropDatabaseEventData eventData(session, db, err);
  callEventObservers(&eventData);
}


} /* namespace plugin */
} /* namespace drizzled */
