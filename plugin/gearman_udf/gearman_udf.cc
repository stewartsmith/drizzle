/* Copyright (C) 2009 Eric Day

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA */

#include <drizzled/server_includes.h>
#include <drizzled/sql_udf.h>
#include <drizzled/item/func.h>
#include <drizzled/function/str/strfunc.h>

#include <list>
#include <string>

#include <libgearman/gearman.h>

using namespace std;

class GearmanFunctionMap
{
  map<string, gearman_client_st *> functionMap;
  pthread_mutex_t lock;
  string errorString;

public:
  GearmanFunctionMap()
  {
    (void) pthread_mutex_init(&lock, NULL);
  }

  bool add(string function)
  {
    pthread_mutex_lock(&lock);

    if (functionMap[function] == NULL)
    {
      functionMap[function]= gearman_client_create(NULL);
      if (functionMap[function] == NULL)
      {
        errorString= "gearman_client_create() failed.";
        pthread_mutex_unlock(&lock);
        return false;
      }
    }

printf("\nSize: %u\n", (uint32_t)functionMap.size());

    pthread_mutex_unlock(&lock);
    return true;
  }
};

static GearmanFunctionMap _functionMap;

class Item_func_gman_servers_set :public Item_str_func
{
  String buffer;
public:
  Item_func_gman_servers_set():Item_str_func(){}
  void fix_length_and_dec() {}
  const char *func_name() const{return "gman_do";}
  String *val_str(String *);
};

String *Item_func_gman_servers_set::val_str(String *str)
{
  String *res;

  if (!(res= args[0]->val_str(str)))
  {
    null_value= 1;
    return 0;
  }

  (void) _functionMap.add(string(res->ptr()));

  buffer.realloc(res->length());
  strcpy(buffer.ptr(), res->ptr());
  buffer.length(res->length());
  null_value= 0;
  return &buffer;
}

class Item_func_gman_do :public Item_str_func
{
  String buffer;
public:
  Item_func_gman_do():Item_str_func(){}
  void fix_length_and_dec() { max_length=10; }
  const char *func_name() const{return "gman_do";}
  String *val_str(String *);
};

String *Item_func_gman_do::val_str(String *str)
{
  String *res;

  if (!(res= args[0]->val_str(str)))
  {
    null_value= 1;
    return 0;
  }

  (void) _functionMap.add(string(res->ptr()));

  buffer.realloc(res->length());
  strcpy(buffer.ptr(), res->ptr());
  buffer.length(res->length());
  null_value= 0;
  return &buffer;
}

Create_function<Item_func_gman_servers_set> gman_servers_set(string("gman_servers_set"));
Create_function<Item_func_gman_do> gman_do(string("gman_do"));

static int gearman_udf_plugin_init(PluginRegistry &registry)
{
  registry.add(&gman_servers_set);
  registry.add(&gman_do);
  return 0;
}

static int gearman_udf_plugin_deinit(PluginRegistry &registry)
{
  registry.remove(&gman_do);
  registry.remove(&gman_servers_set);
  return 0;
}

drizzle_declare_plugin(gearman)
{
  "gearman",
  "0.1",
  "Eric Day",
  "UDFs for Gearman Client",
  PLUGIN_LICENSE_BSD,
  gearman_udf_plugin_init, /* Plugin Init */
  gearman_udf_plugin_deinit, /* Plugin Deinit */
  NULL,   /* status variables */
  NULL,   /* system variables */
  NULL    /* config options */
}
drizzle_declare_plugin_end;
