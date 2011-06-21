/* Copyright (C) 2005 MySQL AB

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

#include <config.h>

#include <dlfcn.h>

#include <cstdio>
#include <string>
#include <vector>
#include <map>
#include <algorithm>
#include <iostream>

#include <boost/program_options.hpp>

#include <drizzled/option.h>
#include <drizzled/internal/m_string.h>

#include <drizzled/plugin.h>
#include <drizzled/module/load_list.h>
#include <drizzled/module/library.h>
#include <drizzled/module/registry.h>
#include <drizzled/module/option_context.h>
#include <drizzled/sql_parse.h>
#include <drizzled/show.h>
#include <drizzled/cursor.h>
#include <drizzled/set_var.h>
#include <drizzled/session.h>
#include <drizzled/item/null.h>
#include <drizzled/error.h>
#include <drizzled/gettext.h>
#include <drizzled/errmsg_print.h>
#include <drizzled/pthread_globals.h>
#include <drizzled/util/tokenize.h>
#include <drizzled/system_variables.h>

#include <boost/foreach.hpp>

/* FreeBSD 2.2.2 does not define RTLD_NOW) */
#ifndef RTLD_NOW
#define RTLD_NOW 1
#endif

namespace po=boost::program_options;

using namespace std;

/** These exist just to prevent symbols from being optimized out */
typedef drizzled::module::Manifest drizzled_builtin_list[];
extern drizzled_builtin_list PANDORA_BUILTIN_SYMBOLS_LIST;
extern drizzled_builtin_list PANDORA_BUILTIN_LOAD_SYMBOLS_LIST;
drizzled::module::Manifest *drizzled_builtins[]=
{
  PANDORA_BUILTIN_SYMBOLS_LIST, NULL
};
drizzled::module::Manifest *drizzled_load_builtins[]=
{
  PANDORA_BUILTIN_LOAD_SYMBOLS_LIST, NULL
};

