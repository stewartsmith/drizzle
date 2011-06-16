/* -*- mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; -*-
 *  vim:expandtab:shiftwidth=2:tabstop=2:smarttab:
 *
 *  Copyright (C) 2008 Sun Microsystems, Inc.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; version 2 of the License.
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

#pragma once

#include <string>
#include <boost/filesystem.hpp>

#include <drizzled/common_fwd.h>
#include <drizzled/constrained_value.h>
#include <drizzled/set_var.h>
#include <drizzled/show_type.h>
#include <drizzled/item_result.h>
#include <drizzled/base.h>
#include <drizzled/charset.h>
#include <drizzled/lex_string.h>
#include <drizzled/visibility.h>

namespace drizzled {

typedef int (*sys_check_func)(Session *,  set_var *);
typedef bool (*sys_update_func)(Session *, set_var *);
typedef void (*sys_after_update_func)(Session *, sql_var_t);
typedef void (*sys_set_default_func)(Session *, sql_var_t);
typedef unsigned char *(*sys_value_ptr_func)(Session *session);

static const std::vector<std::string> empty_aliases;
extern struct drizzle_system_variables max_system_variables;
extern size_t table_def_size;

extern std::string drizzle_tmpdir;
extern const char *first_keyword;
extern const char *in_left_expr_name;
extern const char *in_additional_cond;
extern const char *in_having_cond;
extern boost::filesystem::path basedir;
extern boost::filesystem::path pid_file;
extern boost::filesystem::path secure_file_priv;
extern const char *opt_tc_log_file;
extern uint64_t session_startup_options;
extern uint32_t global_thread_id;
extern uint64_t table_cache_size;
extern back_log_constraints back_log;
extern uint32_t ha_open_options;
extern const char *drizzled_bind_host;
extern uint32_t dropping_tables;
extern bool opt_endinfo;
extern uint32_t volatile thread_running;
extern uint32_t volatile global_read_lock;
extern bool opt_readonly;
extern const char *opt_scheduler;
extern size_t transaction_message_threshold;

uint64_t fix_unsigned(Session*, uint64_t, const option&);

DRIZZLED_API const std::string &getServerHostname();
int sys_var_init();

/**
 * A class which represents a variable, either global or 
 * session-local.
 */
class DRIZZLED_API sys_var
{
protected:
  std::string name; /**< The name of the variable */
  sys_check_func check_func;
  sys_after_update_func after_update; /**< Function pointer triggered after the variable's value is updated */
  struct option *option_limits; /**< Updated by by sys_var_init() */
  bool m_allow_empty_value; /**< Does variable allow an empty value? */
public:
  sys_var(const std::string &name_arg,
          sys_after_update_func func= NULL,
          sys_check_func check_func_arg= NULL)
    :
    name(name_arg),
    check_func(check_func_arg),
    after_update(func),
    option_limits(NULL),
    m_allow_empty_value(true)
  {}
  virtual ~sys_var() {}

  void setName(const std::string &name_in)
  {
    name= name_in;
  }

  /** 
   * Returns the name of the variable.
   *
   * @note 
   *
   * So that we can exist in a Registry. We really need to formalize that 
   */
  inline const std::string &getName() const
  {
    return name;
  }
  /**
   * Returns a vector of strings representing aliases
   * for this variable's name.
   */
  const std::vector<std::string>& getAliases() const
  {
    return empty_aliases;
  }
  /**
   * Returns a pointer to the variable's option limits
   */
  inline struct option *getOptionLimits() const
  {
    return option_limits;
  }
  /**
   * Sets the pointer to the variable's option limits
   *
   * @param Pointer to the option limits option variable
   */
  inline void setOptionLimits(struct option *in_option_limits)
  {
    option_limits= in_option_limits;
  }
  /** 
   * Returns the function pointer for after update trigger, or NULL if none.
   */
  inline sys_after_update_func getAfterUpdateTrigger() const
  {
    return after_update;
  }
  virtual bool check(Session *session, set_var *var);
  bool check_enum(Session *session, set_var *var, const TYPELIB *enum_names);
  virtual bool update(Session *session, set_var *var)=0;
  virtual void set_default(Session *, sql_var_t)
  {}
  virtual SHOW_TYPE show_type()
  {
    return SHOW_UNDEF;
  }
  virtual unsigned char *value_ptr(Session *, sql_var_t, const LEX_STRING *)
  {
    return 0;
  }
  virtual bool check_type(sql_var_t type)
  {
    return type != OPT_GLOBAL;
  }		/* Error if not GLOBAL */
  virtual bool check_update_type(Item_result type)
  {
    return type != INT_RESULT;
  }		/* Assume INT */
  virtual bool check_default(sql_var_t)
  {
    return option_limits == 0;
  }
  Item *item(Session *session, sql_var_t type, const LEX_STRING *base);
  virtual bool is_readonly() const
  {
    return 0;
  }
};

