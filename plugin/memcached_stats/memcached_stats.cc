/* 
 * Copyright (c) 2009, Padraig O'Sullivan
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
 *   * Neither the name of Padraig O'Sullivan nor the names of its contributors
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

#include "config.h"
#include "drizzled/show.h"
#include "drizzled/gettext.h"
#include "drizzled/plugin/info_schema_table.h"

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
static vector<const plugin::ColumnInfo *> memcached_stats_columns;
static vector<const plugin::ColumnInfo *> memcached_analysis_columns;

/*
 * Methods for I_S tables.
 */
static plugin::InfoSchemaMethods *memcached_stats_methods= NULL;
static plugin::InfoSchemaMethods *memcached_analysis_methods= NULL;

/*
 * I_S tables.
 */
static plugin::InfoSchemaTable *memcached_stats_table= NULL;
static plugin::InfoSchemaTable *memcached_analysis_table= NULL;

/*
 * System variable related variables.
 */
static char *sysvar_memcached_servers= NULL;

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
  memcached_stats_table= new(std::nothrow) plugin::InfoSchemaTable("MEMCACHED_STATS",
                                                           memcached_stats_columns,
                                                           -1, -1, false, false, 0,
                                                           memcached_stats_methods);
  if (! memcached_stats_table)
  {
    return true;
  }

  memcached_analysis_table= 
    new(std::nothrow) plugin::InfoSchemaTable("MEMCACHED_ANALYSIS",
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
    return 1;
  }

  if (initColumns())
  {
    return 1;
  }

  if (initMemcachedTables())
  {
    return 1;
  }

  SysvarHolder &sysvar_holder= SysvarHolder::singleton();
  sysvar_holder.setServersString(sysvar_memcached_servers);

  /* we are good to go */
  registry.add(memcached_stats_table);
  registry.add(memcached_analysis_table);

  return 0;
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

  return 0;
}

static int check_memc_servers(Session *,
                              drizzle_sys_var *,
                              void *save,
                              drizzle_value *value)
{
  char buff[STRING_BUFFER_USUAL_SIZE];
  int len= sizeof(buff);
  const char *input= value->val_str(value, buff, &len);

  if (input)
  {
    SysvarHolder &sysvar_holder= SysvarHolder::singleton();
    sysvar_holder.setServersStringVar(input);
    *(bool *) save= (bool) true;
    return 0;
  }

  *(bool *) save= (bool) false;
  return 1;
}

static void set_memc_servers(Session *,
                             drizzle_sys_var *,
                             void *var_ptr,
                             const void *save)
{
  if (*(bool *) save != false)
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
                          ""); /* default value */

static drizzle_sys_var *system_variables[]=
{
  DRIZZLE_SYSVAR(servers),
  NULL
};

DRIZZLE_DECLARE_PLUGIN
{
  DRIZZLE_VERSION_ID,
  "memcached_stats",
  "1.0",
  "Padraig O'Sullivan",
  N_("Memcached Stats as I_S tables"),
  PLUGIN_LICENSE_BSD,
  init,   /* Plugin Init      */
  deinit, /* Plugin Deinit    */
  NULL,   /* status variables */
  system_variables, /* system variables */
  NULL    /* config options   */
}
DRIZZLE_DECLARE_PLUGIN_END;
