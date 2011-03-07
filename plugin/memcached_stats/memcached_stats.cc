/* 
 * Copyright (C) 2009, Padraig O'Sullivan
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

#include <config.h>
#include <drizzled/show.h>
#include <drizzled/gettext.h>
#include <boost/program_options.hpp>
#include <drizzled/module/option_map.h>
#include "stats_table.h"
#include "analysis_table.h"
#include "sysvar_holder.h"

#include <string>
#include <map>

namespace po=boost::program_options;

namespace drizzle_plugin
{


/*
 * DATA_DICTIONARY tables.
 */
static AnalysisTableTool *analysis_table_tool; 

static StatsTableTool *stats_table_tool;

/*
 * System variable related variables.
 */
static std::string sysvar_memcached_servers;

/**
 * Initialize the memcached stats plugin.
 *
 * @param[in] registry the drizzled::plugin::Registry singleton
 * @return false on success; true on failure.
 */
static int init(drizzled::module::Context &context)
{
  const drizzled::module::option_map &vm= context.getOptions();

  /* we are good to go */
  stats_table_tool= new StatsTableTool; 
  context.add(stats_table_tool);

  analysis_table_tool= new AnalysisTableTool;
  context.add(analysis_table_tool);

  context.registerVariable(new sys_var_std_string("servers",
                                                  sysvar_memcached_servers));
                          
  return 0;
}

static void init_options(drizzled::module::option_context &context)
{
  context("servers",
          po::value<std::string>()->default_value(""),
          _("List of memcached servers."));
}

} /* namespace drizzle_plugin */

DRIZZLE_DECLARE_PLUGIN
{
  DRIZZLE_VERSION_ID,
  "memcached_stats",
  "1.0",
  "Padraig O'Sullivan",
  N_("Memcached Stats as I_S tables"),
  PLUGIN_LICENSE_BSD,
  drizzle_plugin::init,   /* Plugin Init      */
  NULL, /* depends */
  drizzle_plugin::init_options    /* config options   */
}
DRIZZLE_DECLARE_PLUGIN_END;