/**
 * A base class for all variables that require its access to
 * be guarded with a mutex.
 */
class DRIZZLED_API sys_var_global: public sys_var
{
protected:
  pthread_mutex_t *guard;
public:
  sys_var_global(const char *name_arg,
                 sys_after_update_func after_update_arg,
                 pthread_mutex_t *guard_arg)
    :
      sys_var(name_arg, after_update_arg), 
      guard(guard_arg) 
  {}
};

class DRIZZLED_API sys_var_uint32_t_ptr :public sys_var
{
  uint32_t *value;
public:
  sys_var_uint32_t_ptr(const char *name_arg,
                       uint32_t *value_ptr_arg)
    :sys_var(name_arg),value(value_ptr_arg)
  {  }
  sys_var_uint32_t_ptr(const char *name_arg,
                       uint32_t *value_ptr_arg,
                       sys_after_update_func func)
    :sys_var(name_arg,func), value(value_ptr_arg)
  {  }
  bool check(Session *session, set_var *var);
  bool update(Session *session, set_var *var);
  void set_default(Session *session, sql_var_t type);
  SHOW_TYPE show_type() { return SHOW_INT; }
  unsigned char *value_ptr(Session *, sql_var_t, const LEX_STRING *)
  { return (unsigned char*) value; }
};

class DRIZZLED_API sys_var_uint32_t_ptr_readonly :
  public sys_var_uint32_t_ptr
{
public:
  sys_var_uint32_t_ptr_readonly(const char *name_arg,
                                uint32_t *value_ptr_arg) :
    sys_var_uint32_t_ptr(name_arg, value_ptr_arg)
  {}

  sys_var_uint32_t_ptr_readonly(const char *name_arg,
                                uint32_t *value_ptr_arg,
                                sys_after_update_func func) :
    sys_var_uint32_t_ptr(name_arg, value_ptr_arg, func)
  {}

  bool is_readonly() const
  {
    return true;
  }
};


class DRIZZLED_API sys_var_uint64_t_ptr :public sys_var
{
  uint64_t *value;
  const uint64_t default_value;
  bool have_default_value;
public:
  sys_var_uint64_t_ptr(const char *name_arg, uint64_t *value_ptr_arg) :
    sys_var(name_arg),
    value(value_ptr_arg),
    default_value(0),
    have_default_value(false)
  {  }

  sys_var_uint64_t_ptr(const char *name_arg,
                       uint64_t *value_ptr_arg,
                       const uint64_t default_value_in) :
    sys_var(name_arg),
    value(value_ptr_arg),
    default_value(default_value_in),
    have_default_value(true)
  {  }

  sys_var_uint64_t_ptr(const char *name_arg,
                       uint64_t *value_ptr_arg,
                       sys_after_update_func func) :
    sys_var(name_arg,func),
    value(value_ptr_arg),
    default_value(0),
    have_default_value(false)
  {  }

  sys_var_uint64_t_ptr(const char *name_arg,
                       uint64_t *value_ptr_arg,
                       sys_after_update_func func,
                       const uint64_t default_value_in) :
    sys_var(name_arg,func),
    value(value_ptr_arg),
    default_value(default_value_in),
    have_default_value(true)
  {  }

  bool update(Session *session, set_var *var);
  void set_default(Session *session, sql_var_t type);
  virtual bool check_default(sql_var_t)
  {
    return (not have_default_value) && option_limits == 0;
  }
  SHOW_TYPE show_type() { return SHOW_LONGLONG; }
  unsigned char *value_ptr(Session *, sql_var_t,
                           const LEX_STRING *)
  { return (unsigned char*) value; }
};

