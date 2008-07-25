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

#include "mysql_priv.h"
#include <my_pthread.h>
#include <my_getopt.h>
#define REPORT_TO_LOG  1
#define REPORT_TO_USER 2

#define plugin_ref_to_int(A) (A ? A[0] : NULL)
#define plugin_int_to_ref(A) &(A)

extern struct st_mysql_plugin *mysqld_builtins[];

char *opt_plugin_load= NULL;
char *opt_plugin_dir_ptr;
char opt_plugin_dir[FN_REFLEN];
/*
  When you ad a new plugin type, add both a string and make sure that the
  init and deinit array are correctly updated.
*/
const LEX_STRING plugin_type_names[MYSQL_MAX_PLUGIN_TYPE_NUM]=
{
  { C_STRING_WITH_LEN("DAEMON") },
  { C_STRING_WITH_LEN("STORAGE ENGINE") },
  { C_STRING_WITH_LEN("INFORMATION SCHEMA") },
  { C_STRING_WITH_LEN("UDF") },
  { C_STRING_WITH_LEN("UDA") },
  { C_STRING_WITH_LEN("AUDIT") },
  { C_STRING_WITH_LEN("LOGGER") },
  { C_STRING_WITH_LEN("AUTH") }
};

extern int initialize_schema_table(st_plugin_int *plugin);
extern int finalize_schema_table(st_plugin_int *plugin);

extern int initialize_udf(st_plugin_int *plugin);
extern int finalize_udf(st_plugin_int *plugin);

/*
  The number of elements in both plugin_type_initialize and
  plugin_type_deinitialize should equal to the number of plugins
  defined.
*/
plugin_type_init plugin_type_initialize[MYSQL_MAX_PLUGIN_TYPE_NUM]=
{
  0,  /* Daemon */
  ha_initialize_handlerton,  /* Storage Engine */
  initialize_schema_table,  /* Information Schema */
  initialize_udf,  /* UDF */
  0,  /* UDA */
  0,  /* Audit */
  0,  /* Logger */
  0  /* Auth */
};

plugin_type_init plugin_type_deinitialize[MYSQL_MAX_PLUGIN_TYPE_NUM]=
{
  0,  /* Daemon */
  ha_finalize_handlerton,  /* Storage Engine */
  finalize_schema_table,  /* Information Schema */
  finalize_udf,  /* UDF */
  0,  /* UDA */
  0,  /* Audit */
  0,  /* Logger */
  0  /* Auth */
};

static const char *plugin_declarations_sym= "_mysql_plugin_declarations_";

/* Note that 'int version' must be the first field of every plugin
   sub-structure (plugin->info).
*/

static bool initialized= 0;

static DYNAMIC_ARRAY plugin_dl_array;
static DYNAMIC_ARRAY plugin_array;
static HASH plugin_hash[MYSQL_MAX_PLUGIN_TYPE_NUM];
static bool reap_needed= false;
static int plugin_array_version=0;

/*
  write-lock on LOCK_system_variables_hash is required before modifying
  the following variables/structures
*/
static MEM_ROOT plugin_mem_root;
static uint global_variables_dynamic_size= 0;
static HASH bookmark_hash;


/*
  hidden part of opaque value passed to variable check functions.
  Used to provide a object-like structure to non C++ consumers.
*/
struct st_item_value_holder : public st_mysql_value
{
  Item *item;
};


/*
  stored in bookmark_hash, this structure is never removed from the
  hash and is used to mark a single offset for a thd local variable
  even if plugins have been uninstalled and reinstalled, repeatedly.
  This structure is allocated from plugin_mem_root.

  The key format is as follows:
    1 byte         - variable type code
    name_len bytes - variable name
    '\0'           - end of key
*/
struct st_bookmark
{
  uint name_len;
  int offset;
  uint version;
  char key[1];
};


/*
  skeleton of a plugin variable - portion of structure common to all.
*/
struct st_mysql_sys_var
{
  MYSQL_PLUGIN_VAR_HEADER;
};


/*
  sys_var class for access to all plugin variables visible to the user
*/
class sys_var_pluginvar: public sys_var
{
public:
  struct st_plugin_int *plugin;
  struct st_mysql_sys_var *plugin_var;

  static void *operator new(size_t size, MEM_ROOT *mem_root)
  { return (void*) alloc_root(mem_root, (uint) size); }
  static void operator delete(void *ptr_arg __attribute__((__unused__)),
                              size_t size __attribute__((__unused__)))
  { TRASH(ptr_arg, size); }

  sys_var_pluginvar(const char *name_arg,
                    struct st_mysql_sys_var *plugin_var_arg)
    :sys_var(name_arg), plugin_var(plugin_var_arg) {}
  sys_var_pluginvar *cast_pluginvar() { return this; }
  bool is_readonly() const { return plugin_var->flags & PLUGIN_VAR_READONLY; }
  bool check_type(enum_var_type type)
  { return !(plugin_var->flags & PLUGIN_VAR_THDLOCAL) && type != OPT_GLOBAL; }
  bool check_update_type(Item_result type);
  SHOW_TYPE show_type();
  uchar* real_value_ptr(THD *thd, enum_var_type type);
  TYPELIB* plugin_var_typelib(void);
  uchar* value_ptr(THD *thd, enum_var_type type, LEX_STRING *base);
  bool check(THD *thd, set_var *var);
  bool check_default(enum_var_type type __attribute__((__unused__)))
    { return is_readonly(); }
  void set_default(THD *thd,
                   enum_var_type type __attribute__((__unused__)));
  bool update(THD *thd, set_var *var);
};


/* prototypes */
static bool plugin_load_list(MEM_ROOT *tmp_root, int *argc, char **argv,
                             const char *list);
static int test_plugin_options(MEM_ROOT *, struct st_plugin_int *,
                               int *, char **);
static bool register_builtin(struct st_mysql_plugin *, struct st_plugin_int *,
                             struct st_plugin_int **);
static void unlock_variables(THD *thd, struct system_variables *vars);
static void cleanup_variables(THD *thd, struct system_variables *vars);
static void plugin_vars_free_values(sys_var *vars);
static void plugin_opt_set_limits(struct my_option *options,
                                  const struct st_mysql_sys_var *opt);
#define my_intern_plugin_lock(A,B) intern_plugin_lock(A,B CALLER_INFO)
#define my_intern_plugin_lock_ci(A,B) intern_plugin_lock(A,B ORIG_CALLER_INFO)
static plugin_ref intern_plugin_lock(LEX *lex, plugin_ref plugin
                                     CALLER_INFO_PROTO);
static void intern_plugin_unlock(LEX *lex, plugin_ref plugin);
static void reap_plugins(void);


/* declared in set_var.cc */
extern sys_var *intern_find_sys_var(const char *str, uint length, bool no_error);
extern bool throw_bounds_warning(THD *thd, bool fixed, bool unsignd,
                                 const char *name, int64_t val);

/****************************************************************************
  Value type thunks, allows the C world to play in the C++ world
****************************************************************************/

static int item_value_type(struct st_mysql_value *value)
{
  switch (((st_item_value_holder*)value)->item->result_type()) {
  case INT_RESULT:
    return MYSQL_VALUE_TYPE_INT;
  case REAL_RESULT:
    return MYSQL_VALUE_TYPE_REAL;
  default:
    return MYSQL_VALUE_TYPE_STRING;
  }
}

static const char *item_val_str(struct st_mysql_value *value,
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
  return current_thd->strmake(res->c_ptr_quick(), res->length());
}


static int item_val_int(struct st_mysql_value *value, int64_t *buf)
{
  Item *item= ((st_item_value_holder*)value)->item;
  *buf= item->val_int();
  if (item->is_null())
    return 1;
  return 0;
}


static int item_val_real(struct st_mysql_value *value, double *buf)
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

static struct st_plugin_dl *plugin_dl_find(const LEX_STRING *dl)
{
  uint i;
  struct st_plugin_dl *tmp;
  for (i= 0; i < plugin_dl_array.elements; i++)
  {
    tmp= *dynamic_element(&plugin_dl_array, i, struct st_plugin_dl **);
    if (tmp->ref_count &&
        ! my_strnncoll(files_charset_info,
                       (const uchar *)dl->str, dl->length,
                       (const uchar *)tmp->dl.str, tmp->dl.length))
      return(tmp);
  }
  return(0);
}

static st_plugin_dl *plugin_dl_insert_or_reuse(struct st_plugin_dl *plugin_dl)
{
  uint i;
  struct st_plugin_dl *tmp;
  for (i= 0; i < plugin_dl_array.elements; i++)
  {
    tmp= *dynamic_element(&plugin_dl_array, i, struct st_plugin_dl **);
    if (! tmp->ref_count)
    {
      memcpy(tmp, plugin_dl, sizeof(struct st_plugin_dl));
      return(tmp);
    }
  }
  if (insert_dynamic(&plugin_dl_array, (uchar*)&plugin_dl))
    return(0);
  tmp= *dynamic_element(&plugin_dl_array, plugin_dl_array.elements - 1,
                        struct st_plugin_dl **)=
      (struct st_plugin_dl *) memdup_root(&plugin_mem_root, (uchar*)plugin_dl,
                                           sizeof(struct st_plugin_dl));
  return(tmp);
}

static inline void free_plugin_mem(struct st_plugin_dl *p)
{
  if (p->handle)
    dlclose(p->handle);
  my_free(p->dl.str, MYF(MY_ALLOW_ZERO_PTR));
}


