/* - mode: c; c-basic-offset: 2; indent-tabs-mode: nil; -*-
 *  vim:expandtab:shiftwidth=2:tabstop=2:smarttab:
 *
 *  Copyright (C) 2011 David Shrewsbury
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
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

#ifndef PLUGIN_SLAVE_QUEUE_MANAGER_H
#define PLUGIN_SLAVE_QUEUE_MANAGER_H

#include "drizzled/session.h"

namespace drizzled
{
  namespace message
  {
    class Transaction;
  }
}

namespace slave
{

class QueueManager
{
public:
  QueueManager() :
    _check_interval(5),
    _schema("test"),
    _table("t1")
  { }

  /**
   * Method to be supplied to an applier thread.
   */
  void processQueue(void);

  void setCheckInterval(uint32_t seconds)
  {
    _check_interval= seconds;
  }

  uint32_t getCheckInterval()
  {
    return _check_interval;
  }

  void setTable(const std::string &table)
  {
    _table= table;
  }

  const std::string &getTable()
  {
    return _table;
  }

  void setSchema(const std::string &schema)
  {
    _schema= schema;
  }

  const std::string &getSchema()
  {
    return _schema;
  }

private:
  /** Number of seconds to sleep between checking queue for messages */
  uint32_t _check_interval;

  /** Name of the table containing the message queue */
  std::string _schema;
  std::string _table;

  bool executeMessage(const drizzled::message::Transaction &transaction);
};

} /* namespace slave */

#endif /* PLUGIN_SLAVE_QUEUE_MANAGER_H */