class DRIZZLED_API sys_var_size_t_ptr :public sys_var
{
  size_t *value;
public:
  sys_var_size_t_ptr(const char *name_arg, size_t *value_ptr_arg)
    :sys_var(name_arg),value(value_ptr_arg)
  {  }
  sys_var_size_t_ptr(const char *name_arg, size_t *value_ptr_arg,
                     sys_after_update_func func)
    :sys_var(name_arg,func), value(value_ptr_arg)
  {  }
  bool update(Session *session, set_var *var);
  void set_default(Session *session, sql_var_t type);
  SHOW_TYPE show_type() { return SHOW_SIZE; }
  unsigned char *value_ptr(Session *, sql_var_t, const LEX_STRING *)
  { return (unsigned char*) value; }
};

class DRIZZLED_API sys_var_size_t_ptr_readonly :public sys_var_size_t_ptr
{
public:
  sys_var_size_t_ptr_readonly(const char *name_arg,
                            size_t *value_arg)
    :sys_var_size_t_ptr(name_arg, value_arg)
  {}
  bool is_readonly() const { return 1; }
};

class DRIZZLED_API sys_var_bool_ptr :public sys_var
{
  bool default_value;
public:
  bool *value;
  sys_var_bool_ptr(const std::string &name_arg, bool *value_arg,
                   sys_after_update_func func= NULL) :
    sys_var(name_arg, func), default_value(*value_arg), value(value_arg)
  { }
  bool check(Session *session, set_var *var);
  virtual bool check_default(sql_var_t)
  {
    return false;
  }
  bool update(Session *session, set_var *var);
  void set_default(Session *session, sql_var_t type);
  SHOW_TYPE show_type() { return SHOW_MY_BOOL; }
  unsigned char *value_ptr(Session *, sql_var_t, const LEX_STRING *)
  { return (unsigned char*) value; }
  bool check_update_type(Item_result)
  { return 0; }
};

class DRIZZLED_API sys_var_bool_ptr_readonly :public sys_var_bool_ptr
{
public:
  sys_var_bool_ptr_readonly(const char *name_arg,
                            bool *value_arg)
    :sys_var_bool_ptr(name_arg, value_arg)
  {}
  bool is_readonly() const { return 1; }
};


class DRIZZLED_API sys_var_str :public sys_var
{
public:
  char *value;					// Pointer to allocated string
  uint32_t value_length;
  sys_update_func update_func;
  sys_set_default_func set_default_func;
  sys_var_str(const char *name_arg,
              sys_check_func check_func_arg,
              sys_update_func update_func_arg,
              sys_set_default_func set_default_func_arg,
              char *value_arg) :
    sys_var(name_arg, NULL, check_func_arg),
    value(value_arg),
    update_func(update_func_arg),
    set_default_func(set_default_func_arg)
  {  }
  bool check(Session *session, set_var *var);
  bool update(Session *session, set_var *var)
  {
    return (*update_func)(session, var);
  }
  void set_default(Session *session, sql_var_t type)
  {
    (*set_default_func)(session, type);
  }
  SHOW_TYPE show_type() { return SHOW_CHAR; }
  unsigned char *value_ptr(Session *, sql_var_t, const LEX_STRING *)
  { return (unsigned char*) value; }
  bool check_update_type(Item_result type)
  {
    return type != STRING_RESULT;		/* Only accept strings */
  }
  bool check_default(sql_var_t)
  { return 0; }
};


class DRIZZLED_API sys_var_fs_path :
  public sys_var
{
  const boost::filesystem::path &value;
public:
  sys_var_fs_path(const char *name_arg,
                  const boost::filesystem::path& value_arg) :
    sys_var(name_arg),
    value(value_arg)
  { }

  inline void set(char *)
  { }

  bool check(Session *, set_var *)
  {
    return true;
  }
  bool update(Session *, set_var *)
  {
    return true;
  }
  SHOW_TYPE show_type() { return SHOW_CHAR; }
  unsigned char *value_ptr(Session *, sql_var_t, const LEX_STRING *)
  {
    return (unsigned char*)(value.file_string().c_str());
  }
  bool check_update_type(Item_result)
  {
    return true;
  }
  bool check_default(sql_var_t) { return true; }
  bool is_readonly() const { return true; }
};

