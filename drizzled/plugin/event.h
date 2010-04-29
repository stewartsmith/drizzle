/* -*- mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; -*-
 *  vim:expandtab:shiftwidth=2:tabstop=2:smarttab:
 *
 *  Definitions required for Event plugin
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
 * In event.h
 * 1: Add your event to EventType.
 * 2: Add it to Event::eventName().
 * 3: Cerate a EventData class for it based on SessionEventData, SchemaEventData, 
 *  or TableEventData depending on the event type class.
 * 4: Add a static method to the Event class, similar to preWriteRowDo() for example,
 *  that will be called by drizzle.
 *
 * In event.cc
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
  class EventObservers;
  class EventData;


class Event : public Plugin
{
  Event();
  Event(const Event &);
  Event& operator=(const Event &);
public:
  explicit Event(std::string name_arg)
    : Plugin(name_arg, "Event")
  {}
  virtual ~Event() {}

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

  /* Event data classes: */
  enum EventClass{SessionEvent, SchemaEvent, TableEvent};

  /*==========================================================*/
  /* registerEvents() must be implemented to allow the plugin to
   * register which events it is interested in.
   */
  virtual void registerTableEvents(TableShare *, EventObservers *){}
  virtual void registerSchemaEvents(const std::string */*db*/, EventObservers *) {}
  virtual void registerSessionEvents(Session *, EventObservers *) {}

  virtual bool observeEvent(EventData *) = 0;

  /*==========================================================*/
  /* Static access methods called by drizzle: */
  static bool addPlugin(Event *handler);
  static void removePlugin(Event *handler);
  
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
  void registerEvent(EventObservers *observers, EventType event, int position = 0); 

  /*==========================================================*/
  /* Called from drizzle to register all events for all event plugins 
   * interested in this table. 
   */
  static void registerTableEventsDo(TableShare *table_share); 
  static void deregisterTableEventsDo(TableShare *table_share); 

  /*==========================================================*/
  /* Called from drizzle to register all events for all event plugins 
  * interested in this database. 
  */
  static void registerSchemaEventsDo(Session *session, const std::string *db); 
  static void deregisterSchemaEventsDo(Session *session, const std::string *db); 

  /*==========================================================*/
  /* Called from drizzle to register all events for all event plugins 
  * interested in this session. 
  */
  static void registerSessionEventsDo(Session *session); 
  static void deregisterSessionEventsDo(Session *session); 

 
  /*==========================================================*/
  /* Static meathods called by drizzle to notify interested plugins 
   * of a schema an event,
   */
  static bool preDropTableDo(Session *session, TableIdentifier *table);
  static void postDropTableDo(Session *session, TableIdentifier *table, int err);
  static bool preRenameTableDo(Session *session, TableIdentifier *from, TableIdentifier *to);
  static void postRenameTableDo(Session *session, TableIdentifier *from, TableIdentifier *to, int err);

  /*==========================================================*/
  /* Static meathods called by drizzle to notify interested plugins 
   * of a table an event,
   */
  static bool preWriteRowDo(Session *session, TableShare *table_share, unsigned char *buf);
  static void postWriteRowDo(Session *session, TableShare *table_share, const unsigned char *buf, int err);
  static bool preDeleteRowDo(Session *session, TableShare *table_share, const unsigned char *buf);
  static void postDeleteRowDo(Session *session, TableShare *table_share, const unsigned char *buf, int err);
  static bool preUpdateRowDo(Session *session, TableShare *table_share, const unsigned char *old_data, unsigned char *new_data);
  static void postUpdateRowDo(Session *session, TableShare *table_share, const unsigned char *old_data, unsigned char *new_data, int err);

  /*==========================================================*/
  /* Static meathods called by drizzle to notify interested plugins 
   * of a table an event,
   */
  static bool preCreateDatabaseDo(Session *session, const char *db);
  static void postCreateDatabaseDo(Session *session, const char *db, int err);
  static bool preDropDatabaseDo(Session *session, const char *db);
  static void postDropDatabaseDo(Session *session, const char *db, int err);


  /*==========================================================*/
  /* Internal method to call the generic event observers.
   */
  private:
  static bool callEventObservers(EventData *data);

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

/* Event data classes: */
//======================================
class EventData
{
  public:
  Event::EventType event;
  Event::EventClass event_classs;
  bool cannot_fail;
  
  EventData(Event::EventType event_arg, Event::EventClass event_class_arg, bool cannot_fail_arg): 
    event(event_arg), 
    event_classs(event_class_arg), 
    cannot_fail(cannot_fail_arg)
    {}
  virtual ~EventData(){}
  
};

//-----
class SessionEventData: public EventData
{
  public:
  Session *session;
  
  SessionEventData(Event::EventType event_arg, bool cannot_fail_arg, Session *session_arg): 
    EventData(event_arg, Event::SessionEvent, cannot_fail_arg),
    session(session_arg)
    {}
  virtual ~SessionEventData(){}
  
};

//-----
class SchemaEventData: public EventData
{
  public:
  Session *session;
  const char *db;
  
  SchemaEventData(Event::EventType event_arg, bool cannot_fail_arg, Session *session_arg, const char *db_arg): 
    EventData(event_arg, Event::SchemaEvent, cannot_fail_arg),
    session(session_arg),
    db(db_arg)
    {}
  virtual ~SchemaEventData(){}
  
};

//-----
class TableEventData: public EventData
{
  public:
  Session *session;
  TableShare *table;  
  
