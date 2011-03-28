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
 *
 * Barry Leslie
 *
 * 2010-05-12
 */

#include <config.h>

#include <string>
#include <vector>

#include <drizzled/session.h>
#include <drizzled/table_list.h>
#include <drizzled/table/instance.h>
#include <drizzled/module/registry.h>
#include <drizzled/plugin/event_observer.h>
#include <drizzled/util/functors.h>
#include <algorithm>

namespace drizzled {
namespace plugin {

  EventObserverVector all_event_plugins;

  const EventObserverVector &EventObserver::getEventObservers()
  {
    return all_event_plugins;
  }

  bool EventObserver::addPlugin(EventObserver *handler)
  {
    if (handler != NULL)
      all_event_plugins.push_back(handler);
    return false;
  }

  void EventObserver::removePlugin(EventObserver *handler)
  {
    if (handler != NULL)
      all_event_plugins.erase(std::find(all_event_plugins.begin(), all_event_plugins.end(), handler));
  }


  /* 
   * The Event Observer list class in which plugins register which events they
   * are interested in.
   *
   * Each table share for example, will have one of these hung on it to store
   * a list off all event observers interested in it and which events they are
   * interested in.
 */
  class EventObserverList
  {

  public:
    typedef std::multimap<uint32_t, EventObserver *> ObserverMap;

  private:
    /* A list of lists indexed by event type. */
    std::vector<ObserverMap *> event_observer_lists;

  public:

    EventObserverList()
    {
			// Initialize the list with NULL pointers.
			event_observer_lists.assign(EventObserver::MAX_EVENT_COUNT, NULL);
    }

    ~EventObserverList()
    {
      std::for_each(event_observer_lists.begin(),
                    event_observer_lists.end(),
                    DeletePtr());
      event_observer_lists.clear();
    }

    /* Add the observer to the observer list for the even, positioning it if required.
     *
     * Note: Event observers are storted in a multimap object so that the order in which
     * they are called can be sorted based on the requested position. Lookups are never done
     * on the multimap, once filled it is used as a vector.
   */
    void addObserver(EventObserver *eventObserver, enum EventObserver::EventType event, int32_t position)
    {
      uint32_t event_pos;
      ObserverMap *observers;

      observers= event_observer_lists[event];
      if (observers == NULL) 
      {
        observers= new ObserverMap();
        event_observer_lists[event]= observers;
      }

      if (position == 0)
        event_pos= INT32_MAX; // Set the event position to be in the middle.
      else
        event_pos= (uint32_t) position;

      /* If positioned then check if the position is already taken. */
      if (position) 
      {
        if (observers->count(event_pos))
        {
          errmsg_printf(error::WARN,
                        _("EventObserverList::addEventObserver() Duplicate event position %d for event '%s' from EventObserver plugin '%s'"),
                        position,
                        EventObserver::eventName(event), 
                        eventObserver->getName().c_str());
        }
      }

      observers->insert(std::pair<uint32_t, EventObserver *>(event_pos, eventObserver) );
    }


    /* Get the observer list for an event type. Will return NULL if no observer exists.*/
    ObserverMap *getObservers(enum EventObserver::EventType event)
    {
      return event_observer_lists[event];
    }
  };


  //---------
  /* registerEvent() is called from the event observer plugins to add themselves to
   * the event observer list to be notified when the specified event takes place.
   */ 
  void EventObserver::registerEvent(EventObserverList &observers, EventType event, int32_t position)
  {
    observers.addObserver(this, event, position);
  }

  /*========================================================*/
  /*              Table Event Observer handling:           */
  /*========================================================*/

  //----------
  /* For each EventObserver plugin call its registerTableEventsDo() meathod so that it can
   * register what events, if any, it is interested in on this table.
   */ 
  class RegisterTableEventsIterate : public std::unary_function<EventObserver *, void>
  {
    TableShare &table_share;
    EventObserverList &observers;

  public:
    RegisterTableEventsIterate(TableShare &table_share_arg, EventObserverList &observers_arg): 
      table_share(table_share_arg), observers(observers_arg) {}
    inline result_type operator() (argument_type eventObserver)
    {
      eventObserver->registerTableEventsDo(table_share, observers);
    }
  };