static st_plugin_dl *plugin_dl_add(const LEX_STRING *dl, int report)
{
  char dlpath[FN_REFLEN];
  uint plugin_dir_len, dummy_errors, dlpathlen;
  struct st_plugin_dl *tmp, plugin_dl;
  void *sym;
  plugin_dir_len= strlen(opt_plugin_dir);
  /*
    Ensure that the dll doesn't have a path.
    This is done to ensure that only approved libraries from the
    plugin directory are used (to make this even remotely secure).
  */
  if (my_strchr(files_charset_info, dl->str, dl->str + dl->length, FN_LIBCHAR) ||
      check_string_char_length((LEX_STRING *) dl, "", NAME_CHAR_LEN,
                               system_charset_info, 1) ||
      plugin_dir_len + dl->length + 1 >= FN_REFLEN)
  {
    if (report & REPORT_TO_USER)
      my_error(ER_UDF_NO_PATHS, MYF(0));
    if (report & REPORT_TO_LOG)
      sql_print_error(ER(ER_UDF_NO_PATHS));
    return(0);
  }
  /* If this dll is already loaded just increase ref_count. */
  if ((tmp= plugin_dl_find(dl)))
  {
    tmp->ref_count++;
    return(tmp);
  }
  bzero(&plugin_dl, sizeof(plugin_dl));
  /* Compile dll path */
  dlpathlen=
    strxnmov(dlpath, sizeof(dlpath) - 1, opt_plugin_dir, "/", dl->str, NullS) -
    dlpath;
  plugin_dl.ref_count= 1;
  /* Open new dll handle */
  if (!(plugin_dl.handle= dlopen(dlpath, RTLD_NOW)))
  {
    const char *errmsg=dlerror();
    if (!strncmp(dlpath, errmsg, dlpathlen))
    { // if errmsg starts from dlpath, trim this prefix.
      errmsg+=dlpathlen;
      if (*errmsg == ':') errmsg++;
      if (*errmsg == ' ') errmsg++;
    }
    if (report & REPORT_TO_USER)
      my_error(ER_CANT_OPEN_LIBRARY, MYF(0), dlpath, errno, errmsg);
    if (report & REPORT_TO_LOG)
      sql_print_error(ER(ER_CANT_OPEN_LIBRARY), dlpath, errno, errmsg);
    return(0);
  }

  /* Find plugin declarations */
  if (!(sym= dlsym(plugin_dl.handle, plugin_declarations_sym)))
  {
    free_plugin_mem(&plugin_dl);
    if (report & REPORT_TO_USER)
      my_error(ER_CANT_FIND_DL_ENTRY, MYF(0), plugin_declarations_sym);
    if (report & REPORT_TO_LOG)
      sql_print_error(ER(ER_CANT_FIND_DL_ENTRY), plugin_declarations_sym);
    return(0);
  }

  plugin_dl.plugins= (struct st_mysql_plugin *)sym;

  /* Duplicate and convert dll name */
  plugin_dl.dl.length= dl->length * files_charset_info->mbmaxlen + 1;
  if (! (plugin_dl.dl.str= (char*) my_malloc(plugin_dl.dl.length, MYF(0))))
  {
    free_plugin_mem(&plugin_dl);
    if (report & REPORT_TO_USER)
      my_error(ER_OUTOFMEMORY, MYF(0), plugin_dl.dl.length);
    if (report & REPORT_TO_LOG)
      sql_print_error(ER(ER_OUTOFMEMORY), plugin_dl.dl.length);
    return(0);
  }
  plugin_dl.dl.length= copy_and_convert(plugin_dl.dl.str, plugin_dl.dl.length,
    files_charset_info, dl->str, dl->length, system_charset_info,
    &dummy_errors);
  plugin_dl.dl.str[plugin_dl.dl.length]= 0;
  /* Add this dll to array */
  if (! (tmp= plugin_dl_insert_or_reuse(&plugin_dl)))
  {
    free_plugin_mem(&plugin_dl);
    if (report & REPORT_TO_USER)
      my_error(ER_OUTOFMEMORY, MYF(0), sizeof(struct st_plugin_dl));
    if (report & REPORT_TO_LOG)
      sql_print_error(ER(ER_OUTOFMEMORY), sizeof(struct st_plugin_dl));
    return(0);
  }
  return(tmp);
}


static void plugin_dl_del(const LEX_STRING *dl)
{
  uint i;

  for (i= 0; i < plugin_dl_array.elements; i++)
  {
    struct st_plugin_dl *tmp= *dynamic_element(&plugin_dl_array, i,
                                               struct st_plugin_dl **);
    if (tmp->ref_count &&
        ! my_strnncoll(files_charset_info,
                       (const uchar *)dl->str, dl->length,
                       (const uchar *)tmp->dl.str, tmp->dl.length))
    {
      /* Do not remove this element, unless no other plugin uses this dll. */
      if (! --tmp->ref_count)
      {
        free_plugin_mem(tmp);
        bzero(tmp, sizeof(struct st_plugin_dl));
      }
      break;
    }
  }
  return;
}


static struct st_plugin_int *plugin_find_internal(const LEX_STRING *name, int type)
{
  uint i;
  if (! initialized)
    return(0);

  if (type == MYSQL_ANY_PLUGIN)
  {
    for (i= 0; i < MYSQL_MAX_PLUGIN_TYPE_NUM; i++)
    {
      struct st_plugin_int *plugin= (st_plugin_int *)
        hash_search(&plugin_hash[i], (const uchar *)name->str, name->length);
      if (plugin)
        return(plugin);
    }
  }
  else
    return((st_plugin_int *)
        hash_search(&plugin_hash[type], (const uchar *)name->str, name->length));
  return(0);
}


static SHOW_COMP_OPTION plugin_status(const LEX_STRING *name, int type)
{
  SHOW_COMP_OPTION rc= SHOW_OPTION_NO;
  struct st_plugin_int *plugin;
  if ((plugin= plugin_find_internal(name, type)))
  {
    rc= SHOW_OPTION_DISABLED;
    if (plugin->state == PLUGIN_IS_READY)
      rc= SHOW_OPTION_YES;
  }
  return(rc);
}


bool plugin_is_ready(const LEX_STRING *name, int type)
{
  bool rc= false;
  if (plugin_status(name, type) == SHOW_OPTION_YES)
    rc= true;
  return rc;
}


SHOW_COMP_OPTION sys_var_have_plugin::get_option()
{
  LEX_STRING plugin_name= { (char *) plugin_name_str, plugin_name_len };
  return plugin_status(&plugin_name, plugin_type);
}


static plugin_ref intern_plugin_lock(LEX *lex, plugin_ref rc CALLER_INFO_PROTO)
{
  st_plugin_int *pi= plugin_ref_to_int(rc);

  if (pi->state & (PLUGIN_IS_READY | PLUGIN_IS_UNINITIALIZED))
  {
    plugin_ref plugin;
    /*
      For debugging, we do an additional malloc which allows the
      memory manager and/or valgrind to track locked references and
      double unlocks to aid resolving reference counting.problems.
    */
    if (!(plugin= (plugin_ref) my_malloc_ci(sizeof(pi), MYF(MY_WME))))
      return(NULL);

    *plugin= pi;
    pi->ref_count++;

    if (lex)
      insert_dynamic(&lex->plugins, (uchar*)&plugin);
    return(plugin);
  }
  return(NULL);
}


plugin_ref plugin_lock(THD *thd, plugin_ref *ptr CALLER_INFO_PROTO)
{
  LEX *lex= thd ? thd->lex : 0;
  plugin_ref rc;
  rc= my_intern_plugin_lock_ci(lex, *ptr);
  return(rc);
}


plugin_ref plugin_lock_by_name(THD *thd, const LEX_STRING *name, int type
                               CALLER_INFO_PROTO)
{
  LEX *lex= thd ? thd->lex : 0;
  plugin_ref rc= NULL;
  st_plugin_int *plugin;
  if ((plugin= plugin_find_internal(name, type)))
    rc= my_intern_plugin_lock_ci(lex, plugin_int_to_ref(plugin));
  return(rc);
}


static st_plugin_int *plugin_insert_or_reuse(struct st_plugin_int *plugin)
{
  uint i;
  struct st_plugin_int *tmp;
  for (i= 0; i < plugin_array.elements; i++)
  {
    tmp= *dynamic_element(&plugin_array, i, struct st_plugin_int **);
    if (tmp->state == PLUGIN_IS_FREED)
    {
      memcpy(tmp, plugin, sizeof(struct st_plugin_int));
      return(tmp);
    }
  }
  if (insert_dynamic(&plugin_array, (uchar*)&plugin))
    return(0);
  tmp= *dynamic_element(&plugin_array, plugin_array.elements - 1,
                        struct st_plugin_int **)=
       (struct st_plugin_int *) memdup_root(&plugin_mem_root, (uchar*)plugin,
                                            sizeof(struct st_plugin_int));
  return(tmp);
}


/*
  NOTE
    Requires that a write-lock is held on LOCK_system_variables_hash
*/
static bool plugin_add(MEM_ROOT *tmp_root,
                       const LEX_STRING *name, const LEX_STRING *dl,
                       int *argc, char **argv, int report)
{
  struct st_plugin_int tmp;
  struct st_mysql_plugin *plugin;
  if (plugin_find_internal(name, MYSQL_ANY_PLUGIN))
  {
    if (report & REPORT_TO_USER)
      my_error(ER_UDF_EXISTS, MYF(0), name->str);
    if (report & REPORT_TO_LOG)
      sql_print_error(ER(ER_UDF_EXISTS), name->str);
    return(true);
  }
  /* Clear the whole struct to catch future extensions. */
  bzero((char*) &tmp, sizeof(tmp));
  if (! (tmp.plugin_dl= plugin_dl_add(dl, report)))
    return(true);
  /* Find plugin by name */
  for (plugin= tmp.plugin_dl->plugins; plugin->name; plugin++)
  {
    uint name_len= strlen(plugin->name);
    if (plugin->type >= 0 && plugin->type < MYSQL_MAX_PLUGIN_TYPE_NUM &&
        ! my_strnncoll(system_charset_info,
                       (const uchar *)name->str, name->length,
                       (const uchar *)plugin->name,
                       name_len))
    {
      struct st_plugin_int *tmp_plugin_ptr;

      tmp.plugin= plugin;
      tmp.name.str= (char *)plugin->name;
      tmp.name.length= name_len;
      tmp.ref_count= 0;
      tmp.state= PLUGIN_IS_UNINITIALIZED;
      if (!test_plugin_options(tmp_root, &tmp, argc, argv))
      {
        if ((tmp_plugin_ptr= plugin_insert_or_reuse(&tmp)))
        {
          plugin_array_version++;
          if (!my_hash_insert(&plugin_hash[plugin->type], (uchar*)tmp_plugin_ptr))
          {
            init_alloc_root(&tmp_plugin_ptr->mem_root, 4096, 4096);
            return(false);
          }
          tmp_plugin_ptr->state= PLUGIN_IS_FREED;
        }
        mysql_del_sys_var_chain(tmp.system_vars);
        goto err;
      }
      /* plugin was disabled */
      plugin_dl_del(dl);
      return(false);
    }
  }
  if (report & REPORT_TO_USER)
    my_error(ER_CANT_FIND_DL_ENTRY, MYF(0), name->str);
  if (report & REPORT_TO_LOG)
    sql_print_error(ER(ER_CANT_FIND_DL_ENTRY), name->str);
err:
  plugin_dl_del(dl);
  return(true);
}


