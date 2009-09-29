/*
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

#include "drizzled/server_includes.h"
#include "drizzled/show.h"
#include "drizzled/gettext.h"
#include "drizzled/info_schema.h"

#include "stats_table.h"
#include "analysis_table.h"
#include "sysvar_holder.h"

#include <string>
#include <map>

using namespace std;
using namespace drizzled;

/*
 * Vectors of columns for I_S tables.
 */
static vector<const ColumnInfo *> memcached_stats_columns;
static vector<const ColumnInfo *> memcached_analysis_columns;

/*
 * Methods for I_S tables.
 */
static InfoSchemaMethods *memcached_stats_methods= NULL;
static InfoSchemaMethods *memcached_analysis_methods= NULL;

/*
 * I_S tables.
 */
static InfoSchemaTable *memcached_stats_table= NULL;
static InfoSchemaTable *memcached_analysis_table= NULL;

/*
 * System variable related variables.
 */
static char *sysvar_memcached_servers= NULL;
static const char DEFAULT_SERVERS_STRING[]= "localhost:11211, localhost:11212";

/**
 * Populate the vectors of columns for each I_S table.
 *
 * @return false on success; true on failure.
 */
static bool initColumns()
{
  if (createMemcachedStatsColumns(memcached_stats_columns))
  {
    return true;
  }

  if (createMemcachedAnalysisColumns(memcached_analysis_columns))
  {
    return true;
  }

  return false;
}

/**
 * Clear the vectors of columns for each I_S table.
 */
static void cleanupColumns()
{
  clearMemcachedColumns(memcached_stats_columns);
  clearMemcachedColumns(memcached_analysis_columns);
}

/**
 * Initialize the methods for each I_S table.
 *
 * @return false on success; true on failure
 */
static bool initMethods()
{
  memcached_stats_methods= new(std::nothrow) 
    MemcachedStatsISMethods();
  if (! memcached_stats_methods)
  {
    return true;
  }

  memcached_analysis_methods= new(std::nothrow) 
    MemcachedAnalysisISMethods();
  if (! memcached_analysis_methods)
  {
    return true;
  }

  return false;
}

/**
 * Delete memory allocated for the I_S table methods.
 */
static void cleanupMethods()
{
  delete memcached_stats_methods;
  delete memcached_analysis_methods;
}

/**
 * Initialize the I_S tables related to memcached.
 *
 * @return false on success; true on failure
 */
static bool initMemcachedTables()
{
  memcached_stats_table= new(std::nothrow) InfoSchemaTable("MEMCACHED_STATS",
                                                           memcached_stats_columns,
                                                           -1, -1, false, false, 0,
                                                           memcached_stats_methods);
  if (! memcached_stats_table)
  {
    return true;
  }

  memcached_analysis_table= 
    new(std::nothrow) InfoSchemaTable("MEMCACHED_ANALYSIS",
                                      memcached_analysis_columns,
                                      -1, -1, false, false, 0,
                                      memcached_analysis_methods);
  if (! memcached_analysis_table)
  {
    return true;
  }

  return false;
}

/**
 * Delete memory allocated for the I_S tables.
 */
static void cleanupMemcachedTables()
{
  delete memcached_stats_table;
  delete memcached_analysis_table;
}

/**
 * Initialize the memcached stats plugin.
 *
 * @param[in] registry the drizzled::plugin::Registry singleton
 * @return false on success; true on failure.
 */
static int init(plugin::Registry &registry)
{
  if (initMethods())
  {
    return true;
  }

  if (initColumns())
  {
    return true;
  }

  if (initMemcachedTables())
  {
    return true;
  }

  SysvarHolder &sysvar_holder= SysvarHolder::singleton();
  sysvar_holder.setServersString(sysvar_memcached_servers);

  /* we are good to go */
  registry.add(memcached_stats_table);
  registry.add(memcached_analysis_table);

  return false;
}

/**
 * Clean up the memcached stats plugin.
 *
 * @param[in] registry the drizzled::plugin::Registry singleton
 * @return false on success; true on failure
 */
static int deinit(plugin::Registry &registry)
{
  registry.remove(memcached_stats_table);
  registry.remove(memcached_analysis_table);

  cleanupMethods();
  cleanupColumns();
  cleanupMemcachedTables();

  return false;
}

static int check_memc_servers(Session *,
                              struct st_mysql_sys_var *,
                              void *,
                              struct st_mysql_value *value)
{
  char buff[STRING_BUFFER_USUAL_SIZE];
  int len= sizeof(buff);
  const char *input= value->val_str(value, buff, &len);

  if (input)
  {
    SysvarHolder &sysvar_holder= SysvarHolder::singleton();
    sysvar_holder.setServersStringVar(input);
    return 0;
  }

  return 1;
}

static void set_memc_servers(Session *,
                             struct st_mysql_sys_var *,
                             void *var_ptr,
                             const void *save)
{
  if (*(bool *) save != true)
  {
    SysvarHolder &sysvar_holder= SysvarHolder::singleton();
    sysvar_holder.updateServersSysvar((const char **) var_ptr);
  }
}

static DRIZZLE_SYSVAR_STR(servers,
                          sysvar_memcached_servers,
                          PLUGIN_VAR_OPCMDARG,
                          N_("List of memcached servers."),
                          check_memc_servers, /* check func */
                          set_memc_servers, /* update func */
                          DEFAULT_SERVERS_STRING);

static struct st_mysql_sys_var *system_variables[]=
{
  DRIZZLE_SYSVAR(servers),
  NULL
};

drizzle_declare_plugin(memcached_stats)
{
  "memcached_stats",
  "0.2",
  "Padraig O'Sullivan",
  N_("Memcached Stats as I_S tables"),
  PLUGIN_LICENSE_GPL,
  init,   /* Plugin Init      */
  deinit, /* Plugin Deinit    */
  NULL,   /* status variables */
  system_variables, /* system variables */
  NULL    /* config options   */
}
drizzle_declare_plugin_end;