  //----------
  /* 
   * registerTableEvents() is called by drizzle to register all plugins that
   * may be interested in table events on the newly created TableShare object.
   */ 
  void EventObserver::registerTableEvents(TableShare &table_share)
  {
    if (all_event_plugins.empty())
      return;

    EventObserverList *observers;

    observers= table_share.getTableObservers();

    if (observers != NULL) 
		{
			errmsg_printf(error::WARN,
									_("EventObserver::registerTableEvents(): Table already has events registered on it: probable programming error."));
			table_share.setTableObservers(NULL);
      delete observers;
    }

		observers= new EventObserverList();
		table_share.setTableObservers(observers);
 

    std::for_each(all_event_plugins.begin(), all_event_plugins.end(),
                  RegisterTableEventsIterate(table_share, *observers));

  }

  //----------
  /* Cleanup before freeing the TableShare object. */
  void EventObserver::deregisterTableEvents(TableShare &table_share)
  {
   if (all_event_plugins.empty())
      return;

    EventObserverList *observers= table_share.getTableObservers();

    if (observers) 
    {
      table_share.setTableObservers(NULL);
      delete observers;
    }
  }


  /*========================================================*/
  /*              Schema Event Observer handling:           */
  /*========================================================*/

  //----------
  /* For each EventObserver plugin call its registerSchemaEventsDo() meathod so that it can
   * register what events, if any, it is interested in on the schema.
   */ 
  class RegisterSchemaEventsIterate : public std::unary_function<EventObserver *, void>
  {
    const std::string &db;
    EventObserverList &observers;
  public:
    RegisterSchemaEventsIterate(const std::string &db_arg, EventObserverList &observers_arg) :     
      db(db_arg),
      observers(observers_arg){}

    inline result_type operator() (argument_type eventObserver)
    {
      eventObserver->registerSchemaEventsDo(db, observers);
    }
  };

  //----------
  /* 
   * registerSchemaEvents() is called by drizzle to register all plugins that
   * may be interested in schema events on the database.
   */ 
  void EventObserver::registerSchemaEvents(Session &session, const std::string &db)
  {
    if (all_event_plugins.empty())
      return;
    EventObserverList *observers= session.getSchemaObservers(db);
    if (observers == NULL) 
      observers= session.setSchemaObservers(db, new EventObserverList());

    std::for_each(all_event_plugins.begin(), all_event_plugins.end(),
                  RegisterSchemaEventsIterate(db, *observers));

  }

  //----------
  /* Cleanup before freeing the Session object. */
  void EventObserver::deregisterSchemaEvents(EventObserverList *observers)
  {
      delete observers;
   }

  /*========================================================*/
  /*             Session Event Observer handling:           */
  /*========================================================*/

  //----------
  /* For each EventObserver plugin call its registerSessionEventsDo() meathod so that it can
   * register what events, if any, it is interested in on this session.
   */ 
  class RegisterSessionEventsIterate : public std::unary_function<EventObserver *, void>
  {
    Session &session;
    EventObserverList &observers;
  public:
    RegisterSessionEventsIterate(Session &session_arg, EventObserverList &observers_arg) : 
      session(session_arg), observers(observers_arg) {}
    inline result_type operator() (argument_type eventObserver)
    {
      eventObserver->registerSessionEventsDo(session, observers);
    }
  };

  //----------
  /* 
   * registerSessionEvents() is called by drizzle to register all plugins that
   * may be interested in session events on the newly created session.
   */ 
  void EventObserver::registerSessionEvents(Session &session)
  {
    if (all_event_plugins.empty())
      return;

    EventObserverList *observers;

    observers= session.getSessionObservers();
		if (observers) { // This should not happed
			errmsg_printf(error::WARN,
									_("EventObserver::registerSessionEvents(): Session already has events registered on it: probable programming error."));
			session.setSessionObservers(NULL);
			delete observers;
		}

	observers= new EventObserverList();
	session.setSessionObservers(observers);

  std::for_each(all_event_plugins.begin(), all_event_plugins.end(),
                RegisterSessionEventsIterate(session, *observers));

  }

  //----------
  /* Cleanup before freeing the session object. */
  void EventObserver::deregisterSessionEvents(EventObserverList *observers)
  {
      delete observers;
  }