static void plugin_deinitialize(struct st_plugin_int *plugin, bool ref_check)
{
  if (plugin->plugin->status_vars)
  {
#ifdef FIX_LATER
    /*
      We have a problem right now where we can not prepend without
      breaking backwards compatibility. We will fix this shortly so
      that engines have "use names" and we wil use those for
      CREATE TABLE, and use the plugin name then for adding automatic
      variable names.
    */
    SHOW_VAR array[2]= {
      {plugin->plugin->name, (char*)plugin->plugin->status_vars, SHOW_ARRAY},
      {0, 0, SHOW_UNDEF}
    };
    remove_status_vars(array);
#else
    remove_status_vars(plugin->plugin->status_vars);
#endif /* FIX_LATER */
  }

  if (plugin_type_deinitialize[plugin->plugin->type])
  {
    if ((*plugin_type_deinitialize[plugin->plugin->type])(plugin))
    {
      sql_print_error("Plugin '%s' of type %s failed deinitialization",
                      plugin->name.str, plugin_type_names[plugin->plugin->type].str);
    }
  }
  else if (plugin->plugin->deinit)
    plugin->plugin->deinit(plugin);

  plugin->state= PLUGIN_IS_UNINITIALIZED;

  /*
    We do the check here because NDB has a worker THD which doesn't
    exit until NDB is shut down.
  */
  if (ref_check && plugin->ref_count)
    sql_print_error("Plugin '%s' has ref_count=%d after deinitialization.",
                    plugin->name.str, plugin->ref_count);
}


static void plugin_del(struct st_plugin_int *plugin)
{
  /* Free allocated strings before deleting the plugin. */
  plugin_vars_free_values(plugin->system_vars);
  hash_delete(&plugin_hash[plugin->plugin->type], (uchar*)plugin);
  if (plugin->plugin_dl)
    plugin_dl_del(&plugin->plugin_dl->dl);
  plugin->state= PLUGIN_IS_FREED;
  plugin_array_version++;
  rw_wrlock(&LOCK_system_variables_hash);
  mysql_del_sys_var_chain(plugin->system_vars);
  rw_unlock(&LOCK_system_variables_hash);
  free_root(&plugin->mem_root, MYF(0));
  return;
}

static void reap_plugins(void)
{
  uint count, idx;
  struct st_plugin_int *plugin, **reap, **list;

  if (!reap_needed)
    return;

  reap_needed= false;
  count= plugin_array.elements;
  reap= (struct st_plugin_int **)my_alloca(sizeof(plugin)*(count+1));
  *(reap++)= NULL;

  for (idx= 0; idx < count; idx++)
  {
    plugin= *dynamic_element(&plugin_array, idx, struct st_plugin_int **);
    if (plugin->state == PLUGIN_IS_DELETED && !plugin->ref_count)
    {
      /* change the status flag to prevent reaping by another thread */
      plugin->state= PLUGIN_IS_DYING;
      *(reap++)= plugin;
    }
  }

  list= reap;
  while ((plugin= *(--list)))
    plugin_deinitialize(plugin, true);

  while ((plugin= *(--reap)))
    plugin_del(plugin);

  my_afree(reap);
}

static void intern_plugin_unlock(LEX *lex, plugin_ref plugin)
{
  int i;
  st_plugin_int *pi;

  if (!plugin)
    return;

  pi= plugin_ref_to_int(plugin);

  my_free((uchar*) plugin, MYF(MY_WME));

  if (lex)
  {
    /*
      Remove one instance of this plugin from the use list.
      We are searching backwards so that plugins locked last
      could be unlocked faster - optimizing for LIFO semantics.
    */
    for (i= lex->plugins.elements - 1; i >= 0; i--)
      if (plugin == *dynamic_element(&lex->plugins, i, plugin_ref*))
      {
        delete_dynamic_element(&lex->plugins, i);
        break;
      }
    assert(i >= 0);
  }

  assert(pi->ref_count);
  pi->ref_count--;

  if (pi->state == PLUGIN_IS_DELETED && !pi->ref_count)
    reap_needed= true;

  return;
}


void plugin_unlock(THD *thd, plugin_ref plugin)
{
  LEX *lex= thd ? thd->lex : 0;
  if (!plugin)
    return;
  intern_plugin_unlock(lex, plugin);
  return;
}


void plugin_unlock_list(THD *thd, plugin_ref *list, uint count)
{
  LEX *lex= thd ? thd->lex : 0;
  assert(list);
  while (count--)
    intern_plugin_unlock(lex, *list++);
  return;
}


static int plugin_initialize(struct st_plugin_int *plugin)
{

  if (plugin_type_initialize[plugin->plugin->type])
  {
    if ((*plugin_type_initialize[plugin->plugin->type])(plugin))
    {
      sql_print_error("Plugin '%s' registration as a %s failed.",
                      plugin->name.str, plugin_type_names[plugin->plugin->type].str);
      goto err;
    }
  }
  else if (plugin->plugin->init)
  {
    if (plugin->plugin->init(plugin))
    {
      sql_print_error("Plugin '%s' init function returned error.",
                      plugin->name.str);
      goto err;
    }
  }

  plugin->state= PLUGIN_IS_READY;

  if (plugin->plugin->status_vars)
  {
#ifdef FIX_LATER
    /*
      We have a problem right now where we can not prepend without
      breaking backwards compatibility. We will fix this shortly so
      that engines have "use names" and we wil use those for
      CREATE TABLE, and use the plugin name then for adding automatic
      variable names.
    */
    SHOW_VAR array[2]= {
      {plugin->plugin->name, (char*)plugin->plugin->status_vars, SHOW_ARRAY},
      {0, 0, SHOW_UNDEF}
    };
    if (add_status_vars(array)) // add_status_vars makes a copy
      goto err;
#else
    add_status_vars(plugin->plugin->status_vars); // add_status_vars makes a copy
#endif /* FIX_LATER */
  }

  /*
    set the plugin attribute of plugin's sys vars so they are pointing
    to the active plugin
  */
  if (plugin->system_vars)
  {
    sys_var_pluginvar *var= plugin->system_vars->cast_pluginvar();
    for (;;)
    {
      var->plugin= plugin;
      if (!var->next)
        break;
      var= var->next->cast_pluginvar();
    }
  }

  return(0);
err:
  return(1);
}


extern "C" uchar *get_plugin_hash_key(const uchar *, size_t *, bool);
extern "C" uchar *get_bookmark_hash_key(const uchar *, size_t *, bool);


uchar *get_plugin_hash_key(const uchar *buff, size_t *length,
                           bool not_used __attribute__((unused)))
{
  struct st_plugin_int *plugin= (st_plugin_int *)buff;
  *length= (uint)plugin->name.length;
  return((uchar *)plugin->name.str);
}


uchar *get_bookmark_hash_key(const uchar *buff, size_t *length,
                             bool not_used __attribute__((unused)))
{
  struct st_bookmark *var= (st_bookmark *)buff;
  *length= var->name_len + 1;
  return (uchar*) var->key;
}


/*
  The logic is that we first load and initialize all compiled in plugins.
  From there we load up the dynamic types (assuming we have not been told to
  skip this part).

  Finally we initialize everything, aka the dynamic that have yet to initialize.
*/
int plugin_init(int *argc, char **argv, int flags)
{
  uint i;
  struct st_mysql_plugin **builtins;
  struct st_mysql_plugin *plugin;
  struct st_plugin_int tmp, *plugin_ptr, **reap;
  MEM_ROOT tmp_root;

  if (initialized)
    return(0);

  init_alloc_root(&plugin_mem_root, 4096, 4096);
  init_alloc_root(&tmp_root, 4096, 4096);

  if (hash_init(&bookmark_hash, &my_charset_bin, 16, 0, 0,
                  get_bookmark_hash_key, NULL, HASH_UNIQUE))
      goto err;


  if (my_init_dynamic_array(&plugin_dl_array,
                            sizeof(struct st_plugin_dl *),16,16) ||
      my_init_dynamic_array(&plugin_array,
                            sizeof(struct st_plugin_int *),16,16))
    goto err;

  for (i= 0; i < MYSQL_MAX_PLUGIN_TYPE_NUM; i++)
  {
    if (hash_init(&plugin_hash[i], system_charset_info, 16, 0, 0,
                  get_plugin_hash_key, NULL, HASH_UNIQUE))
      goto err;
  }

  initialized= 1;

  /*
    First we register builtin plugins
  */
  for (builtins= mysqld_builtins; *builtins; builtins++)
  {
    for (plugin= *builtins; plugin->name; plugin++)
    {
      bzero(&tmp, sizeof(tmp));
      tmp.plugin= plugin;
      tmp.name.str= (char *)plugin->name;
      tmp.name.length= strlen(plugin->name);

      free_root(&tmp_root, MYF(MY_MARK_BLOCKS_FREE));
      if (test_plugin_options(&tmp_root, &tmp, argc, argv))
        continue;

      if (register_builtin(plugin, &tmp, &plugin_ptr))
        goto err_unlock;

      if (plugin_initialize(plugin_ptr))
        goto err_unlock;

      /*
        initialize the global default storage engine so that it may
        not be null in any child thread.
      */
      if (my_strcasecmp(&my_charset_latin1, plugin->name, "MyISAM") == 0)
      {
        assert(!global_system_variables.table_plugin);
        global_system_variables.table_plugin=
          my_intern_plugin_lock(NULL, plugin_int_to_ref(plugin_ptr));
        assert(plugin_ptr->ref_count == 1);
      }
    }
  }

  /* should now be set to MyISAM storage engine */
  assert(global_system_variables.table_plugin);

  /* Register all dynamic plugins */
  if (!(flags & PLUGIN_INIT_SKIP_DYNAMIC_LOADING))
  {
    if (opt_plugin_load)
      plugin_load_list(&tmp_root, argc, argv, opt_plugin_load);
  }

  if (flags & PLUGIN_INIT_SKIP_INITIALIZATION)
    goto end;

  /*
    Now we initialize all remaining plugins
  */

  reap= (st_plugin_int **) my_alloca((plugin_array.elements+1) * sizeof(void*));
  *(reap++)= NULL;

  for (i= 0; i < plugin_array.elements; i++)
  {
    plugin_ptr= *dynamic_element(&plugin_array, i, struct st_plugin_int **);
    if (plugin_ptr->state == PLUGIN_IS_UNINITIALIZED)
    {
      if (plugin_initialize(plugin_ptr))
      {
        plugin_ptr->state= PLUGIN_IS_DYING;
        *(reap++)= plugin_ptr;
      }
    }
  }

  /*
    Check if any plugins have to be reaped
  */
  while ((plugin_ptr= *(--reap)))
  {
    plugin_deinitialize(plugin_ptr, true);
    plugin_del(plugin_ptr);
  }

  my_afree(reap);

end:
  free_root(&tmp_root, MYF(0));

  return(0);

err_unlock:
err:
  free_root(&tmp_root, MYF(0));
  return(1);
}


static bool register_builtin(struct st_mysql_plugin *plugin,
                             struct st_plugin_int *tmp,
                             struct st_plugin_int **ptr)
{