template<class T>
class sys_var_constrained_value :
  public sys_var
{
  constrained_value<T> &value;
  T basic_value;
  T default_value;
public:
  sys_var_constrained_value(const char *name_arg,
                            constrained_value<T> &value_arg) :
    sys_var(name_arg),
    value(value_arg),
    default_value(value_arg.get())
  { }

  sys_var_constrained_value(const char *name_arg,
                            constrained_value<T> &value_arg,
                            sys_after_update_func after_update_func_arg) :
    sys_var(name_arg, after_update_func_arg),
    value(value_arg),
    default_value(value_arg.get())
  { }

  sys_var_constrained_value(const char *name_arg,
                            constrained_value<T> &value_arg,
                            sys_check_func check_func_arg) :
    sys_var(name_arg, NULL, check_func_arg),
    value(value_arg),
    default_value(value_arg.get())
  { }

public:
  bool is_readonly() const
  {
    return false;
  }

  SHOW_TYPE show_type() { return SHOW_INT; }

  bool update(Session *, set_var *var)
  {
    value= uint32_t(var->getInteger());
    return false;
  }

  bool check_default(sql_var_t)
  {
    return false;
  }

  void set_default(Session *, sql_var_t)
  {
    value= default_value;
  }

  unsigned char *value_ptr(Session *, sql_var_t, const LEX_STRING *)
  {
    basic_value= value.get();
    return (unsigned char*)&basic_value;
  }
};

template<>
inline SHOW_TYPE sys_var_constrained_value<uint64_t>::show_type()
{
  return SHOW_LONGLONG;
}

template<>
inline SHOW_TYPE sys_var_constrained_value<int64_t>::show_type()
{
  return SHOW_LONGLONG;
}

template<>
inline SHOW_TYPE sys_var_constrained_value<uint32_t>::show_type()
{
  return SHOW_INT;
}

template<>
inline SHOW_TYPE sys_var_constrained_value<int32_t>::show_type()
{
  return SHOW_LONG;
}

template<>
inline bool sys_var_constrained_value<uint64_t>::update(Session *, set_var *var)
{
  value= var->getInteger();
  return false;
}

template<>
inline bool sys_var_constrained_value<uint32_t>::update(Session *, set_var *var)
{
  value= uint32_t(var->getInteger());
  return false;
}

template<class T>
class sys_var_constrained_value_readonly :
  public sys_var_constrained_value<T>
{
public:
  sys_var_constrained_value_readonly(const char *name_arg,
                                     constrained_value<T> &value_arg) :
    sys_var_constrained_value<T>(name_arg, value_arg)
  { }

  sys_var_constrained_value_readonly(const char *name_arg,
                                     constrained_value<T> &value_arg,
                                     T default_value_arg) :
    sys_var_constrained_value<T>(name_arg, value_arg, default_value_arg)
  { }

public:
  bool is_readonly() const
  {
    return true;
  }
};

class DRIZZLED_API sys_var_std_string :
  public sys_var
{
  std::string &value;
  sys_check_func check_func;
  sys_update_func update_func;
  sys_set_default_func set_default_func;
public:
  sys_var_std_string(const std::string &name_arg,
                     std::string &value_arg,
                     sys_check_func check_func_arg= NULL,
                     sys_update_func update_func_arg= NULL) :
    sys_var(name_arg),
    value(value_arg),
    check_func(check_func_arg),
    update_func(update_func_arg)
  {  }

  inline void set(char *val_in)
  {
    value= val_in; 
  }

  void set_check_func(sys_check_func check_func_arg= NULL)
  {
    check_func= check_func_arg;
  }

  void set_update_func(sys_update_func update_func_arg= NULL)
  {
    update_func= update_func_arg;
  }

  bool check(Session *session, set_var *var);
    
  bool update(Session *session, set_var *var)
  {
    if (update_func != NULL)
    {
      return (*update_func)(session, var);
    }
    return false;
  }
  SHOW_TYPE show_type() { return SHOW_CHAR; }
  unsigned char *value_ptr(Session *, sql_var_t, const LEX_STRING *)
  {
    return (unsigned char*)(value.c_str());
  }
  bool check_update_type(Item_result type)
  {
    return type != STRING_RESULT;		/* Only accept strings */
  }
  bool check_default(sql_var_t)
  { return true; }
  bool is_readonly() const { return false; }
};

