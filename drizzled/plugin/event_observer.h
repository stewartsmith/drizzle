/* -*- mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; -*-
 *  vim:expandtab:shiftwidth=2:tabstop=2:smarttab:
 *
 *  Definitions required for EventObserver plugin
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

/*
 * How to add a new event:
 *
 * In event_observer.h
 * 1: Add your event to EventType.
 * 2: Add it to EventObserver::eventName().
 * 3: Cerate a EventData class for it based on SessionEventData, SchemaEventData, 
 *  or TableEventData depending on the event type class.
 * 4: Add a static method to the EventObserver class, similar to preWriteRowDo() for example,
 *  that will be called by drizzle.
 *
 * In event_observer.cc
 * 5: Impliment your static event method, similar to preWriteRowDo() for example.
 * 6: Depending on the event type class add an event vector for it to the SessionEventObservers,
 *  SchemaEventObservers, or TableEventObservers class.
 *
 */
#ifndef DRIZZLED_PLUGIN_EVENT_H
#define DRIZZLED_PLUGIN_EVENT_H

#include "drizzled/plugin/plugin.h"

#include <string>

namespace drizzled
{

class TableShare;

namespace plugin
{
  class EventObserverList;
  class EventData;


class EventObserver : public Plugin
{
  EventObserver();
  EventObserver(const EventObserver &);
  EventObserver& operator=(const EventObserver &);
public:
  explicit EventObserver(std::string name_arg)
    : Plugin(name_arg, "EventObserver")
  {}
  virtual ~EventObserver() {}

   enum EventType{
      /* Session events: */
      PRE_CREATE_DATABASE, POST_CREATE_DATABASE, 
      PRE_DROP_DATABASE,   POST_DROP_DATABASE,
      
      /* Schema events: */
      PRE_DROP_TABLE,   POST_DROP_TABLE, 
      PRE_RENAME_TABLE, POST_RENAME_TABLE, 
      
      /* Table events: */
      PRE_WRITE_ROW,    POST_WRITE_ROW, 
      PRE_UPDATE_ROW,   POST_UPDATE_ROW, 
      PRE_DELETE_ROW,   POST_DELETE_ROW
      
    };

  /* EventObserver data classes: */
  enum EventClass{SessionEvent, SchemaEvent, TableEvent};

  /*==========================================================*/
  /* registerEvents() must be implemented to allow the plugin to
   * register which events it is interested in.
   */
  virtual void registerTableEvents(TableShare &, EventObserverList &){}
  virtual void registerSchemaEvents(const std::string &/*db*/, EventObserverList &) {}
  virtual void registerSessionEvents(Session &, EventObserverList &) {}

  virtual bool observeEvent(EventData &)= 0;

  /*==========================================================*/
  /* Static access methods called by drizzle: */
  static bool addPlugin(EventObserver *handler);
  static void removePlugin(EventObserver *handler);
  
  /*==========================================================*/
  /* Register an event of interest for this plugin. 
   * This is called from with in the plugin when registering itself.
   *
   * The position field is used to indicate the order the event observer is to be 
   * called. If the event observer must be called before any other observer then 
   * the position must be set to 1. If it must be called last then the position must be 
   * set to -1. A position of 0 indicated the position doesn't matter.
   *
   * If 2 plugins require the same position then which is called first in not guarenteed.
   * In this case a warrning will be logged but execution will continue.
   * 
   * It is good practice that if the event position matters not to hard code the position
   * but supply a systen variable so that it can be set at runtime so that the user can
   * decide which event should be called first.
   */
  void registerEvent(EventObserverList &observers, EventType event, int position= 0); 

  /*==========================================================*/
  /* Called from drizzle to register all events for all event plugins 
   * interested in this table. 
   */
  static void registerTableEventsDo(TableShare &table_share); 
  static void deregisterTableEventsDo(TableShare &table_share); 

  /*==========================================================*/
  /* Called from drizzle to register all events for all event plugins 
  * interested in this database. 
  */
  static void registerSchemaEventsDo(Session &session, const std::string &db); 
  static void deregisterSchemaEventsDo(Session &session, const std::string &db); 

  /*==========================================================*/
  /* Called from drizzle to register all events for all event plugins 
  * interested in this session. 
  */
  static void registerSessionEventsDo(Session &session); 
  static void deregisterSessionEventsDo(Session &session); 

 
  /*==========================================================*/
  /* Static meathods called by drizzle to notify interested plugins 
   * of a schema an event,
   */
  static bool preDropTableDo(Session &session, TableIdentifier &table);
  static bool postDropTableDo(Session &session, TableIdentifier &table, int err);
  static bool preRenameTableDo(Session &session, TableIdentifier &from, TableIdentifier &to);
  static bool postRenameTableDo(Session &session, TableIdentifier &from, TableIdentifier &to, int err);