  TableEventData(Event::EventType event_arg, bool cannot_fail_arg, Session *session_arg, TableShare *table_arg): 
    EventData(event_arg, Event::TableEvent, cannot_fail_arg),
    session(session_arg),
    table(table_arg)
    {}
  virtual ~TableEventData(){}
  
};

//-----
class PreCreateDatabaseEventData: public SessionEventData
{
  public:
  const char *db;

  PreCreateDatabaseEventData(Session *session_arg, const char *db_arg): 
  SessionEventData(Event::PRE_CREATE_DATABASE, false, session_arg), 
  db(db_arg)
  {}  
};

//-----
class PostCreateDatabaseEventData: public SessionEventData
{
  public:
  const char *db;
  int err;

  PostCreateDatabaseEventData(Session *session_arg, const char *db_arg, int err_arg): 
  SessionEventData(Event::POST_CREATE_DATABASE, true, session_arg), 
  db(db_arg), 
  err(err_arg)
  {}  
};

//-----
class PreDropDatabaseEventData: public SessionEventData
{
  public:
  const char *db;

  PreDropDatabaseEventData(Session *session_arg, const char *db_arg): 
  SessionEventData(Event::PRE_DROP_DATABASE, false, session_arg), 
  db(db_arg)
  {}  
};

//-----
class PostDropDatabaseEventData: public SessionEventData
{
  public:
  const char *db;
  int err;

  PostDropDatabaseEventData(Session *session_arg, const char *db_arg, int err_arg): 
  SessionEventData(Event::POST_DROP_DATABASE, true, session_arg), 
  db(db_arg), 
  err(err_arg) 
  {}  
};

//-----
class PreDropTableEventData: public SchemaEventData
{
  public:
  TableIdentifier *table;

  PreDropTableEventData(Session *session_arg, TableIdentifier *table_arg): 
  SchemaEventData(Event::PRE_DROP_TABLE, false, session_arg, table_arg->getSchemaName().c_str()), 
  table(table_arg)
  {}  
};

//-----
class PostDropTableEventData: public SchemaEventData
{
  public:
  TableIdentifier *table;
  int err;

  PostDropTableEventData(Session *session_arg, TableIdentifier *table_arg, int err_arg): 
  SchemaEventData(Event::POST_DROP_TABLE, true, session_arg, table_arg->getSchemaName().c_str()), 
  table(table_arg), 
  err(err_arg)
  {}  
};

//-----
class PreRenameTableEventData: public SchemaEventData
{
  public:
  TableIdentifier *from;
  TableIdentifier *to;

  PreRenameTableEventData(Session *session_arg, TableIdentifier *from_arg, TableIdentifier *to_arg): 
  SchemaEventData(Event::PRE_RENAME_TABLE, false, session_arg, from_arg->getSchemaName().c_str()), 
  from(from_arg), 
  to(to_arg)
  {}  
};

//-----
class PostRenameTableEventData: public SchemaEventData
{
  public:
  TableIdentifier *from;
  TableIdentifier *to;
  int err;

  PostRenameTableEventData(Session *session_arg, TableIdentifier *from_arg, TableIdentifier *to_arg, int err_arg): 
  SchemaEventData(Event::POST_RENAME_TABLE, true, session_arg, from_arg->getSchemaName().c_str()), 
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

  PreWriteRowEventData(Session *session_arg, TableShare *table_arg, unsigned char *row_arg): 
  TableEventData(Event::PRE_WRITE_ROW, false, session_arg, table_arg), 
  row(row_arg)
  {}  
};

//-----
class PostWriteRowEventData: public TableEventData
{
  public:
  const unsigned char *row;
  int err;

  PostWriteRowEventData(Session *session_arg, TableShare *table_arg, const unsigned char *row_arg, int err_arg): 
  TableEventData(Event::POST_WRITE_ROW, true, session_arg, table_arg), 
  row(row_arg),
  err(err_arg)
  {}  
};

//-----
class PreDeleteRowEventData: public TableEventData
{
  public:
  const unsigned char *row;

  PreDeleteRowEventData(Session *session_arg, TableShare *table_arg, const unsigned char *row_arg): 
  TableEventData(Event::PRE_DELETE_ROW, false, session_arg, table_arg), 
  row(row_arg)
  {}  
};

//-----
class PostDeleteRowEventData: public TableEventData
{
  public:
  const unsigned char *row;
  int err;

  PostDeleteRowEventData(Session *session_arg, TableShare *table_arg, const unsigned char *row_arg, int err_arg): 
  TableEventData(Event::POST_DELETE_ROW, true, session_arg, table_arg), 
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

  PreUpdateRowEventData(Session *session_arg, TableShare *table_arg,  
    const unsigned char *old_row_arg, 
    unsigned char *new_row_arg): 
      TableEventData(Event::PRE_UPDATE_ROW, false, session_arg, table_arg), 
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

  PostUpdateRowEventData(Session *session_arg, TableShare *table_arg, 
    const unsigned char *old_row_arg, 
    const unsigned char *new_row_arg, 
    int err_arg): 
      TableEventData(Event::POST_UPDATE_ROW, true, session_arg, table_arg), 
      old_row(old_row_arg),
      new_row(new_row_arg),
      err(err_arg)
      {}  
};

//=======================================================

} /* namespace plugin */
} /* namespace drizzled */

#endif /* DRIZZLED_PLUGIN_EVENT_H */