class DRIZZLED_API sys_var_const_string :
  public sys_var
{
  const std::string &value;
public:
  sys_var_const_string(const char *name_arg,
                       const std::string& value_arg) :
    sys_var(name_arg),
    value(value_arg)
  { }

  inline void set(char *)
  { }

  bool check(Session *, set_var *)
  {
    return true;
  }
  bool update(Session *, set_var *)
  {
    return true;
  }
  SHOW_TYPE show_type() { return SHOW_CHAR; }
  unsigned char *value_ptr(Session *, sql_var_t, const LEX_STRING *)
  {
    return (unsigned char*)(value.c_str());
  }
  bool check_update_type(Item_result)
  {
    return true;
  }
  bool check_default(sql_var_t) { return true; }
  bool is_readonly() const { return true; }
};

class DRIZZLED_API sys_var_const_string_val :
  public sys_var
{
  const std::string value;
public:
  sys_var_const_string_val(const char *name_arg,
                           const std::string& value_arg) :
    sys_var(name_arg),
    value(value_arg)
  { }

  inline void set(char *)
  { }

  bool check(Session *, set_var *)
  {
    return true;
  }
  bool update(Session *, set_var *)
  {
    return true;
  }
  SHOW_TYPE show_type() { return SHOW_CHAR; }
  unsigned char *value_ptr(Session *, sql_var_t, const LEX_STRING *)
  {
    return (unsigned char*)(value.c_str());
  }
  bool check_update_type(Item_result)
  {
    return true;
  }
  bool check_default(sql_var_t) { return true; }
  bool is_readonly() const { return true; }
};

class DRIZZLED_API sys_var_const_str :public sys_var
{
  char *value;					// Pointer to const value
public:
  sys_var_const_str(const char *name_arg,
                    const char *value_arg)
    :sys_var(name_arg), value((char*) value_arg)
  {  }
  inline void set (char *new_value)
  {
    value= new_value;
  }
  bool check(Session *, set_var *)
  {
    return 1;
  }
  bool update(Session *, set_var *)
  {
    return 1;
  }
  SHOW_TYPE show_type() { return SHOW_CHAR; }
  unsigned char *value_ptr(Session *, sql_var_t, const LEX_STRING *)
  {
    return (unsigned char*) value;
  }
  bool check_update_type(Item_result)
  {
    return 1;
  }
  bool check_default(sql_var_t)
  { return 1; }
  bool is_readonly() const { return 1; }
};


class DRIZZLED_API sys_var_const_str_ptr :public sys_var
{
  char **value;					// Pointer to const value
public:
  sys_var_const_str_ptr(const char *name_arg, char **value_arg)
    :sys_var(name_arg),value(value_arg)
  {  }
  bool check(Session *, set_var *)
  {
    return 1;
  }
  bool update(Session *, set_var *)
  {
    return 1;
  }
  SHOW_TYPE show_type() { return SHOW_CHAR; }
  unsigned char *value_ptr(Session *, sql_var_t, const LEX_STRING *)
  {
    return (unsigned char*) *value;
  }
  bool check_update_type(Item_result)
  {
    return 1;
  }
  bool check_default(sql_var_t)
  { return 1; }
  bool is_readonly(void) const { return 1; }
};


class DRIZZLED_API sys_var_session :public sys_var
{
public:
  sys_var_session(const char *name_arg,
              sys_after_update_func func= NULL)
    :sys_var(name_arg, func)
  {}
  bool check_type(sql_var_t)
  { return 0; }
  bool check_default(sql_var_t type)
  {
    return type == OPT_GLOBAL && !option_limits;
  }
};

class DRIZZLED_API sys_var_session_uint32_t :public sys_var_session
{
  sys_check_func check_func;
public:
  uint32_t drizzle_system_variables::*offset;
  sys_var_session_uint32_t(const char *name_arg,
                           uint32_t drizzle_system_variables::*offset_arg,
                           sys_check_func c_func= NULL,
                           sys_after_update_func au_func= NULL)
    :sys_var_session(name_arg, au_func), check_func(c_func),
    offset(offset_arg)
  {  }
  bool check(Session *session, set_var *var);
  bool update(Session *session, set_var *var);
  void set_default(Session *session, sql_var_t type);
  SHOW_TYPE show_type() { return SHOW_INT; }
  unsigned char *value_ptr(Session *session, sql_var_t type,
                           const LEX_STRING *base);
};


