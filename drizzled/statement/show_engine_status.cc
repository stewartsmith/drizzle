/* -*- mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; -*-
 *  vim:expandtab:shiftwidth=2:tabstop=2:smarttab:
 *
 *  Copyright (C) 2009 Sun Microsystems
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

#include <drizzled/server_includes.h>
#include <drizzled/show.h>
#include <drizzled/session.h>
#include <drizzled/statement/show_engine_status.h>
#include <drizzled/plugin/client.h>
#include "drizzled/item/int.h"
#include "drizzled/item/empty_string.h"

namespace drizzled
{

static bool stat_print(Session *session, const char *type, uint32_t type_len,
                       const char *file, uint32_t file_len,
                       const char *status, uint32_t status_len)
{
  session->client->store(type, type_len);
  session->client->store(file, file_len);
  session->client->store(status, status_len);
  if (session->client->flush())
    return true;
  return false;
}

static bool show_status(Session *session, 
                        plugin::StorageEngine *engine, 
                        enum ha_stat_type stat)
{
  List<Item> field_list;
  bool result;

  field_list.push_back(new Item_empty_string("Type",10));
  field_list.push_back(new Item_empty_string("Name",FN_REFLEN));
  field_list.push_back(new Item_empty_string("Status",10));

  if (session->client->sendFields(&field_list))
    return true;

  result= engine->show_status(session, stat_print, stat) ? 1 : 0;

  if (!result)
    session->my_eof();
  return result;
}




bool statement::ShowEngineStatus::execute()
{
  drizzled::plugin::StorageEngine *engine;

  if ((engine= plugin::StorageEngine::findByName(session, engine_name)))
  {
    bool res= show_status(session, 
                          engine,
                          HA_ENGINE_STATUS);
    return res;
  }

  my_error(ER_UNKNOWN_STORAGE_ENGINE, MYF(0), engine_name.c_str());

  return true;
}

} /* namespace drizzled */

