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
 *
 * Barry Leslie
 *
 * 2010-05-12
 */

#pragma once

#include <drizzled/plugin/event_observer.h>

namespace drizzled
{

namespace plugin
{
class HelloEvents: public EventObserver
{
public:

  HelloEvents(std::string name_arg): EventObserver(name_arg), is_enabled(false), db_list(""), table_list(""){}
  ~HelloEvents();

  void registerTableEventsDo(TableShare &table_share, EventObserverList &observers);
  void registerSchemaEventsDo(const std::string &db, EventObserverList &observers);
  void registerSessionEventsDo(Session &session, EventObserverList &observers);

  bool observeEventDo(EventData &);

  // Some custom things for my plugin:
  void enable() { is_enabled= true;}
  void disable() { is_enabled= false;}
  bool isEnabled() const
  {
    return is_enabled;
  }

private:
  
  bool is_enabled;
  //----------------------
  std::string db_list;
  
public:
  void setDatabasesOfInterest(const char *list) 
  {
    db_list.assign(list);
  }
  
  const char *getDatabasesOfInterest() 
  {
    return db_list.c_str();
  }
  
private:
  bool isDatabaseInteresting(const std::string &db_name)
  {
    std::string list(db_list);
    list.append(",");
    
    std::string target(db_name);
    target.append(",");
    
    return (list.find(target) != std::string::npos);
  }
  
  //----------------------
  std::string table_list;
  
public:
  void setTablesOfInterest(const char *list) 
  {
    table_list.assign(list);
  }
  
  const char *getTablesOfInterest() 
  {
    return table_list.c_str();
  }
  
private:
  bool isTableInteresting(const std::string &table_name)
  {
    std::string list(table_list);
    list.append(",");
    
    std::string target(table_name);
    target.append(",");
    
    return (list.find(target) != std::string::npos);
  }


  //----------------------
  bool isSessionInteresting(Session &)
  {
    /* You could filter sessions of interest based on login
     * information.
     */
    return true;
  }

  
};
} /* namespace plugin */
} /* namespace drizzled */