  /*==========================================================*/
  /* Static meathods called by drizzle to notify interested plugins 
   * of a table an event,
   */
  static bool preWriteRowDo(Session &session, TableShare &table_share, unsigned char *buf);
  static bool postWriteRowDo(Session &session, TableShare &table_share, const unsigned char *buf, int err);
  static bool preDeleteRowDo(Session &session, TableShare &table_share, const unsigned char *buf);
  static bool postDeleteRowDo(Session &session, TableShare &table_share, const unsigned char *buf, int err);
  static bool preUpdateRowDo(Session &session, TableShare &table_share, const unsigned char *old_data, unsigned char *new_data);
  static bool postUpdateRowDo(Session &session, TableShare &table_share, const unsigned char *old_data, unsigned char *new_data, int err);

  /*==========================================================*/
  /* Static meathods called by drizzle to notify interested plugins 
   * of a table an event,
   */
  static bool preCreateDatabaseDo(Session &session, const std::string &db);
  static bool postCreateDatabaseDo(Session &session, const std::string &db, int err);
  static bool preDropDatabaseDo(Session &session, const std::string &db);
  static bool postDropDatabaseDo(Session &session, const std::string &db, int err);


  /*==========================================================*/
  /* Internal method to call the generic event observers.
   */
  private:
  static bool callEventObservers(EventData &data);

  public:
  /*==========================================================*/
  /* Some utility functions: */
  static const char *eventName(EventType event) 
  {
    switch(event) 
    {
    case PRE_DROP_TABLE:
      return "PRE_DROP_TABLE";
    
    case POST_DROP_TABLE:
      return "POST_DROP_TABLE";
    
    case PRE_RENAME_TABLE:
      return "PRE_RENAME_TABLE";
      
    case POST_RENAME_TABLE:
      return "POST_RENAME_TABLE";
      
    case PRE_WRITE_ROW:
       return "PRE_WRITE_ROW";
      
    case POST_WRITE_ROW:
       return "POST_WRITE_ROW";
      
    case PRE_UPDATE_ROW:
      return "PRE_UPDATE_ROW";
               
    case POST_UPDATE_ROW:
      return "POST_UPDATE_ROW";
      
    case PRE_DELETE_ROW:
      return "PRE_DELETE_ROW";

    case POST_DELETE_ROW:
      return "POST_DELETE_ROW";

    case PRE_CREATE_DATABASE:
      return "PRE_CREATE_DATABASE";

    case POST_CREATE_DATABASE:
      return "POST_CREATE_DATABASE";

    case PRE_DROP_DATABASE:
      return "PRE_DROP_DATABASE";

    case POST_DROP_DATABASE:
      return "POST_DROP_DATABASE";
   }
    
    return "Unknown";
  }
  
};

/* EventObserver data classes: */
//======================================
class EventData
{
public:
  EventObserver::EventType event;
  EventObserver::EventClass event_classs;
   
  EventData(EventObserver::EventType event_arg, EventObserver::EventClass event_class_arg): 
    event(event_arg), 
    event_classs(event_class_arg)
    {}
  virtual ~EventData(){}
  
};

//-----
class SessionEventData: public EventData
{
public:
  Session &session;
  
  SessionEventData(EventObserver::EventType event_arg, Session &session_arg): 
    EventData(event_arg, EventObserver::SessionEvent),
    session(session_arg)
    {}
  virtual ~SessionEventData(){}
  
};

//-----
class SchemaEventData: public EventData
{
public:
  Session &session;
  const std::string &db;
  
  SchemaEventData(EventObserver::EventType event_arg, Session &session_arg, const std::string &db_arg): 
    EventData(event_arg, EventObserver::SchemaEvent),
    session(session_arg),
    db(db_arg)
    {}
  virtual ~SchemaEventData(){}
  
};

//-----
class TableEventData: public EventData
{
public:
  Session &session;
  TableShare &table;  
  
  TableEventData(EventObserver::EventType event_arg, Session &session_arg, TableShare &table_arg): 
    EventData(event_arg, EventObserver::TableEvent),
    session(session_arg),
    table(table_arg)
    {}
  virtual ~TableEventData(){}
  
};

//-----
class PreCreateDatabaseEventData: public SessionEventData
{
public:
  const std::string &db;

  PreCreateDatabaseEventData(Session &session_arg, const std::string &db_arg): 
  SessionEventData(EventObserver::PRE_CREATE_DATABASE, session_arg), 
  db(db_arg)
  {}  
};

//-----
class PostCreateDatabaseEventData: public SessionEventData
{
public:
  const std::string &db;
  int err;

  PostCreateDatabaseEventData(Session &session_arg, const std::string &db_arg, int err_arg): 
  SessionEventData(EventObserver::POST_CREATE_DATABASE, session_arg), 
  db(db_arg), 
  err(err_arg)
  {}  
};

//-----
class PreDropDatabaseEventData: public SessionEventData
{
public:
  const std::string &db;