namespace drizzled {
 

typedef vector<string> PluginOptions;
static PluginOptions opt_plugin_load;
static PluginOptions opt_plugin_add;
static PluginOptions opt_plugin_remove;
const char *builtin_plugins= PANDORA_BUILTIN_LIST;
const char *builtin_load_plugins= PANDORA_BUILTIN_LOAD_LIST;

/* Note that 'int version' must be the first field of every plugin
   sub-structure (plugin->info).
*/

static bool initialized= false;


static bool reap_needed= false;

/*
  write-lock on LOCK_system_variables_hash is required before modifying
  the following variables/structures
*/
static memory::Root plugin_mem_root(4096);
static uint32_t global_variables_dynamic_size= 0;


/*
  hidden part of opaque value passed to variable check functions.
  Used to provide a object-like structure to non C++ consumers.
*/
struct st_item_value_holder : public drizzle_value
{
  Item *item;
};

/* prototypes */
static void plugin_prune_list(vector<string> &plugin_list, const vector<string> &plugins_to_remove);
static bool plugin_load_list(module::Registry &registry,
                             memory::Root *tmp_root,
                             const set<string> &plugin_list,
                             po::options_description &long_options,
                             bool builtin= false);
static int test_plugin_options(memory::Root*, module::Module*, po::options_description&long_options);
static void unlock_variables(Session *session, drizzle_system_variables *vars);
static void cleanup_variables(drizzle_system_variables *vars);


/****************************************************************************
  Plugin support code
****************************************************************************/




/*
  NOTE
    Requires that a write-lock is held on LOCK_system_variables_hash
*/
static bool plugin_add(module::Registry &registry, memory::Root *tmp_root,
                       module::Library *library,
                       po::options_description &long_options)
{
  if (! initialized)
    return true;

  if (registry.find(library->getName()))
  {
    errmsg_printf(error::WARN, ER(ER_PLUGIN_EXISTS),
                  library->getName().c_str());
    return false;
  }

  /* Find plugin by name */
  const module::Manifest *manifest= library->getManifest();

  if (registry.find(manifest->name))
  {
    errmsg_printf(error::ERROR, 
                  _("Plugin '%s' contains the name '%s' in its manifest, which "
                    "has already been registered.\n"),
                  library->getName().c_str(),
                  manifest->name);
    return true;
  }

  module::Module* tmp= new module::Module(manifest, library);

  if (!test_plugin_options(tmp_root, tmp, long_options))
  {
    registry.add(tmp);
    return false;
  }
  errmsg_printf(error::ERROR, ER(ER_CANT_FIND_DL_ENTRY),
                library->getName().c_str());
  return true;
}


static void reap_plugins(module::Registry &registry)
{
  BOOST_FOREACH(module::Registry::ModuleMap::const_reference module, registry.getModulesMap())
    delete module.second;
}


static bool plugin_initialize(module::Registry &registry,
                              module::Module *module)
{
  assert(module->isInited == false);

  module::Context loading_context(registry, module);
  if (module->getManifest().init)
  {
    if (module->getManifest().init(loading_context))
    {
      errmsg_printf(error::ERROR,
                    _("Plugin '%s' init function returned error.\n"),
                    module->getName().c_str());
      return true;
    }
  }
  module->isInited= true;
  return false;
}

static void compose_plugin_options(vector<string> &target,
                                   vector<string> options)
{
  BOOST_FOREACH(vector<string>::reference it, options)
    tokenize(it, target, ",", true);
  BOOST_FOREACH(vector<string>::reference it, target)
    std::replace(it.begin(), it.end(), '-', '_');
}

void compose_plugin_add(vector<string> options)
{
  compose_plugin_options(opt_plugin_add, options);
}

void compose_plugin_remove(vector<string> options)
{
  compose_plugin_options(opt_plugin_remove, options);
}

void notify_plugin_load(string in_plugin_load)
{
  tokenize(in_plugin_load, opt_plugin_load, ",", true);
}

/*
  The logic is that we first load and initialize all compiled in plugins.
  From there we load up the dynamic types (assuming we have not been told to
  skip this part).

  Finally we initialize everything, aka the dynamic that have yet to initialize.
*/
bool plugin_init(module::Registry &registry,
                 po::options_description &long_options)
{
  if (initialized)
    return false;

  initialized= true;

  PluginOptions builtin_load_list;
  tokenize(builtin_load_plugins, builtin_load_list, ",", true);

  PluginOptions builtin_list;
  tokenize(builtin_plugins, builtin_list, ",", true);

  bool load_failed= false;

  if (opt_plugin_add.size() > 0)
  {
    for (PluginOptions::iterator iter= opt_plugin_add.begin();
         iter != opt_plugin_add.end();
         ++iter)
    {
      if (find(builtin_list.begin(),
               builtin_list.end(), *iter) != builtin_list.end())
      {
        builtin_load_list.push_back(*iter);
      }
      else
      {
        opt_plugin_load.push_back(*iter);
      }
    }
  }

  if (opt_plugin_remove.size() > 0)
  {
    plugin_prune_list(opt_plugin_load, opt_plugin_remove);
    plugin_prune_list(builtin_load_list, opt_plugin_remove);
  }

  memory::Root tmp_root(4096);
  /*
    First we register builtin plugins
  */
  const set<string> builtin_list_set(builtin_load_list.begin(),
                                     builtin_load_list.end());
  load_failed= plugin_load_list(registry, &tmp_root,
                                builtin_list_set, long_options, true);
  if (load_failed)
  {
    tmp_root.free_root(MYF(0));
    return true;
  }

  /* Uniquify the list */
  const set<string> plugin_list_set(opt_plugin_load.begin(),
                                    opt_plugin_load.end());
  
  /* Register all dynamic plugins */
  load_failed= plugin_load_list(registry, &tmp_root,
                                plugin_list_set, long_options);
  if (load_failed)
  {
    tmp_root.free_root(MYF(0));
    return true;
  }

  tmp_root.free_root(MYF(0));

  return false;
}

bool plugin_finalize(module::Registry &registry)
{
  /*
    Now we initialize all remaining plugins
  */
  BOOST_FOREACH(module::Registry::ModuleList::const_reference module, registry.getList())
  {
    if (not module->isInited && plugin_initialize(registry, module))
    {
      registry.remove(module);
      delete module;
      return true;
    }
  }
  BOOST_FOREACH(plugin::Plugin::map::value_type value, registry.getPluginsMap())
  {
    value.second->prime();
  }
  return false;
}

/*
  Window of opportunity for plugins to issue any queries with the database up and running but with no user's connected.
*/
void plugin_startup_window(module::Registry &registry, drizzled::Session &session)
{
  BOOST_FOREACH(plugin::Plugin::map::value_type value, registry.getPluginsMap())
  {
    value.second->startup(session);
  }
}

class PrunePlugin :
  public unary_function<string, bool>
{
  const string to_match;
  PrunePlugin();
  PrunePlugin& operator=(const PrunePlugin&);
public:
  explicit PrunePlugin(const string &match_in) :
    to_match(match_in)
  { }

  result_type operator()(const string &match_against)
  {
    return match_against == to_match;
  }
};

static void plugin_prune_list(vector<string> &plugin_list,
                              const vector<string> &plugins_to_remove)
{
  for (vector<string>::const_iterator iter= plugins_to_remove.begin();
       iter != plugins_to_remove.end();
       ++iter)
  {
    plugin_list.erase(remove_if(plugin_list.begin(),
                                plugin_list.end(),
                                PrunePlugin(*iter)),
                      plugin_list.end());
  }
}

/*
  called only by plugin_init()
*/
static bool plugin_load_list(module::Registry &registry,
                             memory::Root *tmp_root,
                             const set<string> &plugin_list,
                             po::options_description &long_options,
                             bool builtin)
{
  BOOST_FOREACH(const string& plugin_name, plugin_list)
  {
    module::Library* library= registry.addLibrary(plugin_name, builtin);
    if (library == NULL)
    {
      errmsg_printf(error::ERROR,
                    _("Couldn't load plugin library named '%s'.\n"),
                    plugin_name.c_str());
      return true;
    }

    tmp_root->free_root(MYF(memory::MARK_BLOCKS_FREE));
    if (plugin_add(registry, tmp_root, library, long_options))
    {
      registry.removeLibrary(plugin_name);
      errmsg_printf(error::ERROR,
                    _("Couldn't load plugin named '%s'.\n"),
                    plugin_name.c_str());
      return true;
    }
  }
  return false;
}

void module_shutdown(module::Registry &registry)
{
  if (initialized)
  {
    reap_needed= true;

    reap_plugins(registry);
    unlock_variables(NULL, &global_system_variables);
    unlock_variables(NULL, &max_system_variables);

    cleanup_variables(&global_system_variables);
    cleanup_variables(&max_system_variables);

    initialized= 0;
  }

  /* Dispose of the memory */
  plugin_mem_root.free_root(MYF(0));

  global_variables_dynamic_size= 0;
}


/****************************************************************************
  System Variables support
****************************************************************************/



void plugin_sessionvar_init(Session *session)
{
  session->variables.storage_engine= NULL;
  cleanup_variables(&session->variables);

  session->variables= global_system_variables;
  session->variables.storage_engine= NULL;

  /* we are going to allocate these lazily */
  session->variables.dynamic_variables_version= 0;
  session->variables.dynamic_variables_size= 0;
  session->variables.dynamic_variables_ptr= 0;

  session->variables.storage_engine= global_system_variables.storage_engine;
}


/*
  Unlocks all system variables which hold a reference
*/
static void unlock_variables(Session *, struct drizzle_system_variables *vars)
{
  vars->storage_engine= NULL;
}


/*
  Frees memory used by system variables

  Unlike plugin_vars_free_values() it frees all variables of all plugins,
  it's used on shutdown.
*/
static void cleanup_variables(drizzle_system_variables *vars)
{
  assert(vars->storage_engine == NULL);

  free(vars->dynamic_variables_ptr);
  vars->dynamic_variables_ptr= NULL;
  vars->dynamic_variables_size= 0;
  vars->dynamic_variables_version= 0;
}


void plugin_sessionvar_cleanup(Session *session)
{
  unlock_variables(session, &session->variables);
  cleanup_variables(&session->variables);
}



/*
  SYNOPSIS
    test_plugin_options()
    tmp_root                    temporary scratch space
    plugin                      internal plugin structure
    default_enabled             default plugin enable status
  RETURNS:
    0 SUCCESS - plugin should be enabled/loaded
  NOTE:
    Requires that a write-lock is held on LOCK_system_variables_hash
*/
static int test_plugin_options(memory::Root *,
                               module::Module *test_module,
                               po::options_description &long_options)
{

  if (test_module->getManifest().init_options != NULL)
  {
    string plugin_section_title("Options used by ");
    plugin_section_title.append(test_module->getName());
    po::options_description module_options(plugin_section_title);
    module::option_context opt_ctx(test_module->getName(),
                                   module_options.add_options());
    test_module->getManifest().init_options(opt_ctx);
    long_options.add(module_options);
  }

  return 0;
}


/****************************************************************************
  Help Verbose text with Plugin System Variables
****************************************************************************/

class OptionCmp
{
public:
  bool operator() (const option &a, const option &b)
  {
    return my_strcasecmp(&my_charset_utf8_general_ci, a.name, b.name);
  }
};


void my_print_help_inc_plugins(option *main_options)
{
  module::Registry &registry= module::Registry::singleton();
  vector<option> all_options;
  memory::Root mem_root(4096);


  if (initialized)
  {
    std::map<std::string, module::Module *>::const_iterator modules=
      registry.getModulesMap().begin();
    
    while (modules != registry.getModulesMap().end())
    {
      module::Module *p= (*modules).second;
      ++modules;

      /* If we have an init_options function, we are registering
         commmand line options that way, so don't do them this way */
      if (p->getManifest().init_options != NULL)
        continue;

    }
  }

  for (;main_options->id; main_options++)
  {
    if (main_options->comment)
    {
      all_options.push_back(*main_options);
    }
  }

  /** 
   * @TODO: Fix the option building so that it doens't break sort
   *
   * sort(all_options.begin(), all_options.end(), OptionCmp());
   */

  /* main_options now points to the empty option terminator */
  all_options.push_back(*main_options);

  my_print_help(&*(all_options.begin()));

  mem_root.free_root(MYF(0));
}

} /* namespace drizzled */