  tmp->state= PLUGIN_IS_UNINITIALIZED;
  tmp->ref_count= 0;
  tmp->plugin_dl= 0;

  if (insert_dynamic(&plugin_array, (uchar*)&tmp))
    return(1);

  *ptr= *dynamic_element(&plugin_array, plugin_array.elements - 1,
                         struct st_plugin_int **)=
        (struct st_plugin_int *) memdup_root(&plugin_mem_root, (uchar*)tmp,
                                             sizeof(struct st_plugin_int));

  if (my_hash_insert(&plugin_hash[plugin->type],(uchar*) *ptr))
    return(1);

  return(0);
}


/*
  called only by plugin_init()
*/
static bool plugin_load_list(MEM_ROOT *tmp_root, int *argc, char **argv,
                             const char *list)
{
  char buffer[FN_REFLEN];
  LEX_STRING name= {buffer, 0}, dl= {NULL, 0}, *str= &name;
  struct st_plugin_dl *plugin_dl;
  struct st_mysql_plugin *plugin;
  char *p= buffer;
  while (list)
  {
    if (p == buffer + sizeof(buffer) - 1)
    {
      sql_print_error("plugin-load parameter too long");
      return(true);
    }

    switch ((*(p++)= *(list++))) {
    case '\0':
      list= NULL; /* terminate the loop */
      /* fall through */
    case ':':     /* can't use this as delimiter as it may be drive letter */
    case ';':
      str->str[str->length]= '\0';
      if (str == &name)  // load all plugins in named module
      {
        if (!name.length)
        {
          p--;    /* reset pointer */
          continue;
        }

        dl= name;
        if ((plugin_dl= plugin_dl_add(&dl, REPORT_TO_LOG)))
        {
          for (plugin= plugin_dl->plugins; plugin->name; plugin++)
          {
            name.str= (char *) plugin->name;
            name.length= strlen(name.str);

            free_root(tmp_root, MYF(MY_MARK_BLOCKS_FREE));
            if (plugin_add(tmp_root, &name, &dl, argc, argv, REPORT_TO_LOG))
              goto error;
          }
          plugin_dl_del(&dl); // reduce ref count
        }
      }
      else
      {
        free_root(tmp_root, MYF(MY_MARK_BLOCKS_FREE));
        if (plugin_add(tmp_root, &name, &dl, argc, argv, REPORT_TO_LOG))
          goto error;
      }
      name.length= dl.length= 0;
      dl.str= NULL; name.str= p= buffer;
      str= &name;
      continue;
    case '=':
    case '#':
      if (str == &name)
      {
        name.str[name.length]= '\0';
        str= &dl;
        str->str= p;
        continue;
      }
    default:
      str->length++;
      continue;
    }
  }
  return(false);
error:
  sql_print_error("Couldn't load plugin named '%s' with soname '%s'.",
                  name.str, dl.str);
  return(true);
}


void plugin_shutdown(void)
{
  uint i, count= plugin_array.elements, free_slots= 0;
  struct st_plugin_int **plugins, *plugin;
  struct st_plugin_dl **dl;

  if (initialized)
  {
    reap_needed= true;

    /*
      We want to shut down plugins in a reasonable order, this will
      become important when we have plugins which depend upon each other.
      Circular references cannot be reaped so they are forced afterwards.
      TODO: Have an additional step here to notify all active plugins that
      shutdown is requested to allow plugins to deinitialize in parallel.
    */
    while (reap_needed && (count= plugin_array.elements))
    {
      reap_plugins();
      for (i= free_slots= 0; i < count; i++)
      {
        plugin= *dynamic_element(&plugin_array, i, struct st_plugin_int **);
        switch (plugin->state) {
        case PLUGIN_IS_READY:
          plugin->state= PLUGIN_IS_DELETED;
          reap_needed= true;
          break;
        case PLUGIN_IS_FREED:
        case PLUGIN_IS_UNINITIALIZED:
          free_slots++;
          break;
        }
      }
      if (!reap_needed)
      {
        /*
          release any plugin references held.
        */
        unlock_variables(NULL, &global_system_variables);
        unlock_variables(NULL, &max_system_variables);
      }
    }

    if (count > free_slots)
      sql_print_warning("Forcing shutdown of %d plugins", count - free_slots);

    plugins= (struct st_plugin_int **) my_alloca(sizeof(void*) * (count+1));

    /*
      If we have any plugins which did not die cleanly, we force shutdown
    */
    for (i= 0; i < count; i++)
    {
      plugins[i]= *dynamic_element(&plugin_array, i, struct st_plugin_int **);
      /* change the state to ensure no reaping races */
      if (plugins[i]->state == PLUGIN_IS_DELETED)
        plugins[i]->state= PLUGIN_IS_DYING;
    }

    /*
      We loop through all plugins and call deinit() if they have one.
    */
    for (i= 0; i < count; i++)
      if (!(plugins[i]->state & (PLUGIN_IS_UNINITIALIZED | PLUGIN_IS_FREED)))
      {
        sql_print_information("Plugin '%s' will be forced to shutdown",
                              plugins[i]->name.str);
        /*
          We are forcing deinit on plugins so we don't want to do a ref_count
          check until we have processed all the plugins.
        */
        plugin_deinitialize(plugins[i], false);
      }

    /*
      We defer checking ref_counts until after all plugins are deinitialized
      as some may have worker threads holding on to plugin references.
    */
    for (i= 0; i < count; i++)
    {
      if (plugins[i]->ref_count)
        sql_print_error("Plugin '%s' has ref_count=%d after shutdown.",
                        plugins[i]->name.str, plugins[i]->ref_count);
      if (plugins[i]->state & PLUGIN_IS_UNINITIALIZED)
        plugin_del(plugins[i]);
    }

    /*
      Now we can deallocate all memory.
    */

    cleanup_variables(NULL, &global_system_variables);
    cleanup_variables(NULL, &max_system_variables);

    initialized= 0;

    my_afree(plugins);
  }

  /* Dispose of the memory */

  for (i= 0; i < MYSQL_MAX_PLUGIN_TYPE_NUM; i++)
    hash_free(&plugin_hash[i]);
  delete_dynamic(&plugin_array);

  count= plugin_dl_array.elements;
  dl= (struct st_plugin_dl **)my_alloca(sizeof(void*) * count);
  for (i= 0; i < count; i++)
    dl[i]= *dynamic_element(&plugin_dl_array, i, struct st_plugin_dl **);
  for (i= 0; i < plugin_dl_array.elements; i++)
    free_plugin_mem(dl[i]);
  my_afree(dl);
  delete_dynamic(&plugin_dl_array);

  hash_free(&bookmark_hash);
  free_root(&plugin_mem_root, MYF(0));

  global_variables_dynamic_size= 0;

  return;
}


bool plugin_foreach_with_mask(THD *thd, plugin_foreach_func *func,
                       int type, uint state_mask, void *arg)
{
  uint idx, total;
  struct st_plugin_int *plugin, **plugins;
  int version=plugin_array_version;

  if (!initialized)
    return(false);

  state_mask= ~state_mask; // do it only once

  total= type == MYSQL_ANY_PLUGIN ? plugin_array.elements
                                  : plugin_hash[type].records;
  /*
    Do the alloca out here in case we do have a working alloca:
        leaving the nested stack frame invalidates alloca allocation.
  */
  plugins=(struct st_plugin_int **)my_alloca(total*sizeof(plugin));
  if (type == MYSQL_ANY_PLUGIN)
  {
    for (idx= 0; idx < total; idx++)
    {
      plugin= *dynamic_element(&plugin_array, idx, struct st_plugin_int **);
      plugins[idx]= !(plugin->state & state_mask) ? plugin : NULL;
    }
  }
  else
  {
    HASH *hash= plugin_hash + type;
    for (idx= 0; idx < total; idx++)
    {
      plugin= (struct st_plugin_int *) hash_element(hash, idx);
      plugins[idx]= !(plugin->state & state_mask) ? plugin : NULL;
    }
  }
  for (idx= 0; idx < total; idx++)
  {
    if (unlikely(version != plugin_array_version))
    {
      for (uint i=idx; i < total; i++)
        if (plugins[i] && plugins[i]->state & state_mask)
          plugins[i]=0;
    }
    plugin= plugins[idx];
    /* It will stop iterating on first engine error when "func" returns true */
    if (plugin && func(thd, plugin_int_to_ref(plugin), arg))
        goto err;
  }

  my_afree(plugins);
  return(false);
err:
  my_afree(plugins);
  return(true);
}


/****************************************************************************
  Internal type declarations for variables support
****************************************************************************/

#undef MYSQL_SYSVAR_NAME
#define MYSQL_SYSVAR_NAME(name) name
#define PLUGIN_VAR_TYPEMASK 0x007f

#define EXTRA_OPTIONS 3 /* options for: 'foo', 'plugin-foo' and NULL */

typedef DECLARE_MYSQL_SYSVAR_BASIC(sysvar_bool_t, bool);
typedef DECLARE_MYSQL_THDVAR_BASIC(thdvar_bool_t, bool);
typedef DECLARE_MYSQL_SYSVAR_BASIC(sysvar_str_t, char *);
typedef DECLARE_MYSQL_THDVAR_BASIC(thdvar_str_t, char *);

typedef DECLARE_MYSQL_SYSVAR_TYPELIB(sysvar_enum_t, unsigned long);
typedef DECLARE_MYSQL_THDVAR_TYPELIB(thdvar_enum_t, unsigned long);
typedef DECLARE_MYSQL_SYSVAR_TYPELIB(sysvar_set_t, uint64_t);
typedef DECLARE_MYSQL_THDVAR_TYPELIB(thdvar_set_t, uint64_t);

typedef DECLARE_MYSQL_SYSVAR_SIMPLE(sysvar_int_t, int);
typedef DECLARE_MYSQL_SYSVAR_SIMPLE(sysvar_long_t, long);
typedef DECLARE_MYSQL_SYSVAR_SIMPLE(sysvar_int64_t_t, int64_t);
typedef DECLARE_MYSQL_SYSVAR_SIMPLE(sysvar_uint_t, uint);
typedef DECLARE_MYSQL_SYSVAR_SIMPLE(sysvar_ulong_t, ulong);
typedef DECLARE_MYSQL_SYSVAR_SIMPLE(sysvar_uint64_t_t, uint64_t);