  PreDropDatabaseEventData(Session &session_arg, const std::string &db_arg): 
  SessionEventData(EventObserver::PRE_DROP_DATABASE, session_arg), 
  db(db_arg)
  {}  
};

//-----
class PostDropDatabaseEventData: public SessionEventData
{
public:
  const std::string &db;
  int err;

  PostDropDatabaseEventData(Session &session_arg, const std::string &db_arg, int err_arg): 
  SessionEventData(EventObserver::POST_DROP_DATABASE, session_arg), 
  db(db_arg), 
  err(err_arg) 
  {}  
};

//-----
class PreDropTableEventData: public SchemaEventData
{
public:
  TableIdentifier &table;

  PreDropTableEventData(Session &session_arg, TableIdentifier &table_arg): 
  SchemaEventData(EventObserver::PRE_DROP_TABLE, session_arg, table_arg.getSchemaName()), 
  table(table_arg)
  {}  
};

//-----
class PostDropTableEventData: public SchemaEventData
{
public:
  TableIdentifier &table;
  int err;

  PostDropTableEventData(Session &session_arg, TableIdentifier &table_arg, int err_arg): 
  SchemaEventData(EventObserver::POST_DROP_TABLE, session_arg, table_arg.getSchemaName()), 
  table(table_arg), 
  err(err_arg)
  {}  
};

//-----
class PreRenameTableEventData: public SchemaEventData
{
public:
  TableIdentifier &from;
  TableIdentifier &to;

  PreRenameTableEventData(Session &session_arg, TableIdentifier &from_arg, TableIdentifier &to_arg): 
  SchemaEventData(EventObserver::PRE_RENAME_TABLE, session_arg, from_arg.getSchemaName()), 
  from(from_arg), 
  to(to_arg)
  {}  
};

//-----
class PostRenameTableEventData: public SchemaEventData
{
public:
  TableIdentifier &from;
  TableIdentifier &to;
  int err;

  PostRenameTableEventData(Session &session_arg, TableIdentifier &from_arg, TableIdentifier &to_arg, int err_arg): 
  SchemaEventData(EventObserver::POST_RENAME_TABLE, session_arg, from_arg.getSchemaName()), 
  from(from_arg), 
  to(to_arg), 
  err(err_arg)
  {}  
};

//-----
class PreWriteRowEventData: public TableEventData
{
public:
  unsigned char *row;

  PreWriteRowEventData(Session &session_arg, TableShare &table_arg, unsigned char *row_arg): 
  TableEventData(EventObserver::PRE_WRITE_ROW, session_arg, table_arg), 
  row(row_arg)
  {}  
};

//-----
class PostWriteRowEventData: public TableEventData
{
public:
  const unsigned char *row;
  int err;

  PostWriteRowEventData(Session &session_arg, TableShare &table_arg, const unsigned char *row_arg, int err_arg): 
  TableEventData(EventObserver::POST_WRITE_ROW, session_arg, table_arg), 
  row(row_arg),
  err(err_arg)
  {}  
};

//-----
class PreDeleteRowEventData: public TableEventData
{
public:
  const unsigned char *row;

  PreDeleteRowEventData(Session &session_arg, TableShare &table_arg, const unsigned char *row_arg): 
  TableEventData(EventObserver::PRE_DELETE_ROW, session_arg, table_arg), 
  row(row_arg)
  {}  
};

//-----
class PostDeleteRowEventData: public TableEventData
{
public:
  const unsigned char *row;
  int err;

  PostDeleteRowEventData(Session &session_arg, TableShare &table_arg, const unsigned char *row_arg, int err_arg): 
  TableEventData(EventObserver::POST_DELETE_ROW, session_arg, table_arg), 
  row(row_arg),
  err(err_arg)
  {}  
};

//-----
class PreUpdateRowEventData: public TableEventData
{
public:
  const unsigned char *old_row;
  unsigned char *new_row;

  PreUpdateRowEventData(Session &session_arg, TableShare &table_arg,  
    const unsigned char *old_row_arg, 
    unsigned char *new_row_arg): 
      TableEventData(EventObserver::PRE_UPDATE_ROW, session_arg, table_arg), 
      old_row(old_row_arg),
      new_row(new_row_arg)      
      {}  
};

//-----
class PostUpdateRowEventData: public TableEventData
{
public:
  const unsigned char *old_row;
  const unsigned char *new_row;
  int err;

  PostUpdateRowEventData(Session &session_arg, TableShare &table_arg, 
    const unsigned char *old_row_arg, 
    const unsigned char *new_row_arg, 
    int err_arg): 
      TableEventData(EventObserver::POST_UPDATE_ROW, session_arg, table_arg), 
      old_row(old_row_arg),
      new_row(new_row_arg),
      err(err_arg)
      {}  
};

//=======================================================

} /* namespace plugin */
} /* namespace drizzled */

#endif /* DRIZZLED_PLUGIN_EVENT_H */