class DRIZZLED_API sys_var_session_ha_rows :public sys_var_session
{
public:
  ha_rows drizzle_system_variables::*offset;
  sys_var_session_ha_rows(const char *name_arg,
                      ha_rows drizzle_system_variables::*offset_arg)
    :sys_var_session(name_arg), offset(offset_arg)
  {  }
  sys_var_session_ha_rows(const char *name_arg,
                      ha_rows drizzle_system_variables::*offset_arg,
		      sys_after_update_func func)
    :sys_var_session(name_arg,func), offset(offset_arg)
  {  }
  bool update(Session *session, set_var *var);
  void set_default(Session *session, sql_var_t type);
  SHOW_TYPE show_type() { return SHOW_HA_ROWS; }
  unsigned char *value_ptr(Session *session, sql_var_t type,
                           const LEX_STRING *base);
};


class DRIZZLED_API sys_var_session_uint64_t :public sys_var_session
{
  sys_check_func check_func;
public:
  uint64_t drizzle_system_variables::*offset;
  bool only_global;
  sys_var_session_uint64_t(
                           const char *name_arg,
                           uint64_t drizzle_system_variables::*offset_arg,
                           sys_after_update_func au_func= NULL,
                           sys_check_func c_func= NULL)
    :sys_var_session(name_arg, au_func),
    check_func(c_func),
    offset(offset_arg)
  {  }
  sys_var_session_uint64_t(const char *name_arg,
                           uint64_t drizzle_system_variables::*offset_arg,
                           sys_after_update_func func,
                           bool only_global_arg,
                           sys_check_func cfunc= NULL)
    :sys_var_session(name_arg, func),
    check_func(cfunc),
    offset(offset_arg),
    only_global(only_global_arg)
  {  }
  bool update(Session *session, set_var *var);
  void set_default(Session *session, sql_var_t type);
  SHOW_TYPE show_type() { return SHOW_LONGLONG; }
  unsigned char *value_ptr(Session *session, sql_var_t type,
                           const LEX_STRING *base);
  bool check(Session *session, set_var *var);
  bool check_default(sql_var_t type)
  {
    return type == OPT_GLOBAL && !option_limits;
  }
  bool check_type(sql_var_t type)
  {
    return (only_global && type != OPT_GLOBAL);
  }
};

class DRIZZLED_API sys_var_session_size_t :public sys_var_session
{
  sys_check_func check_func;
public:
  size_t drizzle_system_variables::*offset;
  bool only_global;
  sys_var_session_size_t(const char *name_arg,
                         size_t drizzle_system_variables::*offset_arg,
                         sys_after_update_func au_func= NULL,
                         sys_check_func c_func= NULL)
    :sys_var_session(name_arg, au_func),
     check_func(c_func),
     offset(offset_arg)
  {  }
  sys_var_session_size_t(const char *name_arg,
                         size_t drizzle_system_variables::*offset_arg,
                         sys_after_update_func func,
                         bool only_global_arg,
                         sys_check_func cfunc= NULL)
    :sys_var_session(name_arg, func),
     check_func(cfunc),
     offset(offset_arg),
     only_global(only_global_arg)
  {  }
  bool update(Session *session, set_var *var);
  void set_default(Session *session, sql_var_t type);
  SHOW_TYPE show_type() { return SHOW_SIZE; }
  unsigned char *value_ptr(Session *session, sql_var_t type,
                           const LEX_STRING *base);
  bool check(Session *session, set_var *var);
  bool check_default(sql_var_t type)
  {
    return type == OPT_GLOBAL && !option_limits;
  }
  bool check_type(sql_var_t type)
  {
    return (only_global && type != OPT_GLOBAL);
  }
};


class DRIZZLED_API sys_var_session_bool :public sys_var_session
{
public:
  bool drizzle_system_variables::*offset;
  sys_var_session_bool(const char *name_arg, bool drizzle_system_variables::*offset_arg)
    :sys_var_session(name_arg), offset(offset_arg)
  {  }
  sys_var_session_bool(const char *name_arg, bool drizzle_system_variables::*offset_arg,
		   sys_after_update_func func)
    :sys_var_session(name_arg,func), offset(offset_arg)
  {  }
  bool update(Session *session, set_var *var);
  void set_default(Session *session, sql_var_t type);
  SHOW_TYPE show_type() { return SHOW_MY_BOOL; }
  unsigned char *value_ptr(Session *session, sql_var_t type,
                           const LEX_STRING *base);
  bool check(Session *session, set_var *var);
  bool check_update_type(Item_result)
  { return 0; }
};