typedef DECLARE_MYSQL_THDVAR_SIMPLE(thdvar_int_t, int);
typedef DECLARE_MYSQL_THDVAR_SIMPLE(thdvar_long_t, long);
typedef DECLARE_MYSQL_THDVAR_SIMPLE(thdvar_int64_t_t, int64_t);
typedef DECLARE_MYSQL_THDVAR_SIMPLE(thdvar_uint_t, uint);
typedef DECLARE_MYSQL_THDVAR_SIMPLE(thdvar_ulong_t, ulong);
typedef DECLARE_MYSQL_THDVAR_SIMPLE(thdvar_uint64_t_t, uint64_t);

typedef bool *(*mysql_sys_var_ptr_p)(THD* a_thd, int offset);


/****************************************************************************
  default variable data check and update functions
****************************************************************************/

static int check_func_bool(THD *thd __attribute__((__unused__)),
                           struct st_mysql_sys_var *var,
                           void *save, st_mysql_value *value)
{
  char buff[STRING_BUFFER_USUAL_SIZE];
  const char *strvalue= "NULL", *str;
  int result, length;
  int64_t tmp;

  if (value->value_type(value) == MYSQL_VALUE_TYPE_STRING)
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
      llstr(tmp, buff);
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


static int check_func_int(THD *thd, struct st_mysql_sys_var *var,
                          void *save, st_mysql_value *value)
{
  bool fixed;
  int64_t tmp;
  struct my_option options;
  value->val_int(value, &tmp);
  plugin_opt_set_limits(&options, var);

  if (var->flags & PLUGIN_VAR_UNSIGNED)
    *(uint *)save= (uint) getopt_ull_limit_value((uint64_t) tmp, &options,
                                                   &fixed);
  else
    *(int *)save= (int) getopt_ll_limit_value(tmp, &options, &fixed);

  return throw_bounds_warning(thd, fixed, var->flags & PLUGIN_VAR_UNSIGNED,
                              var->name, (int64_t) tmp);
}


static int check_func_long(THD *thd, struct st_mysql_sys_var *var,
                          void *save, st_mysql_value *value)
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

  return throw_bounds_warning(thd, fixed, var->flags & PLUGIN_VAR_UNSIGNED,
                              var->name, (int64_t) tmp);
}


static int check_func_int64_t(THD *thd, struct st_mysql_sys_var *var,
                               void *save, st_mysql_value *value)
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

  return throw_bounds_warning(thd, fixed, var->flags & PLUGIN_VAR_UNSIGNED,
                              var->name, (int64_t) tmp);
}

static int check_func_str(THD *thd,
                          struct st_mysql_sys_var *var __attribute__((__unused__)),
                          void *save, st_mysql_value *value)
{
  char buff[STRING_BUFFER_USUAL_SIZE];
  const char *str;
  int length;

  length= sizeof(buff);
  if ((str= value->val_str(value, buff, &length)))
    str= thd->strmake(str, length);
  *(const char**)save= str;
  return 0;
}


static int check_func_enum(THD *thd __attribute__((__unused__)),
                           struct st_mysql_sys_var *var,
                           void *save, st_mysql_value *value)
{
  char buff[STRING_BUFFER_USUAL_SIZE];
  const char *strvalue= "NULL", *str;
  TYPELIB *typelib;
  int64_t tmp;
  long result;
  int length;

  if (var->flags & PLUGIN_VAR_THDLOCAL)
    typelib= ((thdvar_enum_t*) var)->typelib;
  else
    typelib= ((sysvar_enum_t*) var)->typelib;

  if (value->value_type(value) == MYSQL_VALUE_TYPE_STRING)
  {
    length= sizeof(buff);
    if (!(str= value->val_str(value, buff, &length)))
      goto err;
    if ((result= (long)find_type(typelib, str, length, 1)-1) < 0)
    {
      strvalue= str;
      goto err;
    }
  }
  else
  {
    if (value->val_int(value, &tmp))
      goto err;
    if (tmp >= typelib->count)
    {
      llstr(tmp, buff);
      strvalue= buff;
      goto err;
    }
    result= (long) tmp;
  }
  *(long*)save= result;
  return 0;
err:
  my_error(ER_WRONG_VALUE_FOR_VAR, MYF(0), var->name, strvalue);
  return 1;
}


static int check_func_set(THD *thd __attribute__((__unused__)),
                          struct st_mysql_sys_var *var,
                          void *save, st_mysql_value *value)
{
  char buff[STRING_BUFFER_USUAL_SIZE], *error= 0;
  const char *strvalue= "NULL", *str;
  TYPELIB *typelib;
  uint64_t result;
  uint error_len;
  bool not_used;
  int length;

  if (var->flags & PLUGIN_VAR_THDLOCAL)
    typelib= ((thdvar_set_t*) var)->typelib;
  else
    typelib= ((sysvar_set_t*)var)->typelib;

  if (value->value_type(value) == MYSQL_VALUE_TYPE_STRING)
  {
    length= sizeof(buff);
    if (!(str= value->val_str(value, buff, &length)))
      goto err;
    result= find_set(typelib, str, length, NULL,
                     &error, &error_len, &not_used);
    if (error_len)
    {
      strmake(buff, error, min(sizeof(buff), error_len));
      strvalue= buff;
      goto err;
    }
  }
  else
  {
    if (value->val_int(value, (int64_t *)&result))
      goto err;
    if (unlikely((result >= (1ULL << typelib->count)) &&
                 (typelib->count < sizeof(long)*8)))
    {
      llstr(result, buff);
      strvalue= buff;
      goto err;
    }
  }
  *(uint64_t*)save= result;
  return 0;
err:
  my_error(ER_WRONG_VALUE_FOR_VAR, MYF(0), var->name, strvalue);
  return 1;
}


static void update_func_bool(THD *thd __attribute__((__unused__)),
                             struct st_mysql_sys_var *var __attribute__((__unused__)),
                             void *tgt, const void *save)
{
  *(bool *) tgt= *(int *) save ? 1 : 0;
}


static void update_func_int(THD *thd __attribute__((__unused__)),
                            struct st_mysql_sys_var *var __attribute__((__unused__)),
                             void *tgt, const void *save)
{
  *(int *)tgt= *(int *) save;
}


static void update_func_long(THD *thd __attribute__((__unused__)),
                             struct st_mysql_sys_var *var __attribute__((__unused__)),
                             void *tgt, const void *save)
{
  *(long *)tgt= *(long *) save;
}


static void update_func_int64_t(THD *thd __attribute__((__unused__)),
                                 struct st_mysql_sys_var *var __attribute__((__unused__)),
                                 void *tgt, const void *save)
{
  *(int64_t *)tgt= *(uint64_t *) save;
}


static void update_func_str(THD *thd __attribute__((__unused__)), struct st_mysql_sys_var *var,
                             void *tgt, const void *save)
{
  char *old= *(char **) tgt;
  *(char **)tgt= *(char **) save;
  if (var->flags & PLUGIN_VAR_MEMALLOC)
  {
    *(char **)tgt= my_strdup(*(char **) save, MYF(0));
    my_free(old, MYF(0));
  }
}


/****************************************************************************
  System Variables support
****************************************************************************/


sys_var *find_sys_var(THD *thd, const char *str, uint length)
{
  sys_var *var;
  sys_var_pluginvar *pi= NULL;
  plugin_ref plugin;

  rw_rdlock(&LOCK_system_variables_hash);
  if ((var= intern_find_sys_var(str, length, false)) &&
      (pi= var->cast_pluginvar()))
  {
    rw_unlock(&LOCK_system_variables_hash);
    LEX *lex= thd ? thd->lex : 0;
    if (!(plugin= my_intern_plugin_lock(lex, plugin_int_to_ref(pi->plugin))))
      var= NULL; /* failed to lock it, it must be uninstalling */
    else
    if (!(plugin_state(plugin) & PLUGIN_IS_READY))
    {
      /* initialization not completed */
      var= NULL;
      intern_plugin_unlock(lex, plugin);
    }
  }
  else
    rw_unlock(&LOCK_system_variables_hash);

  /*
    If the variable exists but the plugin it is associated with is not ready
    then the intern_plugin_lock did not raise an error, so we do it here.
  */
  if (pi && !var)
    my_error(ER_UNKNOWN_SYSTEM_VARIABLE, MYF(0), (char*) str);
  return(var);
}


/*
  called by register_var, construct_options and test_plugin_options.
  Returns the 'bookmark' for the named variable.
  LOCK_system_variables_hash should be at least read locked
*/
static st_bookmark *find_bookmark(const char *plugin, const char *name, int flags)
{
  st_bookmark *result= NULL;
  uint namelen, length, pluginlen= 0;
  char *varname, *p;

  if (!(flags & PLUGIN_VAR_THDLOCAL))
    return NULL;

  namelen= strlen(name);
  if (plugin)
    pluginlen= strlen(plugin) + 1;
  length= namelen + pluginlen + 2;
  varname= (char*) my_alloca(length);

  if (plugin)
  {
    strxmov(varname + 1, plugin, "_", name, NullS);
    for (p= varname + 1; *p; p++)
      if (*p == '-')
        *p= '_';
  }
  else
    memcpy(varname + 1, name, namelen + 1);

  varname[0]= flags & PLUGIN_VAR_TYPEMASK;

  result= (st_bookmark*) hash_search(&bookmark_hash,
                                     (const uchar*) varname, length - 1);

  my_afree(varname);
  return result;
}


