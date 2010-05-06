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

#ifndef PLUGIN_HELLO_EVENTS_H
#define PLUGIN_HELLO_EVENTS_H

#include <drizzled/plugin/event_observer.h>

namespace drizzled
{

namespace plugin
{
class HelloEvents: public EventObserver
{
public:

  HelloEvents(std::string name_arg): EventObserver(name_arg), is_enabled(false), db_list(""), table_list(""){}

  void registerTableEvents(TableShare &table_share, EventObserverList &observers);
  void registerSchemaEvents(const std::string &db, EventObserverList &observers);
  void registerSessionEvents(Session &session, EventObserverList &observers);

  bool observeEvent(EventData &);

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
#endif /* PLUGIN_HELLO_EVENTS_H */