  /* Event observer list iterator: */
  //----------
  class EventIterate : public std::unary_function<std::pair<uint32_t, EventObserver *>, bool>
  {
    EventData &data;

  public:
    EventIterate(EventData &data_arg) :
      std::unary_function<std::pair<uint32_t, EventObserver *>, bool>(),
      data(data_arg)
    {}

    inline result_type operator()(argument_type handler)
    {
      bool result= handler.second->observeEventDo(data);
      if (result)
      {
        /* TRANSLATORS: The leading word "EventObserver" is the name
          of the plugin api, and so should not be translated. */
        errmsg_printf(error::ERROR,
                      _("EventIterate event handler '%s' failed for event '%s'"),
                      handler.second->getName().c_str(), handler.second->eventName(data.event));

      }
      return result;
    }
  };


  /*==========================================================*/
  /* Generic meathods called by drizzle to notify all interested  
   * plugins of an event,
 */

  // Call all event observers interested in the event.
  bool EventData::callEventObservers()
  {
    EventObserverList::ObserverMap *eventObservers;

    if (observerList == NULL)
      return false; // Nobody was interested in the event. :(

    eventObservers = observerList->getObservers(event);

    if (eventObservers == NULL)
      return false; // Nobody was interested in the event. :(

    /* Use find_if instead of foreach so that we can collect return codes */
    EventObserverList::ObserverMap::iterator iter=
      std::find_if(eventObservers->begin(), eventObservers->end(),
                   EventIterate(*this)); 
    /* If iter is == end() here, that means that all of the plugins returned
     * false, which in this case means they all succeeded. Since we want to 
     * return false on success, we return the value of the two being !=.
   */
    return iter != eventObservers->end();
  }

  //--------
  bool SessionEventData::callEventObservers()
  {
    observerList= session.getSessionObservers();

    return EventData::callEventObservers();
  }

  bool SessionEventData::hasEvents(Session &in_session)
  {
    return (in_session.getSessionObservers() != NULL);
  }

  //--------
  bool SchemaEventData::callEventObservers()
  {
    observerList= session.getSchemaObservers(db);
    if (!observerList) 
    {
      EventObserver::registerSchemaEvents(session, db);
      observerList= session.getSchemaObservers(db);
    }

    return EventData::callEventObservers();
  }

  //--------
  bool TableEventData::callEventObservers()
  {
    observerList= table.getMutableShare()->getTableObservers();

    return EventData::callEventObservers();
  }

  bool TableEventData::hasEvents(Table &in_table)
  {
    return (in_table.getMutableShare()->getTableObservers() != NULL);
  }

  /*==========================================================*/
  /* Static meathods called by drizzle to notify interested plugins 
   * of a schema event.
 */
  bool EventObserver::beforeDropTable(Session &session, const drizzled::identifier::Table &table)
  {
    if (all_event_plugins.empty())
      return false;

    BeforeDropTableEventData eventData(session, table);
    return eventData.callEventObservers();
  }

  bool EventObserver::afterDropTable(Session &session, const drizzled::identifier::Table &table, int err)
  {
    if (all_event_plugins.empty())
      return false;

    AfterDropTableEventData eventData(session, table, err);
    return eventData.callEventObservers();
  }

  bool EventObserver::beforeRenameTable(Session &session, const drizzled::identifier::Table &from, const drizzled::identifier::Table &to)
  {
    if (all_event_plugins.empty())
      return false;

    BeforeRenameTableEventData eventData(session, from, to);
    return eventData.callEventObservers();
  }

  bool EventObserver::afterRenameTable(Session &session, const drizzled::identifier::Table &from, const drizzled::identifier::Table &to, int err)
  {
    if (all_event_plugins.empty())
      return false;

    AfterRenameTableEventData eventData(session, from, to, err);
    return eventData.callEventObservers();
  }

  /*==========================================================*/
  /* Static meathods called by drizzle to notify interested plugins 
   * of a table event.
   *
   * A quick test is done first to see if there are any interested observers.
 */
  bool EventObserver::beforeInsertRecord(Table &table, unsigned char *buf)
  {
    if (all_event_plugins.empty() || !TableEventData::hasEvents(table))
      return false;

    BeforeInsertRecordEventData eventData(*(table.in_use), table, buf);
    return eventData.callEventObservers();
  }