/*
  returns a bookmark for thd-local variables, creating if neccessary.
  returns null for non thd-local variables.
  Requires that a write lock is obtained on LOCK_system_variables_hash
*/
static st_bookmark *register_var(const char *plugin, const char *name,
                                 int flags)
{
  uint length= strlen(plugin) + strlen(name) + 3, size= 0, offset, new_size;
  st_bookmark *result;
  char *varname, *p;

  if (!(flags & PLUGIN_VAR_THDLOCAL))
    return NULL;

  switch (flags & PLUGIN_VAR_TYPEMASK) {
  case PLUGIN_VAR_BOOL:
    size= sizeof(bool);
    break;
  case PLUGIN_VAR_INT:
    size= sizeof(int);
    break;
  case PLUGIN_VAR_LONG:
  case PLUGIN_VAR_ENUM:
    size= sizeof(long);
    break;
  case PLUGIN_VAR_LONGLONG:
  case PLUGIN_VAR_SET:
    size= sizeof(uint64_t);
    break;
  case PLUGIN_VAR_STR:
    size= sizeof(char*);
    break;
  default:
    assert(0);
    return NULL;
  };

  varname= ((char*) my_alloca(length));
  strxmov(varname + 1, plugin, "_", name, NullS);
  for (p= varname + 1; *p; p++)
    if (*p == '-')
      *p= '_';

  if (!(result= find_bookmark(NULL, varname + 1, flags)))
  {
    result= (st_bookmark*) alloc_root(&plugin_mem_root,
                                      sizeof(struct st_bookmark) + length-1);
    varname[0]= flags & PLUGIN_VAR_TYPEMASK;
    memcpy(result->key, varname, length);
    result->name_len= length - 2;
    result->offset= -1;

    assert(size && !(size & (size-1))); /* must be power of 2 */

    offset= global_system_variables.dynamic_variables_size;
    offset= (offset + size - 1) & ~(size - 1);
    result->offset= (int) offset;

    new_size= (offset + size + 63) & ~63;

    if (new_size > global_variables_dynamic_size)
    {
      global_system_variables.dynamic_variables_ptr= (char*)
        my_realloc(global_system_variables.dynamic_variables_ptr, new_size,
                   MYF(MY_WME | MY_FAE | MY_ALLOW_ZERO_PTR));
      max_system_variables.dynamic_variables_ptr= (char*)
        my_realloc(max_system_variables.dynamic_variables_ptr, new_size,
                   MYF(MY_WME | MY_FAE | MY_ALLOW_ZERO_PTR));
      /*
        Clear the new variable value space. This is required for string
        variables. If their value is non-NULL, it must point to a valid
        string.
      */
      bzero(global_system_variables.dynamic_variables_ptr +
            global_variables_dynamic_size,
            new_size - global_variables_dynamic_size);
      bzero(max_system_variables.dynamic_variables_ptr +
            global_variables_dynamic_size,
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
    if (my_hash_insert(&bookmark_hash, (uchar*) result))
    {
      fprintf(stderr, "failed to add placeholder to hash");
      assert(0);
    }
  }
  my_afree(varname);
  return result;
}


/*
  returns a pointer to the memory which holds the thd-local variable or
  a pointer to the global variable if thd==null.
  If required, will sync with global variables if the requested variable
  has not yet been allocated in the current thread.
*/
static uchar *intern_sys_var_ptr(THD* thd, int offset, bool global_lock)
{
  assert(offset >= 0);
  assert((uint)offset <= global_system_variables.dynamic_variables_head);

  if (!thd)
    return (uchar*) global_system_variables.dynamic_variables_ptr + offset;

  /*
    dynamic_variables_head points to the largest valid offset
  */
  if (!thd->variables.dynamic_variables_ptr ||
      (uint)offset > thd->variables.dynamic_variables_head)
  {
    uint idx;

    rw_rdlock(&LOCK_system_variables_hash);

    thd->variables.dynamic_variables_ptr= (char*)
      my_realloc(thd->variables.dynamic_variables_ptr,
                 global_variables_dynamic_size,
                 MYF(MY_WME | MY_FAE | MY_ALLOW_ZERO_PTR));

    if (global_lock)
      pthread_mutex_lock(&LOCK_global_system_variables);

    safe_mutex_assert_owner(&LOCK_global_system_variables);

    memcpy(thd->variables.dynamic_variables_ptr +
             thd->variables.dynamic_variables_size,
           global_system_variables.dynamic_variables_ptr +
             thd->variables.dynamic_variables_size,
           global_system_variables.dynamic_variables_size -
             thd->variables.dynamic_variables_size);

    /*
      now we need to iterate through any newly copied 'defaults'
      and if it is a string type with MEMALLOC flag, we need to strdup
    */
    for (idx= 0; idx < bookmark_hash.records; idx++)
    {
      sys_var_pluginvar *pi;
      sys_var *var;
      st_bookmark *v= (st_bookmark*) hash_element(&bookmark_hash,idx);

      if (v->version <= thd->variables.dynamic_variables_version ||
          !(var= intern_find_sys_var(v->key + 1, v->name_len, true)) ||
          !(pi= var->cast_pluginvar()) ||
          v->key[0] != (pi->plugin_var->flags & PLUGIN_VAR_TYPEMASK))
        continue;

      /* Here we do anything special that may be required of the data types */

      if ((pi->plugin_var->flags & PLUGIN_VAR_TYPEMASK) == PLUGIN_VAR_STR &&
          pi->plugin_var->flags & PLUGIN_VAR_MEMALLOC)
      {
         char **pp= (char**) (thd->variables.dynamic_variables_ptr +
                             *(int*)(pi->plugin_var + 1));
         if ((*pp= *(char**) (global_system_variables.dynamic_variables_ptr +
                             *(int*)(pi->plugin_var + 1))))
           *pp= my_strdup(*pp, MYF(MY_WME|MY_FAE));
      }
    }

    if (global_lock)
      pthread_mutex_unlock(&LOCK_global_system_variables);

    thd->variables.dynamic_variables_version=
           global_system_variables.dynamic_variables_version;
    thd->variables.dynamic_variables_head=
           global_system_variables.dynamic_variables_head;
    thd->variables.dynamic_variables_size=
           global_system_variables.dynamic_variables_size;

    rw_unlock(&LOCK_system_variables_hash);
  }
  return (uchar*)thd->variables.dynamic_variables_ptr + offset;
}

static bool *mysql_sys_var_ptr_bool(THD* a_thd, int offset)
{
  return (bool *)intern_sys_var_ptr(a_thd, offset, true);
}

static int *mysql_sys_var_ptr_int(THD* a_thd, int offset)
{
  return (int *)intern_sys_var_ptr(a_thd, offset, true);
}

static long *mysql_sys_var_ptr_long(THD* a_thd, int offset)
{
  return (long *)intern_sys_var_ptr(a_thd, offset, true);
}

static int64_t *mysql_sys_var_ptr_int64_t(THD* a_thd, int offset)
{
  return (int64_t *)intern_sys_var_ptr(a_thd, offset, true);
}

static char **mysql_sys_var_ptr_str(THD* a_thd, int offset)
{
  return (char **)intern_sys_var_ptr(a_thd, offset, true);
}

static uint64_t *mysql_sys_var_ptr_set(THD* a_thd, int offset)
{
  return (uint64_t *)intern_sys_var_ptr(a_thd, offset, true);
}

static unsigned long *mysql_sys_var_ptr_enum(THD* a_thd, int offset)
{
  return (unsigned long *)intern_sys_var_ptr(a_thd, offset, true);
}


void plugin_thdvar_init(THD *thd)
{
  plugin_ref old_table_plugin= thd->variables.table_plugin;
  
  thd->variables.table_plugin= NULL;
  cleanup_variables(thd, &thd->variables);
  
  thd->variables= global_system_variables;
  thd->variables.table_plugin= NULL;

  /* we are going to allocate these lazily */
  thd->variables.dynamic_variables_version= 0;
  thd->variables.dynamic_variables_size= 0;
  thd->variables.dynamic_variables_ptr= 0;

  thd->variables.table_plugin=
        my_intern_plugin_lock(NULL, global_system_variables.table_plugin);
  intern_plugin_unlock(NULL, old_table_plugin);
  return;
}


/*
  Unlocks all system variables which hold a reference
*/
static void unlock_variables(THD *thd __attribute__((__unused__)),
                             struct system_variables *vars)
{
  intern_plugin_unlock(NULL, vars->table_plugin);
  vars->table_plugin= NULL;
}


/*
  Frees memory used by system variables

  Unlike plugin_vars_free_values() it frees all variables of all plugins,
  it's used on shutdown.
*/
static void cleanup_variables(THD *thd, struct system_variables *vars)
{
  st_bookmark *v;
  sys_var_pluginvar *pivar;
  sys_var *var;
  int flags;
  uint idx;

  rw_rdlock(&LOCK_system_variables_hash);
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
        flags & PLUGIN_VAR_THDLOCAL && flags & PLUGIN_VAR_MEMALLOC)
    {
      char **ptr= (char**) pivar->real_value_ptr(thd, OPT_SESSION);
      my_free(*ptr, MYF(MY_WME | MY_FAE | MY_ALLOW_ZERO_PTR));
      *ptr= NULL;
    }
  }
  rw_unlock(&LOCK_system_variables_hash);

  assert(vars->table_plugin == NULL);

  my_free(vars->dynamic_variables_ptr, MYF(MY_ALLOW_ZERO_PTR));
  vars->dynamic_variables_ptr= NULL;
  vars->dynamic_variables_size= 0;
  vars->dynamic_variables_version= 0;
}


