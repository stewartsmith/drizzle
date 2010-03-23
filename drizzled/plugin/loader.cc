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
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA */

#include "config.h"

#include <dlfcn.h>

#include <string>
#include <vector>
#include <map>
#include <algorithm>

#include "drizzled/my_getopt.h"
#include "drizzled/my_hash.h"
#include "drizzled/internal/m_string.h"

#include "drizzled/plugin.h"
#include "drizzled/plugin/load_list.h"
#include "drizzled/sql_parse.h"
#include "drizzled/show.h"
#include "drizzled/cursor.h"
#include "drizzled/set_var.h"
#include "drizzled/session.h"
#include "drizzled/item/null.h"
#include "drizzled/plugin/registry.h"
#include "drizzled/error.h"
#include "drizzled/gettext.h"
#include "drizzled/errmsg_print.h"
#include "drizzled/plugin/library.h"
#include "drizzled/strfunc.h"
#include "drizzled/pthread_globals.h"
#include "drizzled/util/tokenize.h"

/* FreeBSD 2.2.2 does not define RTLD_NOW) */
#ifndef RTLD_NOW
#define RTLD_NOW 1
#endif

using namespace std;

typedef drizzled::plugin::Manifest drizzled_builtin_plugin[];
extern drizzled_builtin_plugin PANDORA_BUILTIN_LIST;
static drizzled::plugin::Manifest *drizzled_builtins[]=
{
  PANDORA_BUILTIN_LIST, NULL
};

namespace drizzled
{
 

class sys_var_pluginvar;
static vector<sys_var_pluginvar *> plugin_sysvar_vec;

char *opt_plugin_add= NULL;
char *opt_plugin_remove= NULL;
char *opt_plugin_load= NULL;
char *opt_plugin_dir_ptr;
char opt_plugin_dir[FN_REFLEN];
const char *opt_plugin_load_default= PANDORA_PLUGIN_LIST;

/* Note that 'int version' must be the first field of every plugin
   sub-structure (plugin->info).
*/

static bool initialized= false;


static bool reap_needed= false;

/*
  write-lock on LOCK_system_variables_hash is required before modifying
  the following variables/structures
*/
static memory::Root plugin_mem_root;
static uint32_t global_variables_dynamic_size= 0;
static HASH bookmark_hash;


/*
  hidden part of opaque value passed to variable check functions.
  Used to provide a object-like structure to non C++ consumers.
*/
struct st_item_value_holder : public drizzle_value
{
  Item *item;
};


/*
  stored in bookmark_hash, this structure is never removed from the
  hash and is used to mark a single offset for a session local variable
  even if plugins have been uninstalled and reinstalled, repeatedly.
  This structure is allocated from plugin_mem_root.

  The key format is as follows:
    1 byte         - variable type code
    name_len bytes - variable name
    '\0'           - end of key
*/
struct st_bookmark
{
  uint32_t name_len;
  int offset;
  uint32_t version;
  char key[1];
};


/*
  sys_var class for access to all plugin variables visible to the user
*/
class sys_var_pluginvar: public sys_var
{
public:
  plugin::Module *plugin;
  drizzle_sys_var *plugin_var;