class DRIZZLED_API sys_var_session_enum :public sys_var_session
{
protected:
  uint32_t drizzle_system_variables::*offset;
  TYPELIB *enum_names;
  sys_check_func check_func;
public:
  sys_var_session_enum(const char *name_arg,
                   uint32_t drizzle_system_variables::*offset_arg, TYPELIB *typelib,
                   sys_after_update_func func= NULL,
                   sys_check_func check_f= NULL)
    :sys_var_session(name_arg, func), offset(offset_arg),
    enum_names(typelib), check_func(check_f)
  {  }
  bool check(Session *session, set_var *var)
  {
    int ret= 0;
    if (check_func)
      ret= (*check_func)(session, var);
    return ret ? ret : check_enum(session, var, enum_names);
  }
  bool update(Session *session, set_var *var);
  void set_default(Session *session, sql_var_t type);
  SHOW_TYPE show_type() { return SHOW_CHAR; }
  unsigned char *value_ptr(Session *session, sql_var_t type,
                           const LEX_STRING *base);
  bool check_update_type(Item_result)
  { return 0; }
};


class DRIZZLED_API sys_var_session_storage_engine :public sys_var_session
{
protected:
  plugin::StorageEngine *drizzle_system_variables::*offset;
public:
  sys_var_session_storage_engine(const char *name_arg,
                                 plugin::StorageEngine *drizzle_system_variables::*offset_arg)
    :sys_var_session(name_arg), offset(offset_arg)
  {  }
  SHOW_TYPE show_type() { return SHOW_CHAR; }
  bool check_update_type(Item_result type)
  {
    return type != STRING_RESULT;		/* Only accept strings */
  }
  void set_default(Session *session, sql_var_t type);
  bool update(Session *session, set_var *var);
  unsigned char *value_ptr(Session *session, sql_var_t type,
                           const LEX_STRING *base);
};

class DRIZZLED_API sys_var_session_bit :public sys_var_session
{
  sys_check_func check_func;
  sys_update_func update_func;
public:
  uint64_t bit_flag;
  bool reverse;
  sys_var_session_bit(const char *name_arg,
                  sys_check_func c_func, sys_update_func u_func,
                  uint64_t bit, bool reverse_arg=0)
    :sys_var_session(name_arg, NULL), check_func(c_func),
    update_func(u_func), bit_flag(bit), reverse(reverse_arg)
  {  }
  bool check(Session *session, set_var *var);
  bool update(Session *session, set_var *var);
  bool check_update_type(Item_result)
  { return 0; }
  bool check_type(sql_var_t type) { return type == OPT_GLOBAL; }
  SHOW_TYPE show_type() { return SHOW_MY_BOOL; }
  unsigned char *value_ptr(Session *session, sql_var_t type,
                           const LEX_STRING *base);
};

/* some variables that require special handling */

class DRIZZLED_API sys_var_timestamp :public sys_var
{
public:
  sys_var_timestamp(const char *name_arg)
    :sys_var(name_arg, NULL)
  {  }
  bool update(Session *session, set_var *var);
  void set_default(Session *session, sql_var_t type);
  bool check_type(sql_var_t type)    { return type == OPT_GLOBAL; }
  bool check_default(sql_var_t)
  { return 0; }
  SHOW_TYPE show_type(void) { return SHOW_LONG; }
  unsigned char *value_ptr(Session *session, sql_var_t type,
                           const LEX_STRING *base);
};


class DRIZZLED_API sys_var_last_insert_id :public sys_var
{
public:
  sys_var_last_insert_id(const char *name_arg)
    :sys_var(name_arg, NULL)
  {  }
  bool update(Session *session, set_var *var);
  bool check_type(sql_var_t type) { return type == OPT_GLOBAL; }
  SHOW_TYPE show_type() { return SHOW_LONGLONG; }
  unsigned char *value_ptr(Session *session, sql_var_t type,
                           const LEX_STRING *base);
};


