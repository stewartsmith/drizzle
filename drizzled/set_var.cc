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

#include <config.h>

#include <boost/foreach.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/exception/get_error_info.hpp>
#include <string>

#include <drizzled/session.h>
#include <drizzled/item/string.h>
#include <drizzled/function/set_user_var.h>
#include <drizzled/sql_lex.h>
#include <drizzled/util/test.h>

using namespace std;

namespace drizzled
{

/**
  Execute update of all variables.

  First run a check of all variables that all updates will go ok.
  If yes, then execute all updates, returning an error if any one failed.

  This should ensure that in all normal cases none all or variables are
  updated.

  @param Session		Thread id
  @param var_list       List of variables to update

  @retval
    0	ok
  @retval
    1	ERROR, message sent (normally no variables was updated)
  @retval
    -1  ERROR, message not sent
*/

int sql_set_variables(Session *session, const SetVarVector &var_list)
{
  int error;
  BOOST_FOREACH(SetVarVector::const_reference it, var_list)
  {
    if ((error= it->check(session)))
      goto err;
  }
  if (!(error= test(session->is_error())))
  {
    BOOST_FOREACH(SetVarVector::const_reference it, var_list)
    {
      error|= it->update(session);         // Returns 0, -1 or 1
    }
  }
err:
  free_underlaid_joins(session, &session->lex().select_lex);
  return error;
}


/*****************************************************************************
  Functions to handle SET mysql_internal_variable=const_expr
*****************************************************************************/
set_var::set_var(sql_var_t type_arg, sys_var *var_arg,
                 const LEX_STRING *base_name_arg, Item *value_arg) :
  uint64_t_value(0),
  str_value(""),
  var(var_arg),
  type(type_arg),
  base(*base_name_arg)
{
  /*
    If the set value is a field, change it to a string to allow things like
    SET table_type=MYISAM;
  */
  if (value_arg && value_arg->type() == Item::FIELD_ITEM)
  {
    Item_field *item= (Item_field*) value_arg;
    value=new Item_string(item->field_name, (uint32_t) strlen(item->field_name), item->collation.collation);
  }
  else
  {
    value= value_arg;
  }
}

int set_var::check(Session *session)
{
  if (var->is_readonly())
  {
    my_error(ER_INCORRECT_GLOBAL_LOCAL_VAR, MYF(0), var->getName().c_str(), "read only");
    return -1;
  }
  if (var->check_type(type))
  {
    int err= type == OPT_GLOBAL ? ER_LOCAL_VARIABLE : ER_GLOBAL_VARIABLE;
    my_error(static_cast<drizzled::error_t>(err), MYF(0), var->getName().c_str());
    return -1;
  }
  /* value is a NULL pointer if we are using SET ... = DEFAULT */
  if (not value)
  {
    if (var->check_default(type))
    {
      my_error(ER_NO_DEFAULT, MYF(0), var->getName().c_str());
      return -1;
    }
    return 0;
  }

  if ((!value->fixed &&
       value->fix_fields(session, &value)) || value->check_cols(1))
    return -1;
  if (var->check_update_type(value->result_type()))
  {
    my_error(ER_WRONG_TYPE_FOR_VAR, MYF(0), var->getName().c_str());
    return -1;
  }
  return var->check(session, this) ? -1 : 0;
}

/**
  Update variable

  @param   session    thread handler
  @returns 0|1    ok or	ERROR

  @note ERROR can be only due to abnormal operations involving
  the server's execution evironment such as
  out of memory, hard disk failure or the computer blows up.
  Consider set_var::check() method if there is a need to return
  an error due to logics.
*/
int set_var::update(Session *session)
{
  try
  {
    if (not value)
      var->set_default(session, type);
    else if (var->update(session, this))
      return -1;				// should never happen
    if (var->getAfterUpdateTrigger())
      (*var->getAfterUpdateTrigger())(session, type);
  }
  catch (invalid_option_value &ex)
  {
    /* TODO: Fix this to be typesafe once we have properly typed set_var */
    string new_val= boost::lexical_cast<string>(uint64_t_value);
    if (boost::get_error_info<invalid_max_info>(ex) != NULL)
    { 
      const uint64_t max_val= *(boost::get_error_info<invalid_max_info>(ex));
      string explanation("(> ");
      explanation.append(boost::lexical_cast<std::string>(max_val));
      explanation.push_back(')');
      push_warning_printf(session, DRIZZLE_ERROR::WARN_LEVEL_ERROR,
                          ER_INVALID_OPTION_VALUE,
                          ER(ER_INVALID_OPTION_VALUE),
                          var->getName().c_str(),
                          new_val.c_str(),
                          explanation.c_str());
    }
    else if (boost::get_error_info<invalid_min_info>(ex) != NULL)
    { 
      const int64_t min_val= *(boost::get_error_info<invalid_min_info>(ex));
      string explanation("(< ");
      explanation.append(boost::lexical_cast<std::string>(min_val));
      explanation.push_back(')');
      push_warning_printf(session, DRIZZLE_ERROR::WARN_LEVEL_ERROR,
                          ER_INVALID_OPTION_VALUE,
                          ER(ER_INVALID_OPTION_VALUE),
                          var->getName().c_str(),
                          new_val.c_str(),
                          explanation.c_str());
    }
    else if (boost::get_error_info<invalid_value>(ex) != NULL)
    {
      const std::string str_val= *(boost::get_error_info<invalid_value>(ex));
      string explanation("(");
      explanation.append(str_val);
      explanation.push_back(')');
      push_warning_printf(session, DRIZZLE_ERROR::WARN_LEVEL_ERROR,
                          ER_INVALID_OPTION_VALUE,
                          ER(ER_INVALID_OPTION_VALUE),
                          var->getName().c_str(),
                          new_val.c_str(),
                          explanation.c_str());
    }
    else
    {
      push_warning_printf(session, DRIZZLE_ERROR::WARN_LEVEL_ERROR,
                          ER_INVALID_OPTION_VALUE,
                          ER(ER_INVALID_OPTION_VALUE),
                          var->getName().c_str(),
                          new_val.c_str(),
                          "");
    }
  }
  return 0;
}

/*****************************************************************************
  Functions to handle SET @user_variable=const_expr
*****************************************************************************/

int set_var_user::check(Session *session)
{
  /*
    Item_func_set_user_var can't substitute something else on its place =>
    0 can be passed as last argument (reference on item)
  */
  return (user_var_item->fix_fields(session, (Item**) 0) ||
	  user_var_item->check(0)) ? -1 : 0;
}


int set_var_user::update(Session *)
{
  user_var_item->update();
	return 0;
}

void set_var::setValue(const std::string &new_value)
{
  str_value= new_value;
}

void set_var::setValue(uint64_t new_value)
{
  uint64_t_value= new_value;
}

void set_var::updateValue()
{
  if (var->show_type() != SHOW_CHAR)
  {
    uint64_t_value= value->val_int();
  }
}


} /* namespace drizzled */