void plugin_thdvar_cleanup(THD *thd)
{
  uint idx;
  plugin_ref *list;

  unlock_variables(thd, &thd->variables);
  cleanup_variables(thd, &thd->variables);

  if ((idx= thd->lex->plugins.elements))
  {
    list= ((plugin_ref*) thd->lex->plugins.buffer) + idx - 1;
    while ((uchar*) list >= thd->lex->plugins.buffer)
      intern_plugin_unlock(NULL, *list--);
  }

  reset_dynamic(&thd->lex->plugins);

  return;
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

  for (sys_var *var= vars; var; var= var->next)
  {
    sys_var_pluginvar *piv= var->cast_pluginvar();
    if (piv &&
        ((piv->plugin_var->flags & PLUGIN_VAR_TYPEMASK) == PLUGIN_VAR_STR) &&
        (piv->plugin_var->flags & PLUGIN_VAR_MEMALLOC))
    {
      /* Free the string from global_system_variables. */
      char **valptr= (char**) piv->real_value_ptr(NULL, OPT_GLOBAL);
      my_free(*valptr, MYF(MY_WME | MY_FAE | MY_ALLOW_ZERO_PTR));
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
  case PLUGIN_VAR_ENUM:
  case PLUGIN_VAR_SET:
    return SHOW_CHAR;
  default:
    assert(0);
    return SHOW_UNDEF;
  }
}


uchar* sys_var_pluginvar::real_value_ptr(THD *thd, enum_var_type type)
{
  assert(thd || (type == OPT_GLOBAL));
  if (plugin_var->flags & PLUGIN_VAR_THDLOCAL)
  {
    if (type == OPT_GLOBAL)
      thd= NULL;

    return intern_sys_var_ptr(thd, *(int*) (plugin_var+1), false);
  }
  return *(uchar**) (plugin_var+1);
}


TYPELIB* sys_var_pluginvar::plugin_var_typelib(void)
{
  switch (plugin_var->flags & (PLUGIN_VAR_TYPEMASK | PLUGIN_VAR_THDLOCAL)) {
  case PLUGIN_VAR_ENUM:
    return ((sysvar_enum_t *)plugin_var)->typelib;
  case PLUGIN_VAR_SET:
    return ((sysvar_set_t *)plugin_var)->typelib;
  case PLUGIN_VAR_ENUM | PLUGIN_VAR_THDLOCAL:
    return ((thdvar_enum_t *)plugin_var)->typelib;
  case PLUGIN_VAR_SET | PLUGIN_VAR_THDLOCAL:
    return ((thdvar_set_t *)plugin_var)->typelib;
  default:
    return NULL;
  }
  return NULL;
}


uchar* sys_var_pluginvar::value_ptr(THD *thd, enum_var_type type,
                                    LEX_STRING *base __attribute__((__unused__)))
{
  uchar* result;

  result= real_value_ptr(thd, type);

  if ((plugin_var->flags & PLUGIN_VAR_TYPEMASK) == PLUGIN_VAR_ENUM)
    result= (uchar*) get_type(plugin_var_typelib(), *(ulong*)result);
  else if ((plugin_var->flags & PLUGIN_VAR_TYPEMASK) == PLUGIN_VAR_SET)
  {
    char buffer[STRING_BUFFER_USUAL_SIZE];
    String str(buffer, sizeof(buffer), system_charset_info);
    TYPELIB *typelib= plugin_var_typelib();
    uint64_t mask= 1, value= *(uint64_t*) result;
    uint i;

    str.length(0);
    for (i= 0; i < typelib->count; i++, mask<<=1)
    {
      if (!(value & mask))
        continue;
      str.append(typelib->type_names[i], typelib->type_lengths[i]);
      str.append(',');
    }

    result= (uchar*) "";
    if (str.length())
      result= (uchar*) thd->strmake(str.ptr(), str.length()-1);
  }
  return result;
}


bool sys_var_pluginvar::check(THD *thd, set_var *var)
{
  st_item_value_holder value;
  assert(is_readonly() || plugin_var->check);

  value.value_type= item_value_type;
  value.val_str= item_val_str;
  value.val_int= item_val_int;
  value.val_real= item_val_real;
  value.item= var->value;

  return is_readonly() ||
         plugin_var->check(thd, plugin_var, &var->save_result, &value);
}


void sys_var_pluginvar::set_default(THD *thd, enum_var_type type)
{
  const void *src;
  void *tgt;

  assert(is_readonly() || plugin_var->update);

  if (is_readonly())
    return;

  pthread_mutex_lock(&LOCK_global_system_variables);
  tgt= real_value_ptr(thd, type);
  src= ((void **) (plugin_var + 1) + 1);

  if (plugin_var->flags & PLUGIN_VAR_THDLOCAL)
  {
    if (type != OPT_GLOBAL)
      src= real_value_ptr(thd, OPT_GLOBAL);
    else
    switch (plugin_var->flags & PLUGIN_VAR_TYPEMASK) {
	case PLUGIN_VAR_INT:
	  src= &((thdvar_uint_t*) plugin_var)->def_val;
	  break;
	case PLUGIN_VAR_LONG:
	  src= &((thdvar_ulong_t*) plugin_var)->def_val;
	  break;
	case PLUGIN_VAR_LONGLONG:
	  src= &((thdvar_uint64_t_t*) plugin_var)->def_val;
	  break;
	case PLUGIN_VAR_ENUM:
	  src= &((thdvar_enum_t*) plugin_var)->def_val;
	  break;
	case PLUGIN_VAR_SET:
	  src= &((thdvar_set_t*) plugin_var)->def_val;
	  break;
	case PLUGIN_VAR_BOOL:
	  src= &((thdvar_bool_t*) plugin_var)->def_val;
	  break;
	case PLUGIN_VAR_STR:
	  src= &((thdvar_str_t*) plugin_var)->def_val;
	  break;
	default:
	  assert(0);
	}
  }

  /* thd must equal current_thd if PLUGIN_VAR_THDLOCAL flag is set */
  assert(!(plugin_var->flags & PLUGIN_VAR_THDLOCAL) ||
              thd == current_thd);

  if (!(plugin_var->flags & PLUGIN_VAR_THDLOCAL) || type == OPT_GLOBAL)
  {
    plugin_var->update(thd, plugin_var, tgt, src);
    pthread_mutex_unlock(&LOCK_global_system_variables);
  }
  else
  {
    pthread_mutex_unlock(&LOCK_global_system_variables);
    plugin_var->update(thd, plugin_var, tgt, src);
  }
}


bool sys_var_pluginvar::update(THD *thd, set_var *var)
{
  void *tgt;

  assert(is_readonly() || plugin_var->update);

  /* thd must equal current_thd if PLUGIN_VAR_THDLOCAL flag is set */
  assert(!(plugin_var->flags & PLUGIN_VAR_THDLOCAL) ||
              thd == current_thd);

  if (is_readonly())
    return 1;

  pthread_mutex_lock(&LOCK_global_system_variables);
  tgt= real_value_ptr(thd, var->type);

  if (!(plugin_var->flags & PLUGIN_VAR_THDLOCAL) || var->type == OPT_GLOBAL)
  {
    /* variable we are updating has global scope, so we unlock after updating */
    plugin_var->update(thd, plugin_var, tgt, &var->save_result);
    pthread_mutex_unlock(&LOCK_global_system_variables);
  }
  else
  {
    pthread_mutex_unlock(&LOCK_global_system_variables);
    plugin_var->update(thd, plugin_var, tgt, &var->save_result);
  }
 return 0;
}


#define OPTION_SET_LIMITS(type, options, opt) \
  options->var_type= type; \
  options->def_value= (opt)->def_val; \
  options->min_value= (opt)->min_val; \
  options->max_value= (opt)->max_val; \
  options->block_size= (long) (opt)->blk_sz


static void plugin_opt_set_limits(struct my_option *options,
                                  const struct st_mysql_sys_var *opt)
{
  options->sub_size= 0;

  switch (opt->flags & (PLUGIN_VAR_TYPEMASK |
                        PLUGIN_VAR_UNSIGNED | PLUGIN_VAR_THDLOCAL)) {
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
    OPTION_SET_LIMITS(GET_ULONG, options, (sysvar_ulong_t*) opt);
    break;
  case PLUGIN_VAR_LONGLONG:
    OPTION_SET_LIMITS(GET_LL, options, (sysvar_int64_t_t*) opt);
    break;
  case PLUGIN_VAR_LONGLONG | PLUGIN_VAR_UNSIGNED:
    OPTION_SET_LIMITS(GET_ULL, options, (sysvar_uint64_t_t*) opt);
    break;
  case PLUGIN_VAR_ENUM:
    options->var_type= GET_ENUM;
    options->typelib= ((sysvar_enum_t*) opt)->typelib;
    options->def_value= ((sysvar_enum_t*) opt)->def_val;
    options->min_value= options->block_size= 0;
    options->max_value= options->typelib->count - 1;
    break;
  case PLUGIN_VAR_SET:
    options->var_type= GET_SET;
    options->typelib= ((sysvar_set_t*) opt)->typelib;
    options->def_value= ((sysvar_set_t*) opt)->def_val;
    options->min_value= options->block_size= 0;
    options->max_value= (1ULL << options->typelib->count) - 1;
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
  case PLUGIN_VAR_INT | PLUGIN_VAR_THDLOCAL:
    OPTION_SET_LIMITS(GET_INT, options, (thdvar_int_t*) opt);
    break;
  case PLUGIN_VAR_INT | PLUGIN_VAR_UNSIGNED | PLUGIN_VAR_THDLOCAL:
    OPTION_SET_LIMITS(GET_UINT, options, (thdvar_uint_t*) opt);
    break;
  case PLUGIN_VAR_LONG | PLUGIN_VAR_THDLOCAL:
    OPTION_SET_LIMITS(GET_LONG, options, (thdvar_long_t*) opt);
    break;
  case PLUGIN_VAR_LONG | PLUGIN_VAR_UNSIGNED | PLUGIN_VAR_THDLOCAL:
    OPTION_SET_LIMITS(GET_ULONG, options, (thdvar_ulong_t*) opt);
    break;
  case PLUGIN_VAR_LONGLONG | PLUGIN_VAR_THDLOCAL:
    OPTION_SET_LIMITS(GET_LL, options, (thdvar_int64_t_t*) opt);
    break;
  case PLUGIN_VAR_LONGLONG | PLUGIN_VAR_UNSIGNED | PLUGIN_VAR_THDLOCAL:
    OPTION_SET_LIMITS(GET_ULL, options, (thdvar_uint64_t_t*) opt);
    break;
  case PLUGIN_VAR_ENUM | PLUGIN_VAR_THDLOCAL:
    options->var_type= GET_ENUM;
    options->typelib= ((thdvar_enum_t*) opt)->typelib;
    options->def_value= ((thdvar_enum_t*) opt)->def_val;
    options->min_value= options->block_size= 0;
    options->max_value= options->typelib->count - 1;
    break;
  case PLUGIN_VAR_SET | PLUGIN_VAR_THDLOCAL:
    options->var_type= GET_SET;
    options->typelib= ((thdvar_set_t*) opt)->typelib;
    options->def_value= ((thdvar_set_t*) opt)->def_val;
    options->min_value= options->block_size= 0;
    options->max_value= (1ULL << options->typelib->count) - 1;
    break;
  case PLUGIN_VAR_BOOL | PLUGIN_VAR_THDLOCAL:
    options->var_type= GET_BOOL;
    options->def_value= ((thdvar_bool_t*) opt)->def_val;
    break;
  case PLUGIN_VAR_STR | PLUGIN_VAR_THDLOCAL:
    options->var_type= ((opt->flags & PLUGIN_VAR_MEMALLOC) ?
                        GET_STR_ALLOC : GET_STR);
    options->def_value= (intptr_t) ((thdvar_str_t*) opt)->def_val;
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

extern "C" bool get_one_plugin_option(int optid, const struct my_option *,
                                         char *);

bool get_one_plugin_option(int optid __attribute__((unused)),
                              const struct my_option *opt __attribute__((__unused__)),
                              char *argument __attribute__((__unused__)))
{
  return 0;
}


static int construct_options(MEM_ROOT *mem_root, struct st_plugin_int *tmp,
                             my_option *options, bool can_disable)
{
  const char *plugin_name= tmp->plugin->name;
  uint namelen= strlen(plugin_name), optnamelen;
  uint buffer_length= namelen * 4 + (can_disable ? 75 : 10);
  char *name= (char*) alloc_root(mem_root, buffer_length) + 1;
  char *optname, *p;
  int index= 0, offset= 0;
  st_mysql_sys_var *opt, **plugin_option;
  st_bookmark *v;

  /* support --skip-plugin-foo syntax */
  memcpy(name, plugin_name, namelen + 1);
  my_casedn_str(&my_charset_latin1, name);
  strxmov(name + namelen + 1, "plugin-", name, NullS);
  /* Now we have namelen + 1 + 7 + namelen + 1 == namelen * 2 + 9. */

  for (p= name + namelen*2 + 8; p > name; p--)
    if (*p == '_')
      *p= '-';

  if (can_disable)
  {
    strxmov(name + namelen*2 + 10, "Enable ", plugin_name, " plugin. "
            "Disable with --skip-", name," (will save memory).", NullS);
    /*
      Now we have namelen * 2 + 10 (one char unused) + 7 + namelen + 9 +
      20 + namelen + 20 + 1 == namelen * 4 + 67.
    */

    options[0].comment= name + namelen*2 + 10;
  }

  options[1].name= (options[0].name= name) + namelen + 1;
  options[0].id= options[1].id= 256; /* must be >255. dup id ok */
  options[0].var_type= options[1].var_type= GET_BOOL;
  options[0].arg_type= options[1].arg_type= NO_ARG;
  options[0].def_value= options[1].def_value= true;
  options[0].value= options[0].u_max_value=
  options[1].value= options[1].u_max_value= (char**) (name - 1);
  options+= 2;

  /*
    Two passes as the 2nd pass will take pointer addresses for use
    by my_getopt and register_var() in the first pass uses realloc
  */

  for (plugin_option= tmp->plugin->system_vars;
       plugin_option && *plugin_option; plugin_option++, index++)
  {
    opt= *plugin_option;
    if (!(opt->flags & PLUGIN_VAR_THDLOCAL))
      continue;
    if (!(register_var(name, opt->name, opt->flags)))
      continue;
    switch (opt->flags & PLUGIN_VAR_TYPEMASK) {
    case PLUGIN_VAR_BOOL:
      (((thdvar_bool_t *)opt)->resolve)= mysql_sys_var_ptr_bool;
      break;
    case PLUGIN_VAR_INT:
      (((thdvar_int_t *)opt)->resolve)= mysql_sys_var_ptr_int;
      break;
    case PLUGIN_VAR_LONG:
      (((thdvar_long_t *)opt)->resolve)= mysql_sys_var_ptr_long;
      break;
    case PLUGIN_VAR_LONGLONG:
      (((thdvar_int64_t_t *)opt)->resolve)= mysql_sys_var_ptr_int64_t;
      break;
    case PLUGIN_VAR_STR:
      (((thdvar_str_t *)opt)->resolve)= mysql_sys_var_ptr_str;
      break;
    case PLUGIN_VAR_ENUM:
      (((thdvar_enum_t *)opt)->resolve)= mysql_sys_var_ptr_enum;
      break;
    case PLUGIN_VAR_SET:
      (((thdvar_set_t *)opt)->resolve)= mysql_sys_var_ptr_set;
      break;
    default:
      sql_print_error("Unknown variable type code 0x%x in plugin '%s'.",
                      opt->flags, plugin_name);
      return(-1);
    };
  }

  for (plugin_option= tmp->plugin->system_vars;
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
          sql_print_warning("Server variable %s of plugin %s was forced "
                            "to be read-only: string variable without "
                            "update_func and PLUGIN_VAR_MEMALLOC flag",
                            opt->name, plugin_name);
        }
      }
      break;
    case PLUGIN_VAR_ENUM:
      if (!opt->check)
        opt->check= check_func_enum;
      if (!opt->update)
        opt->update= update_func_long;
      break;
    case PLUGIN_VAR_SET:
      if (!opt->check)
        opt->check= check_func_set;
      if (!opt->update)
        opt->update= update_func_int64_t;
      break;
    default:
      sql_print_error("Unknown variable type code 0x%x in plugin '%s'.",
                      opt->flags, plugin_name);
      return(-1);
    }

    if ((opt->flags & (PLUGIN_VAR_NOCMDOPT | PLUGIN_VAR_THDLOCAL))
                    == PLUGIN_VAR_NOCMDOPT)
      continue;

    if (!opt->name)
    {
      sql_print_error("Missing variable name in plugin '%s'.",
                      plugin_name);
      return(-1);
    }

    if (!(opt->flags & PLUGIN_VAR_THDLOCAL))
    {
      optnamelen= strlen(opt->name);
      optname= (char*) alloc_root(mem_root, namelen + optnamelen + 2);
      strxmov(optname, name, "-", opt->name, NullS);
      optnamelen= namelen + optnamelen + 1;
    }
    else
    {
      /* this should not fail because register_var should create entry */
      if (!(v= find_bookmark(name, opt->name, opt->flags)))
      {
        sql_print_error("Thread local variable '%s' not allocated "
                        "in plugin '%s'.", opt->name, plugin_name);
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
    options->id= (options-1)->id + 1;

    plugin_opt_set_limits(options, opt);

    if (opt->flags & PLUGIN_VAR_THDLOCAL)
      options->value= options->u_max_value= (char**)
        (global_system_variables.dynamic_variables_ptr + offset);
    else
      options->value= options->u_max_value= *(char***) (opt + 1);

    options[1]= options[0];
    options[1].name= p= (char*) alloc_root(mem_root, optnamelen + 8);
    options[1].comment= 0; // hidden
    strxmov(p, "plugin-", optname, NullS);

    options+= 2;
  }

  return(0);
}


static my_option *construct_help_options(MEM_ROOT *mem_root,
                                         struct st_plugin_int *p)
{
  st_mysql_sys_var **opt;
  my_option *opts;
  bool can_disable;
  uint count= EXTRA_OPTIONS;

  for (opt= p->plugin->system_vars; opt && *opt; opt++, count+= 2) {};

  if (!(opts= (my_option*) alloc_root(mem_root, sizeof(my_option) * count)))
    return(NULL);

  bzero(opts, sizeof(my_option) * count);

  if ((my_strcasecmp(&my_charset_latin1, p->name.str, "MyISAM") == 0))
    can_disable= false;
  else if ((my_strcasecmp(&my_charset_latin1, p->name.str, "MEMORY") == 0))
    can_disable= false;
  else
    can_disable= true;


  if (construct_options(mem_root, p, opts, can_disable))
    return(NULL);

  return(opts);
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
static int test_plugin_options(MEM_ROOT *tmp_root, struct st_plugin_int *tmp,
                               int *argc, char **argv)
{
  struct sys_var_chain chain= { NULL, NULL };
  bool enabled_saved= true;
  bool can_disable;
  MEM_ROOT *mem_root= alloc_root_inited(&tmp->mem_root) ?
                      &tmp->mem_root : &plugin_mem_root;
  st_mysql_sys_var **opt;
  my_option *opts= NULL;
  char *p, *varname;
  int error;
  st_mysql_sys_var *o;
  sys_var *v;
  struct st_bookmark *var;
  uint len, count= EXTRA_OPTIONS;
  assert(tmp->plugin && tmp->name.str);

  for (opt= tmp->plugin->system_vars; opt && *opt; opt++)
    count+= 2; /* --{plugin}-{optname} and --plugin-{plugin}-{optname} */

  if ((my_strcasecmp(&my_charset_latin1, tmp->name.str, "MyISAM") == 0))
    can_disable= false;
  else if ((my_strcasecmp(&my_charset_latin1, tmp->name.str, "MEMORY") == 0))
    can_disable= false;
  else
    can_disable= true;

  if (count > EXTRA_OPTIONS || (*argc > 1))
  {
    if (!(opts= (my_option*) alloc_root(tmp_root, sizeof(my_option) * count)))
    {
      sql_print_error("Out of memory for plugin '%s'.", tmp->name.str);
      return(-1);
    }
    bzero(opts, sizeof(my_option) * count);

    if (construct_options(tmp_root, tmp, opts, can_disable))
    {
      sql_print_error("Bad options for plugin '%s'.", tmp->name.str);
      return(-1);
    }

    error= handle_options(argc, &argv, opts, get_one_plugin_option);
    (*argc)++; /* add back one for the program name */

    if (error)
    {
       sql_print_error("Parsing options for plugin '%s' failed.",
                       tmp->name.str);
       goto err;
    }
  }

  error= 1;

  {
    for (opt= tmp->plugin->system_vars; opt && *opt; opt++)
    {
      if (((o= *opt)->flags & PLUGIN_VAR_NOSYSVAR))
        continue;

      if ((var= find_bookmark(tmp->name.str, o->name, o->flags)))
        v= new (mem_root) sys_var_pluginvar(var->key + 1, o);
      else
      {
        len= tmp->name.length + strlen(o->name) + 2;
        varname= (char*) alloc_root(mem_root, len);
        strxmov(varname, tmp->name.str, "-", o->name, NullS);
        my_casedn_str(&my_charset_latin1, varname);

        for (p= varname; *p; p++)
          if (*p == '-')
            *p= '_';

        v= new (mem_root) sys_var_pluginvar(varname, o);
      }
      assert(v); /* check that an object was actually constructed */

      /*
        Add to the chain of variables.
        Done like this for easier debugging so that the
        pointer to v is not lost on optimized builds.
      */
      v->chain_sys_var(&chain);
    }
    if (chain.first)
    {
      chain.last->next = NULL;
      if (mysql_add_sys_var_chain(chain.first, NULL))
      {
        sql_print_error("Plugin '%s' has conflicting system variables",
                        tmp->name.str);
        goto err;
      }
      tmp->system_vars= chain.first;
    }
    return(0);
  }

  if (enabled_saved && global_system_variables.log_warnings)
    sql_print_information("Plugin '%s' disabled by command line option",
                          tmp->name.str);
err:
  if (opts)
    my_cleanup_options(opts);
  return(error);
}


/****************************************************************************
  Help Verbose text with Plugin System Variables
****************************************************************************/

static int option_cmp(my_option *a, my_option *b)
{
  return my_strcasecmp(&my_charset_latin1, a->name, b->name);
}


void my_print_help_inc_plugins(my_option *main_options, uint size)
{
  DYNAMIC_ARRAY all_options;
  struct st_plugin_int *p;
  MEM_ROOT mem_root;
  my_option *opt;

  init_alloc_root(&mem_root, 4096, 4096);
  my_init_dynamic_array(&all_options, sizeof(my_option), size, size/4);

  if (initialized)
    for (uint idx= 0; idx < plugin_array.elements; idx++)
    {
      p= *dynamic_element(&plugin_array, idx, struct st_plugin_int **);

      if (!p->plugin->system_vars ||
          !(opt= construct_help_options(&mem_root, p)))
        continue;

      /* Only options with a non-NULL comment are displayed in help text */
      for (;opt->id; opt++)
        if (opt->comment)
          insert_dynamic(&all_options, (uchar*) opt);
    }

  for (;main_options->id; main_options++)
    insert_dynamic(&all_options, (uchar*) main_options);

  sort_dynamic(&all_options, (qsort_cmp) option_cmp);

  /* main_options now points to the empty option terminator */
  insert_dynamic(&all_options, (uchar*) main_options);

  my_print_help((my_option*) all_options.buffer);
  my_print_variables((my_option*) all_options.buffer);

  delete_dynamic(&all_options);
  free_root(&mem_root, MYF(0));
}