  sys_var_pluginvar(const std::string name_arg,
                    drizzle_sys_var *plugin_var_arg)
    :sys_var(name_arg), plugin_var(plugin_var_arg) {}
  sys_var_pluginvar *cast_pluginvar() { return this; }
  bool is_readonly() const { return plugin_var->flags & PLUGIN_VAR_READONLY; }
  bool check_type(sql_var_t type)
  { return !(plugin_var->flags & PLUGIN_VAR_SessionLOCAL) && type != OPT_GLOBAL; }
  bool check_update_type(Item_result type);
  SHOW_TYPE show_type();
  unsigned char* real_value_ptr(Session *session, sql_var_t type);
  TYPELIB* plugin_var_typelib(void);
  unsigned char* value_ptr(Session *session, sql_var_t type,
                           const LEX_STRING *base);
  bool check(Session *session, set_var *var);
  bool check_default(sql_var_t)
    { return is_readonly(); }
  void set_default(Session *session, sql_var_t);
  bool update(Session *session, set_var *var);
};


/* prototypes */
static void plugin_prune_list(vector<string> &plugin_list,
                              const vector<string> &plugins_to_remove);
static bool plugin_load_list(plugin::Registry &registry,
                             memory::Root *tmp_root, int *argc, char **argv,
                             const set<string> &plugin_list);
static int test_plugin_options(memory::Root *, plugin::Module *,
                               int *, char **);
static void unlock_variables(Session *session, struct system_variables *vars);
static void cleanup_variables(Session *session, struct system_variables *vars);
static void plugin_vars_free_values(sys_var *vars);

/* declared in set_var.cc */
extern sys_var *intern_find_sys_var(const char *str, uint32_t length, bool no_error);
extern bool throw_bounds_warning(Session *session, bool fixed, bool unsignd,
                                 const std::string &name, int64_t val);

static bool throw_bounds_warning(Session *session, bool fixed, bool unsignd,
                                 const char *name, int64_t val)
{
  const std::string name_str(name);
  return throw_bounds_warning(session, fixed, unsignd, name_str, val);
}

/****************************************************************************
  Value type thunks, allows the C world to play in the C++ world
****************************************************************************/

static int item_value_type(drizzle_value *value)
{
  switch (((st_item_value_holder*)value)->item->result_type()) {
  case INT_RESULT:
    return DRIZZLE_VALUE_TYPE_INT;
  case REAL_RESULT:
    return DRIZZLE_VALUE_TYPE_REAL;
  default:
    return DRIZZLE_VALUE_TYPE_STRING;
  }
}

static const char *item_val_str(drizzle_value *value,
                                char *buffer, int *length)
{
  String str(buffer, *length, system_charset_info), *res;
  if (!(res= ((st_item_value_holder*)value)->item->val_str(&str)))
    return NULL;
  *length= res->length();
  if (res->c_ptr_quick() == buffer)
    return buffer;

  /*
    Lets be nice and create a temporary string since the
    buffer was too small
  */
  return current_session->strmake(res->c_ptr_quick(), res->length());
}


static int item_val_int(drizzle_value *value, int64_t *buf)
{
  Item *item= ((st_item_value_holder*)value)->item;
  *buf= item->val_int();
  if (item->is_null())
    return 1;
  return 0;
}


static int item_val_real(drizzle_value *value, double *buf)
{
  Item *item= ((st_item_value_holder*)value)->item;
  *buf= item->val_real();
  if (item->is_null())
    return 1;
  return 0;
}


/****************************************************************************
  Plugin support code
****************************************************************************/




/*
  NOTE
    Requires that a write-lock is held on LOCK_system_variables_hash
*/
static bool plugin_add(plugin::Registry &registry, memory::Root *tmp_root,
                       plugin::Library *library,
                       int *argc, char **argv)
{
  if (! initialized)
    return true;

  if (registry.find(library->getName()))
  {
    errmsg_printf(ERRMSG_LVL_WARN, ER(ER_PLUGIN_EXISTS),
                  library->getName().c_str());
    return false;
  }

  plugin::Module *tmp= NULL;
  /* Find plugin by name */
  const plugin::Manifest *manifest= library->getManifest();

  if (registry.find(manifest->name))
  {
    errmsg_printf(ERRMSG_LVL_ERROR, 
                  _("Plugin '%s' contains the name '%s' in its manifest, which "
                    "has already been registered.\n"),
                  library->getName().c_str(),
                  manifest->name);
    return true;
  }

  tmp= new (std::nothrow) plugin::Module(manifest, library);
  if (tmp == NULL)
    return true;

  if (!test_plugin_options(tmp_root, tmp, argc, argv))
  {
    registry.add(tmp);
    return false;
  }
  errmsg_printf(ERRMSG_LVL_ERROR, ER(ER_CANT_FIND_DL_ENTRY),
                library->getName().c_str());
  return true;
}


static void delete_module(plugin::Registry &registry, plugin::Module *module)
{
  plugin::Manifest manifest= module->getManifest();

  if (module->isInited)
  {
    if (manifest.deinit)
      manifest.deinit(registry);
  }

  /* Free allocated strings before deleting the plugin. */
  plugin_vars_free_values(module->system_vars);
  module->isInited= false;
  pthread_rwlock_wrlock(&LOCK_system_variables_hash);
  mysql_del_sys_var_chain(module->system_vars);
  pthread_rwlock_unlock(&LOCK_system_variables_hash);
  delete module;
}


static void reap_plugins(plugin::Registry &registry)
{
  plugin::Module *module;

  std::map<std::string, plugin::Module *>::const_iterator modules=
    registry.getModulesMap().begin();
    
  while (modules != registry.getModulesMap().end())
  {
    module= (*modules).second;
    delete_module(registry, module);
    ++modules;
  }
  drizzle_del_plugin_sysvar();
}


static void plugin_initialize_vars(plugin::Module *module)
{
  /*
    set the plugin attribute of plugin's sys vars so they are pointing
    to the active plugin
  */
  if (module->system_vars)
  {
    sys_var_pluginvar *var= module->system_vars->cast_pluginvar();
    for (;;)
    {
      var->plugin= module;
      if (! var->getNext())
        break;
      var= var->getNext()->cast_pluginvar();
    }
  }
}


static bool plugin_initialize(plugin::Registry &registry,
                              plugin::Module *module)
{
  assert(module->isInited == false);

  registry.setCurrentModule(module);
  if (module->getManifest().init)
  {
    if (module->getManifest().init(registry))
    {
      errmsg_printf(ERRMSG_LVL_ERROR,
                    _("Plugin '%s' init function returned error.\n"),
                    module->getName().c_str());
      return true;
    }
  }
  registry.clearCurrentModule();
  module->isInited= true;


  return false;
}


static unsigned char *get_bookmark_hash_key(const unsigned char *buff,
                                            size_t *length, bool)
{
  struct st_bookmark *var= (st_bookmark *)buff;
  *length= var->name_len + 1;
  return (unsigned char*) var->key;
}


/*
  The logic is that we first load and initialize all compiled in plugins.
  From there we load up the dynamic types (assuming we have not been told to
  skip this part).

  Finally we initialize everything, aka the dynamic that have yet to initialize.
*/
bool plugin_init(plugin::Registry &registry,
                 int *argc, char **argv,
                 bool skip_init)
{
  plugin::Manifest **builtins;
  plugin::Manifest *manifest;
  plugin::Module *module;
  memory::Root tmp_root;

  if (initialized)
    return false;

  init_alloc_root(&plugin_mem_root, 4096);
  init_alloc_root(&tmp_root, 4096);

  if (hash_init(&bookmark_hash, &my_charset_bin, 16, 0, 0,
                  get_bookmark_hash_key, NULL, HASH_UNIQUE))
  {
    free_root(&tmp_root, MYF(0));
    return true;
  }


  initialized= 1;

  /*
    First we register builtin plugins
  */
  for (builtins= drizzled_builtins; *builtins; builtins++)
  {
    manifest= *builtins;
    if (manifest->name != NULL)
    {
      module= new (std::nothrow) plugin::Module(manifest);
      if (module == NULL)
        return true;

      free_root(&tmp_root, MYF(memory::MARK_BLOCKS_FREE));
      if (test_plugin_options(&tmp_root, module, argc, argv))
        continue;

      registry.add(module);

      plugin_initialize_vars(module);

      if (! skip_init)
      {
        if (plugin_initialize(registry, module))
        {
          free_root(&tmp_root, MYF(0));
          return true;
        }
      }
    }
  }


  bool load_failed= false;
  vector<string> plugin_list;
  if (opt_plugin_load)
  {
    tokenize(opt_plugin_load, plugin_list, ",", true);
  }
  else
  {
    tokenize(opt_plugin_load_default, plugin_list, ",", true);
  }
  if (opt_plugin_add)
  {
    tokenize(opt_plugin_add, plugin_list, ",", true);
  }

  if (opt_plugin_remove)
  {
    vector<string> plugins_to_remove;
    tokenize(opt_plugin_remove, plugins_to_remove, ",", true);
    plugin_prune_list(plugin_list, plugins_to_remove);
  }

  /* Uniquify the list */
  const set<string> plugin_list_set(plugin_list.begin(), plugin_list.end());
  
  /* Register all dynamic plugins */
  load_failed= plugin_load_list(registry, &tmp_root, argc, argv,
                                plugin_list_set);
  if (load_failed)
  {
    free_root(&tmp_root, MYF(0));
    return true;
  }

  if (skip_init)
  {
    free_root(&tmp_root, MYF(0));
    return false;
  }

  /*
    Now we initialize all remaining plugins
  */
  std::map<std::string, plugin::Module *>::const_iterator modules=
    registry.getModulesMap().begin();
    
  while (modules != registry.getModulesMap().end())
  {
    module= (*modules).second;
    ++modules;
    if (module->isInited == false)
    {
      plugin_initialize_vars(module);

      if (plugin_initialize(registry, module))
        delete_module(registry, module);
    }
  }


  free_root(&tmp_root, MYF(0));

  return false;
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
static bool plugin_load_list(plugin::Registry &registry,
                             memory::Root *tmp_root, int *argc, char **argv,
                             const set<string> &plugin_list)
{
  plugin::Library *library= NULL;

  for (set<string>::const_iterator iter= plugin_list.begin();
       iter != plugin_list.end();
       ++iter)
  {
    const string plugin_name(*iter);
    library= registry.addLibrary(plugin_name);
    if (library == NULL)
    {
      errmsg_printf(ERRMSG_LVL_ERROR,
                    _("Couldn't load plugin library named '%s'.\n"),
                    plugin_name.c_str());
      return true;
    }

    free_root(tmp_root, MYF(memory::MARK_BLOCKS_FREE));
    if (plugin_add(registry, tmp_root, library, argc, argv))
    {
      registry.removeLibrary(plugin_name);
      errmsg_printf(ERRMSG_LVL_ERROR,
                    _("Couldn't load plugin named '%s'.\n"),
                    plugin_name.c_str());
      return true;

    }
  }
  return false;
}


void plugin_shutdown(plugin::Registry &registry)
{

  if (initialized)
  {
    reap_needed= true;

    reap_plugins(registry);
    unlock_variables(NULL, &global_system_variables);
    unlock_variables(NULL, &max_system_variables);

    cleanup_variables(NULL, &global_system_variables);
    cleanup_variables(NULL, &max_system_variables);

    initialized= 0;
  }

  /* Dispose of the memory */

  hash_free(&bookmark_hash);
  free_root(&plugin_mem_root, MYF(0));

  global_variables_dynamic_size= 0;
}

/****************************************************************************
  Internal type declarations for variables support
****************************************************************************/

#undef DRIZZLE_SYSVAR_NAME
#define DRIZZLE_SYSVAR_NAME(name) name
#define PLUGIN_VAR_TYPEMASK 0x007f

static const uint32_t EXTRA_OPTIONS= 1; /* handle the NULL option */

typedef DECLARE_DRIZZLE_SYSVAR_BASIC(sysvar_bool_t, bool);
typedef DECLARE_DRIZZLE_SessionVAR_BASIC(sessionvar_bool_t, bool);
typedef DECLARE_DRIZZLE_SYSVAR_BASIC(sysvar_str_t, char *);
typedef DECLARE_DRIZZLE_SessionVAR_BASIC(sessionvar_str_t, char *);

typedef DECLARE_DRIZZLE_SessionVAR_TYPELIB(sessionvar_enum_t, unsigned long);
typedef DECLARE_DRIZZLE_SessionVAR_TYPELIB(sessionvar_set_t, uint64_t);

typedef DECLARE_DRIZZLE_SYSVAR_SIMPLE(sysvar_int_t, int);
typedef DECLARE_DRIZZLE_SYSVAR_SIMPLE(sysvar_long_t, long);
typedef DECLARE_DRIZZLE_SYSVAR_SIMPLE(sysvar_int64_t_t, int64_t);
typedef DECLARE_DRIZZLE_SYSVAR_SIMPLE(sysvar_uint_t, uint);
typedef DECLARE_DRIZZLE_SYSVAR_SIMPLE(sysvar_ulong_t, ulong);
typedef DECLARE_DRIZZLE_SYSVAR_SIMPLE(sysvar_uint64_t_t, uint64_t);

typedef DECLARE_DRIZZLE_SessionVAR_SIMPLE(sessionvar_int_t, int);
typedef DECLARE_DRIZZLE_SessionVAR_SIMPLE(sessionvar_long_t, long);
typedef DECLARE_DRIZZLE_SessionVAR_SIMPLE(sessionvar_int64_t_t, int64_t);
typedef DECLARE_DRIZZLE_SessionVAR_SIMPLE(sessionvar_uint_t, uint);
typedef DECLARE_DRIZZLE_SessionVAR_SIMPLE(sessionvar_ulong_t, ulong);
typedef DECLARE_DRIZZLE_SessionVAR_SIMPLE(sessionvar_uint64_t_t, uint64_t);

typedef bool *(*mysql_sys_var_ptr_p)(Session* a_session, int offset);


/****************************************************************************
  default variable data check and update functions
****************************************************************************/

static int check_func_bool(Session *, drizzle_sys_var *var,
                           void *save, drizzle_value *value)
{
  char buff[STRING_BUFFER_USUAL_SIZE];
  const char *strvalue= "NULL", *str;
  int result, length;
  int64_t tmp;

  if (value->value_type(value) == DRIZZLE_VALUE_TYPE_STRING)
  {
    length= sizeof(buff);
    if (!(str= value->val_str(value, buff, &length)) ||
        (result= find_type(&bool_typelib, str, length, 1)-1) < 0)
    {
      if (str)
        strvalue= str;
      goto err;
    }
  }
  else
  {
    if (value->val_int(value, &tmp) < 0)
      goto err;
    if (tmp > 1)
    {
      internal::llstr(tmp, buff);
      strvalue= buff;
      goto err;
    }
    result= (int) tmp;
  }
  *(int*)save= -result;
  return 0;
err:
  my_error(ER_WRONG_VALUE_FOR_VAR, MYF(0), var->name, strvalue);
  return 1;
}


static int check_func_int(Session *session, drizzle_sys_var *var,
                          void *save, drizzle_value *value)
{
  bool fixed;
  int64_t tmp;
  struct my_option options;
  value->val_int(value, &tmp);
  plugin_opt_set_limits(&options, var);

  if (var->flags & PLUGIN_VAR_UNSIGNED)
    *(uint32_t *)save= (uint32_t) getopt_ull_limit_value((uint64_t) tmp, &options,
                                                   &fixed);
  else
    *(int *)save= (int) getopt_ll_limit_value(tmp, &options, &fixed);

  return throw_bounds_warning(session, fixed, var->flags & PLUGIN_VAR_UNSIGNED,
                              var->name, (int64_t) tmp);
}


static int check_func_long(Session *session, drizzle_sys_var *var,
                          void *save, drizzle_value *value)
{
  bool fixed;
  int64_t tmp;
  struct my_option options;
  value->val_int(value, &tmp);
  plugin_opt_set_limits(&options, var);

  if (var->flags & PLUGIN_VAR_UNSIGNED)
    *(ulong *)save= (ulong) getopt_ull_limit_value((uint64_t) tmp, &options,
                                                   &fixed);
  else
    *(long *)save= (long) getopt_ll_limit_value(tmp, &options, &fixed);

  return throw_bounds_warning(session, fixed, var->flags & PLUGIN_VAR_UNSIGNED,
                              var->name, (int64_t) tmp);
}


static int check_func_int64_t(Session *session, drizzle_sys_var *var,
                               void *save, drizzle_value *value)
{
  bool fixed;
  int64_t tmp;
  struct my_option options;
  value->val_int(value, &tmp);
  plugin_opt_set_limits(&options, var);

  if (var->flags & PLUGIN_VAR_UNSIGNED)
    *(uint64_t *)save= getopt_ull_limit_value((uint64_t) tmp, &options,
                                               &fixed);
  else
    *(int64_t *)save= getopt_ll_limit_value(tmp, &options, &fixed);

  return throw_bounds_warning(session, fixed, var->flags & PLUGIN_VAR_UNSIGNED,
                              var->name, (int64_t) tmp);
}

static int check_func_str(Session *session, drizzle_sys_var *,
                          void *save, drizzle_value *value)
{
  char buff[STRING_BUFFER_USUAL_SIZE];
  const char *str;
  int length;

  length= sizeof(buff);
  if ((str= value->val_str(value, buff, &length)))
    str= session->strmake(str, length);
  *(const char**)save= str;
  return 0;
}


static void update_func_bool(Session *, drizzle_sys_var *,
                             void *tgt, const void *save)
{
  *(bool *) tgt= *(int *) save ? 1 : 0;
}


static void update_func_int(Session *, drizzle_sys_var *,
                             void *tgt, const void *save)
{
  *(int *)tgt= *(int *) save;
}


static void update_func_long(Session *, drizzle_sys_var *,
                             void *tgt, const void *save)
{
  *(long *)tgt= *(long *) save;
}


static void update_func_int64_t(Session *, drizzle_sys_var *,
                                 void *tgt, const void *save)
{
  *(int64_t *)tgt= *(uint64_t *) save;
}


static void update_func_str(Session *, drizzle_sys_var *var,
                             void *tgt, const void *save)
{
  char *old= *(char **) tgt;
  *(char **)tgt= *(char **) save;
  if (var->flags & PLUGIN_VAR_MEMALLOC)
  {
    *(char **)tgt= strdup(*(char **) save);
    free(old);
    /*
     * There isn't a _really_ good thing to do here until this whole set_var
     * mess gets redesigned
     */
    if (tgt == NULL)
      errmsg_printf(ERRMSG_LVL_ERROR, _("Out of memory."));

  }
}


/****************************************************************************
  System Variables support
****************************************************************************/


sys_var *find_sys_var(Session *, const char *str, uint32_t length)
{
  sys_var *var;
  sys_var_pluginvar *pi= NULL;
  plugin::Module *module;

  pthread_rwlock_rdlock(&LOCK_system_variables_hash);
  if ((var= intern_find_sys_var(str, length, false)) &&
      (pi= var->cast_pluginvar()))
  {
    pthread_rwlock_unlock(&LOCK_system_variables_hash);
    if (!(module= pi->plugin))
      var= NULL; /* failed to lock it, it must be uninstalling */
    else if (module->isInited == false)
    {
      var= NULL;
    }
  }
  else
    pthread_rwlock_unlock(&LOCK_system_variables_hash);

  /*
    If the variable exists but the plugin it is associated with is not ready
    then the intern_plugin_lock did not raise an error, so we do it here.
  */
  if (pi && !var)
    my_error(ER_UNKNOWN_SYSTEM_VARIABLE, MYF(0), (char*) str);
  return(var);
}

static const string make_bookmark_name(const string &plugin, const char *name, int flags)
{
  /* Embed the flags into the first char of the string */
  string varname(1, static_cast<char>(flags & PLUGIN_VAR_TYPEMASK));
  varname.append(plugin);
  varname.push_back('_');
  varname.append(name);

  for (string::iterator p= varname.begin() + 1; p != varname.end(); ++p)
  {
    if (*p == '-')
    {
      *p= '_';
    }
  }
  return varname;
}

/*
  called by register_var, construct_options and test_plugin_options.
  Returns the 'bookmark' for the named variable.
  LOCK_system_variables_hash should be at least read locked
*/
static st_bookmark *find_bookmark(const string &plugin, const char *name, int flags)
{
  st_bookmark *result= NULL;

  if (!(flags & PLUGIN_VAR_SessionLOCAL))
    return NULL;

  const string varname(make_bookmark_name(plugin, name, flags));


  result= (st_bookmark*) hash_search(&bookmark_hash,
                                     (const unsigned char*) varname.c_str(), varname.size() - 1);

  return result;
}


/*
  returns a bookmark for session-local variables, creating if neccessary.
  returns null for non session-local variables.
  Requires that a write lock is obtained on LOCK_system_variables_hash
*/
static st_bookmark *register_var(const string &plugin, const char *name,
                                 int flags)
{
  if (!(flags & PLUGIN_VAR_SessionLOCAL))
    return NULL;

  uint32_t size= 0, offset, new_size;
  st_bookmark *result;

  switch (flags & PLUGIN_VAR_TYPEMASK) {
  case PLUGIN_VAR_BOOL:
    size= ALIGN_SIZE(sizeof(bool));
    break;
  case PLUGIN_VAR_INT:
    size= ALIGN_SIZE(sizeof(int));
    break;
  case PLUGIN_VAR_LONG:
    size= ALIGN_SIZE(sizeof(long));
    break;
  case PLUGIN_VAR_LONGLONG:
    size= ALIGN_SIZE(sizeof(uint64_t));
    break;
  case PLUGIN_VAR_STR:
    size= ALIGN_SIZE(sizeof(char*));
    break;
  default:
    assert(0);
    return NULL;
  };


  if (!(result= find_bookmark(plugin, name, flags)))
  {
    const string varname(make_bookmark_name(plugin, name, flags));

    result= static_cast<st_bookmark*>(alloc_root(&plugin_mem_root,
                                      sizeof(struct st_bookmark) + varname.size() + 1));
    memset(result->key, 0, varname.size()+1);
    memcpy(result->key, varname.c_str(), varname.size());
    result->name_len= varname.size() - 2;
    result->offset= -1;

    assert(size && !(size & (size-1))); /* must be power of 2 */

    offset= global_system_variables.dynamic_variables_size;
    offset= (offset + size - 1) & ~(size - 1);
    result->offset= (int) offset;

    new_size= (offset + size + 63) & ~63;

    if (new_size > global_variables_dynamic_size)
    {
      char* tmpptr= NULL;
      if (!(tmpptr=
              (char *)realloc(global_system_variables.dynamic_variables_ptr,
                              new_size)))
        return NULL;
      global_system_variables.dynamic_variables_ptr= tmpptr;
      tmpptr= NULL;
      if (!(tmpptr=
              (char *)realloc(max_system_variables.dynamic_variables_ptr,
                              new_size)))
        return NULL;
      max_system_variables.dynamic_variables_ptr= tmpptr;
           
      /*
        Clear the new variable value space. This is required for string
        variables. If their value is non-NULL, it must point to a valid
        string.
      */
      memset(global_system_variables.dynamic_variables_ptr +
             global_variables_dynamic_size, 0,
             new_size - global_variables_dynamic_size);
      memset(max_system_variables.dynamic_variables_ptr +
             global_variables_dynamic_size, 0,
             new_size - global_variables_dynamic_size);
      global_variables_dynamic_size= new_size;
    }

    global_system_variables.dynamic_variables_head= offset;
    max_system_variables.dynamic_variables_head= offset;
    global_system_variables.dynamic_variables_size= offset + size;
    max_system_variables.dynamic_variables_size= offset + size;
    global_system_variables.dynamic_variables_version++;
    max_system_variables.dynamic_variables_version++;

    result->version= global_system_variables.dynamic_variables_version;

    /* this should succeed because we have already checked if a dup exists */
    if (my_hash_insert(&bookmark_hash, (unsigned char*) result))
    {
      fprintf(stderr, "failed to add placeholder to hash");
      assert(0);
    }
  }
  return result;
}


/*
  returns a pointer to the memory which holds the session-local variable or
  a pointer to the global variable if session==null.
  If required, will sync with global variables if the requested variable
  has not yet been allocated in the current thread.
*/
static unsigned char *intern_sys_var_ptr(Session* session, int offset, bool global_lock)
{
  assert(offset >= 0);
  assert((uint32_t)offset <= global_system_variables.dynamic_variables_head);

  if (!session)
    return (unsigned char*) global_system_variables.dynamic_variables_ptr + offset;

  /*
    dynamic_variables_head points to the largest valid offset
  */
  if (!session->variables.dynamic_variables_ptr ||
      (uint32_t)offset > session->variables.dynamic_variables_head)
  {
    uint32_t idx;

    pthread_rwlock_rdlock(&LOCK_system_variables_hash);

    char *tmpptr= NULL;
    if (!(tmpptr= (char *)realloc(session->variables.dynamic_variables_ptr,
                                  global_variables_dynamic_size)))
      return NULL;
    session->variables.dynamic_variables_ptr= tmpptr;

    if (global_lock)
      pthread_mutex_lock(&LOCK_global_system_variables);

    //safe_mutex_assert_owner(&LOCK_global_system_variables);

    memcpy(session->variables.dynamic_variables_ptr +
             session->variables.dynamic_variables_size,
           global_system_variables.dynamic_variables_ptr +
             session->variables.dynamic_variables_size,
           global_system_variables.dynamic_variables_size -
             session->variables.dynamic_variables_size);

    /*
      now we need to iterate through any newly copied 'defaults'
      and if it is a string type with MEMALLOC flag, we need to strdup
    */
    for (idx= 0; idx < bookmark_hash.records; idx++)
    {
      sys_var_pluginvar *pi;
      sys_var *var;
      st_bookmark *v= (st_bookmark*) hash_element(&bookmark_hash,idx);

      if (v->version <= session->variables.dynamic_variables_version ||
          !(var= intern_find_sys_var(v->key + 1, v->name_len, true)) ||
          !(pi= var->cast_pluginvar()) ||
          v->key[0] != (pi->plugin_var->flags & PLUGIN_VAR_TYPEMASK))
        continue;

      /* Here we do anything special that may be required of the data types */

      if ((pi->plugin_var->flags & PLUGIN_VAR_TYPEMASK) == PLUGIN_VAR_STR &&
          pi->plugin_var->flags & PLUGIN_VAR_MEMALLOC)
      {
         char **pp= (char**) (session->variables.dynamic_variables_ptr +
                             *(int*)(pi->plugin_var + 1));
         if ((*pp= *(char**) (global_system_variables.dynamic_variables_ptr +
                             *(int*)(pi->plugin_var + 1))))
           *pp= strdup(*pp);
         if (*pp == NULL)
           return NULL;
      }
    }

    if (global_lock)
      pthread_mutex_unlock(&LOCK_global_system_variables);

    session->variables.dynamic_variables_version=
           global_system_variables.dynamic_variables_version;
    session->variables.dynamic_variables_head=
           global_system_variables.dynamic_variables_head;
    session->variables.dynamic_variables_size=
           global_system_variables.dynamic_variables_size;

    pthread_rwlock_unlock(&LOCK_system_variables_hash);
  }
  return (unsigned char*)session->variables.dynamic_variables_ptr + offset;
}

static bool *mysql_sys_var_ptr_bool(Session* a_session, int offset)
{
  return (bool *)intern_sys_var_ptr(a_session, offset, true);
}

static int *mysql_sys_var_ptr_int(Session* a_session, int offset)
{
  return (int *)intern_sys_var_ptr(a_session, offset, true);
}

static long *mysql_sys_var_ptr_long(Session* a_session, int offset)
{
  return (long *)intern_sys_var_ptr(a_session, offset, true);
}

static int64_t *mysql_sys_var_ptr_int64_t(Session* a_session, int offset)
{
  return (int64_t *)intern_sys_var_ptr(a_session, offset, true);
}

static char **mysql_sys_var_ptr_str(Session* a_session, int offset)
{
  return (char **)intern_sys_var_ptr(a_session, offset, true);
}

void plugin_sessionvar_init(Session *session)
{
  session->variables.storage_engine= NULL;
  cleanup_variables(session, &session->variables);

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
static void unlock_variables(Session *, struct system_variables *vars)
{
  vars->storage_engine= NULL;
}


/*
  Frees memory used by system variables

  Unlike plugin_vars_free_values() it frees all variables of all plugins,
  it's used on shutdown.
*/
static void cleanup_variables(Session *session, struct system_variables *vars)
{
  st_bookmark *v;
  sys_var_pluginvar *pivar;
  sys_var *var;
  int flags;
  uint32_t idx;

  pthread_rwlock_rdlock(&LOCK_system_variables_hash);
  for (idx= 0; idx < bookmark_hash.records; idx++)
  {
    v= (st_bookmark*) hash_element(&bookmark_hash, idx);
    if (v->version > vars->dynamic_variables_version ||
        !(var= intern_find_sys_var(v->key + 1, v->name_len, true)) ||
        !(pivar= var->cast_pluginvar()) ||
        v->key[0] != (pivar->plugin_var->flags & PLUGIN_VAR_TYPEMASK))
      continue;

    flags= pivar->plugin_var->flags;

    if ((flags & PLUGIN_VAR_TYPEMASK) == PLUGIN_VAR_STR &&
        flags & PLUGIN_VAR_SessionLOCAL && flags & PLUGIN_VAR_MEMALLOC)
    {
      char **ptr= (char**) pivar->real_value_ptr(session, OPT_SESSION);
      free(*ptr);
      *ptr= NULL;
    }
  }
  pthread_rwlock_unlock(&LOCK_system_variables_hash);

  assert(vars->storage_engine == NULL);

  free(vars->dynamic_variables_ptr);
  vars->dynamic_variables_ptr= NULL;
  vars->dynamic_variables_size= 0;
  vars->dynamic_variables_version= 0;
}


void plugin_sessionvar_cleanup(Session *session)
{
  unlock_variables(session, &session->variables);
  cleanup_variables(session, &session->variables);
}


/**
  @brief Free values of thread variables of a plugin.

  This must be called before a plugin is deleted. Otherwise its
  variables are no longer accessible and the value space is lost. Note
  that only string values with PLUGIN_VAR_MEMALLOC are allocated and
  must be freed.

  @param[in]        vars        Chain of system variables of a plugin
*/

static void plugin_vars_free_values(sys_var *vars)
{

  for (sys_var *var= vars; var; var= var->getNext())
  {
    sys_var_pluginvar *piv= var->cast_pluginvar();
    if (piv &&
        ((piv->plugin_var->flags & PLUGIN_VAR_TYPEMASK) == PLUGIN_VAR_STR) &&
        (piv->plugin_var->flags & PLUGIN_VAR_MEMALLOC))
    {
      /* Free the string from global_system_variables. */
      char **valptr= (char**) piv->real_value_ptr(NULL, OPT_GLOBAL);
      free(*valptr);
      *valptr= NULL;
    }
  }
  return;
}


bool sys_var_pluginvar::check_update_type(Item_result type)
{
  if (is_readonly())
    return 1;
  switch (plugin_var->flags & PLUGIN_VAR_TYPEMASK) {
  case PLUGIN_VAR_INT:
  case PLUGIN_VAR_LONG:
  case PLUGIN_VAR_LONGLONG:
    return type != INT_RESULT;
  case PLUGIN_VAR_STR:
    return type != STRING_RESULT;
  default:
    return 0;
  }
}


SHOW_TYPE sys_var_pluginvar::show_type()
{
  switch (plugin_var->flags & PLUGIN_VAR_TYPEMASK) {
  case PLUGIN_VAR_BOOL:
    return SHOW_MY_BOOL;
  case PLUGIN_VAR_INT:
    return SHOW_INT;
  case PLUGIN_VAR_LONG:
    return SHOW_LONG;
  case PLUGIN_VAR_LONGLONG:
    return SHOW_LONGLONG;
  case PLUGIN_VAR_STR:
    return SHOW_CHAR_PTR;
  default:
    assert(0);
    return SHOW_UNDEF;
  }
}


unsigned char* sys_var_pluginvar::real_value_ptr(Session *session, sql_var_t type)
{
  assert(session || (type == OPT_GLOBAL));
  if (plugin_var->flags & PLUGIN_VAR_SessionLOCAL)
  {
    if (type == OPT_GLOBAL)
      session= NULL;

    return intern_sys_var_ptr(session, *(int*) (plugin_var+1), false);
  }
  return *(unsigned char**) (plugin_var+1);
}


TYPELIB* sys_var_pluginvar::plugin_var_typelib(void)
{
  switch (plugin_var->flags & (PLUGIN_VAR_TYPEMASK | PLUGIN_VAR_SessionLOCAL)) {
  case PLUGIN_VAR_SessionLOCAL:
    return ((sessionvar_enum_t *)plugin_var)->typelib;
  default:
    return NULL;
  }
  return NULL;
}


unsigned char* sys_var_pluginvar::value_ptr(Session *session, sql_var_t type, const LEX_STRING *)
{
  unsigned char* result;

  result= real_value_ptr(session, type);

  return result;
}


bool sys_var_pluginvar::check(Session *session, set_var *var)
{
  st_item_value_holder value;
  assert(is_readonly() || plugin_var->check);

  value.value_type= item_value_type;
  value.val_str= item_val_str;
  value.val_int= item_val_int;
  value.val_real= item_val_real;
  value.item= var->value;

  return is_readonly() ||
         plugin_var->check(session, plugin_var, &var->save_result, &value);
}


void sys_var_pluginvar::set_default(Session *session, sql_var_t type)
{
  const void *src;
  void *tgt;

  assert(is_readonly() || plugin_var->update);

  if (is_readonly())
    return;

  pthread_mutex_lock(&LOCK_global_system_variables);
  tgt= real_value_ptr(session, type);
  src= ((void **) (plugin_var + 1) + 1);

  if (plugin_var->flags & PLUGIN_VAR_SessionLOCAL)
  {
    if (type != OPT_GLOBAL)
      src= real_value_ptr(session, OPT_GLOBAL);
    else
    switch (plugin_var->flags & PLUGIN_VAR_TYPEMASK) {
	case PLUGIN_VAR_INT:
	  src= &((sessionvar_uint_t*) plugin_var)->def_val;
	  break;
	case PLUGIN_VAR_LONG:
	  src= &((sessionvar_ulong_t*) plugin_var)->def_val;
	  break;
	case PLUGIN_VAR_LONGLONG:
	  src= &((sessionvar_uint64_t_t*) plugin_var)->def_val;
	  break;
	case PLUGIN_VAR_BOOL:
	  src= &((sessionvar_bool_t*) plugin_var)->def_val;
	  break;
	case PLUGIN_VAR_STR:
	  src= &((sessionvar_str_t*) plugin_var)->def_val;
	  break;
	default:
	  assert(0);
	}
  }

  /* session must equal current_session if PLUGIN_VAR_SessionLOCAL flag is set */
  assert(!(plugin_var->flags & PLUGIN_VAR_SessionLOCAL) ||
              session == current_session);

  if (!(plugin_var->flags & PLUGIN_VAR_SessionLOCAL) || type == OPT_GLOBAL)
  {
    plugin_var->update(session, plugin_var, tgt, src);
    pthread_mutex_unlock(&LOCK_global_system_variables);
  }
  else
  {
    pthread_mutex_unlock(&LOCK_global_system_variables);
    plugin_var->update(session, plugin_var, tgt, src);
  }
}


bool sys_var_pluginvar::update(Session *session, set_var *var)
{
  void *tgt;

  assert(is_readonly() || plugin_var->update);

  /* session must equal current_session if PLUGIN_VAR_SessionLOCAL flag is set */
  assert(!(plugin_var->flags & PLUGIN_VAR_SessionLOCAL) ||
              session == current_session);

  if (is_readonly())
    return 1;

  pthread_mutex_lock(&LOCK_global_system_variables);
  tgt= real_value_ptr(session, var->type);

  if (!(plugin_var->flags & PLUGIN_VAR_SessionLOCAL) || var->type == OPT_GLOBAL)
  {
    /* variable we are updating has global scope, so we unlock after updating */
    plugin_var->update(session, plugin_var, tgt, &var->save_result);
    pthread_mutex_unlock(&LOCK_global_system_variables);
  }
  else
  {
    pthread_mutex_unlock(&LOCK_global_system_variables);
    plugin_var->update(session, plugin_var, tgt, &var->save_result);
  }
 return 0;
}


#define OPTION_SET_LIMITS(type, options, opt) \
  options->var_type= type;                    \
  options->def_value= (opt)->def_val;         \
  options->min_value= (opt)->min_val;         \
  options->max_value= (opt)->max_val;         \
  options->block_size= (long) (opt)->blk_sz


void plugin_opt_set_limits(struct my_option *options,
													 const drizzle_sys_var *opt)
{
  options->sub_size= 0;

  switch (opt->flags & (PLUGIN_VAR_TYPEMASK |
                        PLUGIN_VAR_UNSIGNED | PLUGIN_VAR_SessionLOCAL)) {
  /* global system variables */
  case PLUGIN_VAR_INT:
    OPTION_SET_LIMITS(GET_INT, options, (sysvar_int_t*) opt);
    break;
  case PLUGIN_VAR_INT | PLUGIN_VAR_UNSIGNED:
    OPTION_SET_LIMITS(GET_UINT, options, (sysvar_uint_t*) opt);
    break;
  case PLUGIN_VAR_LONG:
    OPTION_SET_LIMITS(GET_LONG, options, (sysvar_long_t*) opt);
    break;
  case PLUGIN_VAR_LONG | PLUGIN_VAR_UNSIGNED:
    OPTION_SET_LIMITS(GET_ULONG_IS_FAIL, options, (sysvar_ulong_t*) opt);
    break;
  case PLUGIN_VAR_LONGLONG:
    OPTION_SET_LIMITS(GET_LL, options, (sysvar_int64_t_t*) opt);
    break;
  case PLUGIN_VAR_LONGLONG | PLUGIN_VAR_UNSIGNED:
    OPTION_SET_LIMITS(GET_ULL, options, (sysvar_uint64_t_t*) opt);
    break;
  case PLUGIN_VAR_BOOL:
    options->var_type= GET_BOOL;
    options->def_value= ((sysvar_bool_t*) opt)->def_val;
    break;
  case PLUGIN_VAR_STR:
    options->var_type= ((opt->flags & PLUGIN_VAR_MEMALLOC) ?
                        GET_STR_ALLOC : GET_STR);
    options->def_value= (intptr_t) ((sysvar_str_t*) opt)->def_val;
    break;
  /* threadlocal variables */
  case PLUGIN_VAR_INT | PLUGIN_VAR_SessionLOCAL:
    OPTION_SET_LIMITS(GET_INT, options, (sessionvar_int_t*) opt);
    break;
  case PLUGIN_VAR_INT | PLUGIN_VAR_UNSIGNED | PLUGIN_VAR_SessionLOCAL:
    OPTION_SET_LIMITS(GET_UINT, options, (sessionvar_uint_t*) opt);
    break;
  case PLUGIN_VAR_LONG | PLUGIN_VAR_SessionLOCAL:
    OPTION_SET_LIMITS(GET_LONG, options, (sessionvar_long_t*) opt);
    break;
  case PLUGIN_VAR_LONG | PLUGIN_VAR_UNSIGNED | PLUGIN_VAR_SessionLOCAL:
    OPTION_SET_LIMITS(GET_ULONG_IS_FAIL, options, (sessionvar_ulong_t*) opt);
    break;
  case PLUGIN_VAR_LONGLONG | PLUGIN_VAR_SessionLOCAL:
    OPTION_SET_LIMITS(GET_LL, options, (sessionvar_int64_t_t*) opt);
    break;
  case PLUGIN_VAR_LONGLONG | PLUGIN_VAR_UNSIGNED | PLUGIN_VAR_SessionLOCAL:
    OPTION_SET_LIMITS(GET_ULL, options, (sessionvar_uint64_t_t*) opt);
    break;
  case PLUGIN_VAR_BOOL | PLUGIN_VAR_SessionLOCAL:
    options->var_type= GET_BOOL;
    options->def_value= ((sessionvar_bool_t*) opt)->def_val;
    break;
  case PLUGIN_VAR_STR | PLUGIN_VAR_SessionLOCAL:
    options->var_type= ((opt->flags & PLUGIN_VAR_MEMALLOC) ?
                        GET_STR_ALLOC : GET_STR);
    options->def_value= (intptr_t) ((sessionvar_str_t*) opt)->def_val;
    break;
  default:
    assert(0);
  }
  options->arg_type= REQUIRED_ARG;
  if (opt->flags & PLUGIN_VAR_NOCMDARG)
    options->arg_type= NO_ARG;
  if (opt->flags & PLUGIN_VAR_OPCMDARG)
    options->arg_type= OPT_ARG;
}

static bool get_one_plugin_option(int, const struct my_option *, char *)
{
  return 0;
}


static int construct_options(memory::Root *mem_root, plugin::Module *tmp,
                             my_option *options)
{
  
  int localoptionid= 256;
  const string plugin_name(tmp->getManifest().name);

  size_t namelen= plugin_name.size(), optnamelen;

  char *optname, *p;
  int index= 0, offset= 0;
  drizzle_sys_var *opt, **plugin_option;
  st_bookmark *v;

  string name(plugin_name);
  transform(name.begin(), name.end(), name.begin(), ::tolower);

  for (string::iterator iter= name.begin(); iter != name.end(); ++iter)
  {
    if (*iter == '_')
      *iter= '-';
  }

  /*
    Two passes as the 2nd pass will take pointer addresses for use
    by my_getopt and register_var() in the first pass uses realloc
  */

  for (plugin_option= tmp->getManifest().system_vars;
       plugin_option && *plugin_option; plugin_option++, index++)
  {
    opt= *plugin_option;
    if (!(opt->flags & PLUGIN_VAR_SessionLOCAL))
      continue;
    if (!(register_var(name, opt->name, opt->flags)))
      continue;
    switch (opt->flags & PLUGIN_VAR_TYPEMASK) {
    case PLUGIN_VAR_BOOL:
      (((sessionvar_bool_t *)opt)->resolve)= mysql_sys_var_ptr_bool;
      break;
    case PLUGIN_VAR_INT:
      (((sessionvar_int_t *)opt)->resolve)= mysql_sys_var_ptr_int;
      break;
    case PLUGIN_VAR_LONG:
      (((sessionvar_long_t *)opt)->resolve)= mysql_sys_var_ptr_long;
      break;
    case PLUGIN_VAR_LONGLONG:
      (((sessionvar_int64_t_t *)opt)->resolve)= mysql_sys_var_ptr_int64_t;
      break;
    case PLUGIN_VAR_STR:
      (((sessionvar_str_t *)opt)->resolve)= mysql_sys_var_ptr_str;
      break;
    default:
      errmsg_printf(ERRMSG_LVL_ERROR, _("Unknown variable type code 0x%x in plugin '%s'."),
                      opt->flags, plugin_name.c_str());
      return(-1);
    };
  }

  for (plugin_option= tmp->getManifest().system_vars;
       plugin_option && *plugin_option; plugin_option++, index++)
  {
    switch ((opt= *plugin_option)->flags & PLUGIN_VAR_TYPEMASK) {
    case PLUGIN_VAR_BOOL:
      if (!opt->check)
        opt->check= check_func_bool;
      if (!opt->update)
        opt->update= update_func_bool;
      break;
    case PLUGIN_VAR_INT:
      if (!opt->check)
        opt->check= check_func_int;
      if (!opt->update)
        opt->update= update_func_int;
      break;
    case PLUGIN_VAR_LONG:
      if (!opt->check)
        opt->check= check_func_long;
      if (!opt->update)
        opt->update= update_func_long;
      break;
    case PLUGIN_VAR_LONGLONG:
      if (!opt->check)
        opt->check= check_func_int64_t;
      if (!opt->update)
        opt->update= update_func_int64_t;
      break;
    case PLUGIN_VAR_STR:
      if (!opt->check)
        opt->check= check_func_str;
      if (!opt->update)
      {
        opt->update= update_func_str;
        if ((opt->flags & (PLUGIN_VAR_MEMALLOC | PLUGIN_VAR_READONLY)) == false)
        {
          opt->flags|= PLUGIN_VAR_READONLY;
          errmsg_printf(ERRMSG_LVL_WARN, _("Server variable %s of plugin %s was forced "
                            "to be read-only: string variable without "
                            "update_func and PLUGIN_VAR_MEMALLOC flag"),
                            opt->name, plugin_name.c_str());
        }
      }
      break;
    default:
      errmsg_printf(ERRMSG_LVL_ERROR, _("Unknown variable type code 0x%x in plugin '%s'."),
                      opt->flags, plugin_name.c_str());
      return(-1);
    }

    if ((opt->flags & (PLUGIN_VAR_NOCMDOPT | PLUGIN_VAR_SessionLOCAL))
                    == PLUGIN_VAR_NOCMDOPT)
      continue;

    if (!opt->name)
    {
      errmsg_printf(ERRMSG_LVL_ERROR, _("Missing variable name in plugin '%s'."),
                    plugin_name.c_str());
      return(-1);
    }

    if (!(opt->flags & PLUGIN_VAR_SessionLOCAL))
    {
      optnamelen= strlen(opt->name);
      optname= (char*) alloc_root(mem_root, namelen + optnamelen + 2);
      sprintf(optname, "%s-%s", name.c_str(), opt->name);
      optnamelen= namelen + optnamelen + 1;
    }
    else
    {
      /* this should not fail because register_var should create entry */
      if (!(v= find_bookmark(name, opt->name, opt->flags)))
      {
        errmsg_printf(ERRMSG_LVL_ERROR, _("Thread local variable '%s' not allocated "
                      "in plugin '%s'."), opt->name, plugin_name.c_str());
        return(-1);
      }

      *(int*)(opt + 1)= offset= v->offset;

      if (opt->flags & PLUGIN_VAR_NOCMDOPT)
        continue;

      optname= (char*) memdup_root(mem_root, v->key + 1,
                                   (optnamelen= v->name_len) + 1);
    }

    /* convert '_' to '-' */
    for (p= optname; *p; p++)
      if (*p == '_')
        *p= '-';

    options->name= optname;
    options->comment= opt->comment;
    options->app_type= opt;
    options->id= localoptionid++;

    plugin_opt_set_limits(options, opt);

    if (opt->flags & PLUGIN_VAR_SessionLOCAL)
      options->value= options->u_max_value= (char**)
        (global_system_variables.dynamic_variables_ptr + offset);
    else
      options->value= options->u_max_value= *(char***) (opt + 1);

    options++;
  }

  return(0);
}


static my_option *construct_help_options(memory::Root *mem_root, plugin::Module *p)
{
  drizzle_sys_var **opt;
  my_option *opts;
  uint32_t count= EXTRA_OPTIONS;

  for (opt= p->getManifest().system_vars; opt && *opt; opt++, count++) {};

  opts= (my_option*)alloc_root(mem_root, (sizeof(my_option) * count));
  if (opts == NULL)
    return NULL;

  memset(opts, 0, sizeof(my_option) * count);

  if (construct_options(mem_root, p, opts))
    return NULL;

  return(opts);
}

void drizzle_add_plugin_sysvar(sys_var_pluginvar *var)
{
  plugin_sysvar_vec.push_back(var);
}

void drizzle_del_plugin_sysvar()
{
  vector<sys_var_pluginvar *>::iterator iter= plugin_sysvar_vec.begin();
  while(iter != plugin_sysvar_vec.end())
  {
    delete *iter;
    ++iter;
  }
  plugin_sysvar_vec.clear();
}

/*
  SYNOPSIS
    test_plugin_options()
    tmp_root                    temporary scratch space
    plugin                      internal plugin structure
    argc                        user supplied arguments
    argv                        user supplied arguments
    default_enabled             default plugin enable status
  RETURNS:
    0 SUCCESS - plugin should be enabled/loaded
  NOTE:
    Requires that a write-lock is held on LOCK_system_variables_hash
*/
static int test_plugin_options(memory::Root *tmp_root, plugin::Module *tmp,
                               int *argc, char **argv)
{
  struct sys_var_chain chain= { NULL, NULL };
  drizzle_sys_var **opt;
  my_option *opts= NULL;
  int error;
  drizzle_sys_var *o;
  struct st_bookmark *var;
  uint32_t len, count= EXTRA_OPTIONS;

  for (opt= tmp->getManifest().system_vars; opt && *opt; opt++)
    count++;


  if (count > EXTRA_OPTIONS || (*argc > 1))
  {
    if (!(opts= (my_option*) alloc_root(tmp_root, sizeof(my_option) * count)))
    {
      errmsg_printf(ERRMSG_LVL_ERROR, _("Out of memory for plugin '%s'."), tmp->getName().c_str());
      return(-1);
    }
    memset(opts, 0, sizeof(my_option) * count);

    if (construct_options(tmp_root, tmp, opts))
    {
      errmsg_printf(ERRMSG_LVL_ERROR, _("Bad options for plugin '%s'."), tmp->getName().c_str());
      return(-1);
    }

    error= handle_options(argc, &argv, opts, get_one_plugin_option);
    (*argc)++; /* add back one for the program name */

    if (error)
    {
       errmsg_printf(ERRMSG_LVL_ERROR, _("Parsing options for plugin '%s' failed."),
                       tmp->getName().c_str());
       goto err;
    }
  }

  error= 1;

  {
    for (opt= tmp->getManifest().system_vars; opt && *opt; opt++)
    {
      sys_var *v;
      if (((o= *opt)->flags & PLUGIN_VAR_NOSYSVAR))
        continue;

      if ((var= find_bookmark(tmp->getName(), o->name, o->flags)))
        v= new sys_var_pluginvar(var->key + 1, o);
      else
      {
        len= tmp->getName().length() + strlen(o->name) + 2;
        string vname(tmp->getName());
        vname.push_back('-');
        vname.append(o->name);
        transform(vname.begin(), vname.end(), vname.begin(), ::tolower);
        string::iterator p= vname.begin();      
        while  (p != vname.end())
        {
          if (*p == '-')
            *p= '_';
          ++p;
        }

        v= new sys_var_pluginvar(vname, o);
      }
      assert(v); /* check that an object was actually constructed */

      drizzle_add_plugin_sysvar(static_cast<sys_var_pluginvar *>(v));
      /*
        Add to the chain of variables.
        Done like this for easier debugging so that the
        pointer to v is not lost on optimized builds.
      */
      v->chain_sys_var(&chain);
    }
    if (chain.first)
    {
      chain.last->setNext(NULL);
      if (mysql_add_sys_var_chain(chain.first, NULL))
      {
        errmsg_printf(ERRMSG_LVL_ERROR, _("Plugin '%s' has conflicting system variables"),
                        tmp->getName().c_str());
        goto err;
      }
      tmp->system_vars= chain.first;
    }
    return(0);
  }

err:
  if (opts)
    my_cleanup_options(opts);
  return(error);
}


/****************************************************************************
  Help Verbose text with Plugin System Variables
****************************************************************************/

class OptionCmp
{
public:
  bool operator() (const my_option &a, const my_option &b)
  {
    return my_strcasecmp(&my_charset_utf8_general_ci, a.name, b.name);
  }
};


void my_print_help_inc_plugins(my_option *main_options)
{
  plugin::Registry &registry= plugin::Registry::singleton();
  vector<my_option> all_options;
  plugin::Module *p;
  memory::Root mem_root;
  my_option *opt= NULL;

  init_alloc_root(&mem_root, 4096);

  if (initialized)
  {
    std::map<std::string, plugin::Module *>::const_iterator modules=
      registry.getModulesMap().begin();
    
    while (modules != registry.getModulesMap().end())
    {
      p= (*modules).second;
      ++modules;

      if (p->getManifest().system_vars == NULL)
        continue;

      opt= construct_help_options(&mem_root, p);
      if (opt == NULL)
        continue;

      /* Only options with a non-NULL comment are displayed in help text */
      for (;opt->id; opt++)
      {
        if (opt->comment)
        {
          all_options.push_back(*opt);
          
        }
      }
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
   * @TODO: Fix the my_option building so that it doens't break sort
   *
   * sort(all_options.begin(), all_options.end(), OptionCmp());
   */

  /* main_options now points to the empty option terminator */
  all_options.push_back(*main_options);

  my_print_help(&*(all_options.begin()));
  my_print_variables(&*(all_options.begin()));

  free_root(&mem_root, MYF(0));

}

} /* namespace drizzled */