  bool EventObserver::afterInsertRecord(Table &table, const unsigned char *buf, int err)
  {
    if (all_event_plugins.empty() || !TableEventData::hasEvents(table))
      return false;

    AfterInsertRecordEventData eventData(*(table.in_use), table, buf, err);
    return eventData.callEventObservers();
  }

  bool EventObserver::beforeDeleteRecord(Table &table, const unsigned char *buf)
  {
    if (all_event_plugins.empty() || !TableEventData::hasEvents(table))
      return false;

    BeforeDeleteRecordEventData eventData(*(table.in_use), table, buf);
    return eventData.callEventObservers();
  }

  bool EventObserver::afterDeleteRecord(Table &table, const unsigned char *buf, int err)
  {
    if (all_event_plugins.empty() || !TableEventData::hasEvents(table))
      return false;

    AfterDeleteRecordEventData eventData(*(table.in_use), table, buf, err);
    return eventData.callEventObservers();
  }

  bool EventObserver::beforeUpdateRecord(Table &table, const unsigned char *old_data, unsigned char *new_data)
  {
    if (all_event_plugins.empty() || !TableEventData::hasEvents(table))
      return false;

    BeforeUpdateRecordEventData eventData(*(table.in_use), table, old_data, new_data);
    return eventData.callEventObservers();
  }

  bool EventObserver::afterUpdateRecord(Table &table, const unsigned char *old_data, unsigned char *new_data, int err)
  {
    if (all_event_plugins.empty() || !TableEventData::hasEvents(table))
      return false;

    AfterUpdateRecordEventData eventData(*(table.in_use), table, old_data, new_data, err);
    return eventData.callEventObservers();
  }

  /*==========================================================*/
  /* Static meathods called by drizzle to notify interested plugins 
   * of a session event.
   *
   * A quick test is done first to see if there are any interested observers.
*/
  bool EventObserver::beforeCreateDatabase(Session &session, const std::string &db)
  {
    if (all_event_plugins.empty() || !SessionEventData::hasEvents(session))
      return false;

    BeforeCreateDatabaseEventData eventData(session, db);
    return eventData.callEventObservers();
  }

  bool EventObserver::afterCreateDatabase(Session &session, const std::string &db, int err)
  {
    if (all_event_plugins.empty() || !SessionEventData::hasEvents(session))
      return false;

    AfterCreateDatabaseEventData eventData(session, db, err);
    return eventData.callEventObservers();
  }

  bool EventObserver::beforeDropDatabase(Session &session, const std::string &db)
  {
    if (all_event_plugins.empty() || !SessionEventData::hasEvents(session))
      return false;

    BeforeDropDatabaseEventData eventData(session, db);
    return eventData.callEventObservers();
  }

  bool EventObserver::afterDropDatabase(Session &session, const std::string &db, int err)
  {
    if (all_event_plugins.empty() || !SessionEventData::hasEvents(session))
      return false;

    AfterDropDatabaseEventData eventData(session, db, err);
    return eventData.callEventObservers();
  }

  bool EventObserver::connectSession(Session &session)
  {
    if (all_event_plugins.empty() || !SessionEventData::hasEvents(session))
      return false;

    ConnectSessionEventData eventData(session);
    return eventData.callEventObservers();
  }

  bool EventObserver::disconnectSession(Session &session)
  {
    if (all_event_plugins.empty() || !SessionEventData::hasEvents(session))
      return false;

    DisconnectSessionEventData eventData(session);
    return eventData.callEventObservers();
  }

  bool EventObserver::beforeStatement(Session &session)
  {
    if (all_event_plugins.empty() || !SessionEventData::hasEvents(session))
      return false;

    BeforeStatementEventData eventData(session);
    return eventData.callEventObservers();
  }

  bool EventObserver::afterStatement(Session &session)
  {
    if (all_event_plugins.empty() || !SessionEventData::hasEvents(session))
      return false;

    AfterStatementEventData eventData(session);
    return eventData.callEventObservers();
  }


} /* namespace plugin */
} /* namespace drizzled */