class DRIZZLED_API sys_var_collation :public sys_var_session
{
public:
  sys_var_collation(const char *name_arg)
    :sys_var_session(name_arg, NULL)
  { }
  SHOW_TYPE show_type() { return SHOW_CHAR; }
  bool check_update_type(Item_result type)
  {
    return ((type != STRING_RESULT) && (type != INT_RESULT));
  }
  bool check_default(sql_var_t) { return 0; }
  virtual void set_default(Session *session, sql_var_t type)= 0;
};

class DRIZZLED_API sys_var_collation_sv :public sys_var_collation
{
  const charset_info_st *drizzle_system_variables::*offset;
  const charset_info_st **global_default;
public:
  sys_var_collation_sv(const char *name_arg,
                       const charset_info_st *drizzle_system_variables::*offset_arg,
                       const charset_info_st **global_default_arg)
    :sys_var_collation(name_arg),
    offset(offset_arg), global_default(global_default_arg)
  {
    
  }
  bool update(Session *session, set_var *var);
  void set_default(Session *session, sql_var_t type);
  unsigned char *value_ptr(Session *session, sql_var_t type,
                           const LEX_STRING *base);
};

/* Variable that you can only read from */

class DRIZZLED_API sys_var_readonly: public sys_var
{
public:
  sql_var_t var_type;
  SHOW_TYPE show_type_value;
  sys_value_ptr_func value_ptr_func;
  sys_var_readonly(const char *name_arg, sql_var_t type,
		   SHOW_TYPE show_type_arg,
		   sys_value_ptr_func value_ptr_func_arg)
    :sys_var(name_arg), var_type(type),
       show_type_value(show_type_arg), value_ptr_func(value_ptr_func_arg)
  {  }
  bool update(Session *, set_var *)
  { return 1; }
  bool check_default(sql_var_t)
  { return 1; }
  bool check_type(sql_var_t type) { return type != var_type; }
  bool check_update_type(Item_result)
  { return 1; }
  unsigned char *value_ptr(Session *session, sql_var_t,
                           const LEX_STRING *)
  {
    return (*value_ptr_func)(session);
  }
  SHOW_TYPE show_type(void) { return show_type_value; }
  bool is_readonly(void) const { return 1; }
};

class DRIZZLED_API sys_var_microseconds :public sys_var_session
{
  uint64_t drizzle_system_variables::*offset;
public:
  sys_var_microseconds(const char *name_arg,
                       uint64_t drizzle_system_variables::*offset_arg):
    sys_var_session(name_arg), offset(offset_arg)
  {  }
  bool check(Session *, set_var *) {return 0;}
  bool update(Session *session, set_var *var);
  void set_default(Session *session, sql_var_t type);
  SHOW_TYPE show_type() { return SHOW_DOUBLE; }
  bool check_update_type(Item_result type)
  {
    return (type != INT_RESULT && type != REAL_RESULT && type != DECIMAL_RESULT);
  }
};

class DRIZZLED_API sys_var_session_lc_time_names :public sys_var_session
{
public:
  sys_var_session_lc_time_names(const char *name_arg)
    : sys_var_session(name_arg, NULL)
  {
    
  }
  SHOW_TYPE show_type() { return SHOW_CHAR; }
  bool check_update_type(Item_result type)
  {
    return ((type != STRING_RESULT) && (type != INT_RESULT));
  }
  bool check_default(sql_var_t)
  { return 0; }
  bool update(Session *session, set_var *var);
  unsigned char *value_ptr(Session *session, sql_var_t type,
                           const LEX_STRING *base);
  virtual void set_default(Session *session, sql_var_t type);
};



/* For sql_yacc */
struct sys_var_with_base
{
  sys_var *var;
  LEX_STRING base_name;
};

/*
  Prototypes for helper functions
*/

drizzle_show_var* enumerate_sys_vars(Session *session);
void add_sys_var_to_list(sys_var *var, struct option *long_options);
void add_sys_var_to_list(sys_var *var);
sys_var *find_sys_var(const std::string &name);
extern sys_var_session_bit sys_autocommit;
const charset_info_st *get_old_charset_by_name(const char *old_name);

extern sys_var_str sys_var_general_log_path, sys_var_slow_log_path;

} /* namespace drizzled */

