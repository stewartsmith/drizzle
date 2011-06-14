/* -*- mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; -*-
 *  vim:expandtab:shiftwidth=2:tabstop=2:smarttab:
 *
 *  Copyright (C) 2010 Monty Taylor
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

#include <boost/exception/info.hpp>
#include <boost/program_options.hpp>
#include <boost/program_options/errors.hpp>
#include <iostream>
#include <netinet/in.h> /* for in_port_t */

namespace drizzled
{

/* We have to make this mixin exception class because boost program_option
  exceptions don't derive from f-ing boost::exception. FAIL
*/
class invalid_option_value :
  public boost::exception,
  public boost::program_options::invalid_option_value
{
public:
  invalid_option_value(const std::string &option_value) :
    boost::exception(),
    boost::program_options::invalid_option_value(option_value)
  {}
};

template<class T> class constrained_value;
template<class T>
std::istream& operator>>(std::istream& is, constrained_value<T>& bound_val);
template<class T>
std::ostream& operator<<(std::ostream& os, const constrained_value<T>& v);

template<class T>
class constrained_value
{
  T m_val;
protected:

  virtual constrained_value<T>& set_value(const constrained_value<T>& rhs)= 0;
  virtual constrained_value<T>& set_value(T rhs)= 0;

public:
  explicit constrained_value<T>(T in_value= 0) :
    m_val(in_value)
  { }

  virtual ~constrained_value<T>()
  {}

  operator T() const
  {
    return m_val;
  }

  constrained_value<T>& operator=(const constrained_value<T>& rhs)
  {
    return set_value(rhs);
  }

  constrained_value<T>& operator=(T rhs)
  {
    return set_value(rhs);
  }

  T get() const
  {
    return m_val;
  }

  void setVal(T in_val)
  {
    m_val= in_val;
  }

  friend std::istream&
  operator>>(std::istream& is,
             constrained_value<T>& bound_val)
  {
    T inner_val;
    is >> inner_val;
    bound_val= inner_val;
    return is;
  }

  friend
  std::ostream& operator<<(std::ostream& os, const constrained_value<T>& v)
  {
    os << v.get();
    return os;
  }
};

namespace
{
template<class T, T min_val>
bool less_than_min(T val_to_check)
{
  return val_to_check < min_val;
}

template<>
inline bool less_than_min<uint16_t, 0>(uint16_t)
{
  return false;
}

template<>
inline bool less_than_min<uint32_t, 0>(uint32_t)
{
  return false;
}

template<>
inline bool less_than_min<uint64_t, 0>(uint64_t)
{
  return false;
}

template<class T, T min_val>
bool greater_than_max(T val_to_check)
{
  return val_to_check > min_val;
}

template<>
inline bool greater_than_max<uint16_t, UINT16_MAX>(uint16_t)
{
  return false;
}

template<>
inline bool greater_than_max<uint32_t, UINT32_MAX>(uint32_t)
{
  return false;
}

template<>
inline bool greater_than_max<uint64_t, UINT64_MAX>(uint64_t)
{
  return false;
}
} 

typedef boost::error_info<struct tag_invalid_max,uint64_t> invalid_max_info;
typedef boost::error_info<struct tag_invalid_min,int64_t> invalid_min_info;
typedef boost::error_info<struct tag_invalid_min,std::string> invalid_value;

template<class T,
  T MAXVAL,
  T MINVAL, unsigned int ALIGN= 1>
class constrained_check :
  public constrained_value<T>
{
public:
  constrained_check<T,MAXVAL,MINVAL,ALIGN>(T in_value= 0) :
    constrained_value<T>(in_value)
  { }

protected:
  constrained_value<T>& set_value(const constrained_value<T>& rhs)
  {
    return set_value(rhs.get());
  }

  constrained_value<T>& set_value(T rhs)
  {
    if (greater_than_max<T,MAXVAL>(rhs))
    {
      boost::throw_exception(invalid_option_value(boost::lexical_cast<std::string>(rhs)) << invalid_max_info(static_cast<uint64_t>(MAXVAL)));
    }
      
    if (less_than_min<T,MINVAL>(rhs))
    {
      boost::throw_exception(invalid_option_value(boost::lexical_cast<std::string>(rhs)) << invalid_min_info(static_cast<int64_t>(MINVAL)));
    }
    rhs-= rhs % ALIGN;
    this->setVal(rhs);
    return *this;
  }


};

typedef constrained_check<uint64_t, UINT64_MAX, 0> uint64_constraint;
typedef constrained_check<uint32_t, UINT32_MAX, 0> uint32_constraint;
typedef constrained_check<uint64_t, UINT64_MAX, 1> uint64_nonzero_constraint;
typedef constrained_check<uint32_t, UINT32_MAX, 1> uint32_nonzero_constraint;
typedef drizzled::constrained_check<in_port_t, 65535, 0> port_constraint;

typedef constrained_check<uint32_t,65535,1> back_log_constraints;

} /* namespace drizzled */

template<class T>
void validate(boost::any& v,
              const std::vector<std::string>& values,
              drizzled::constrained_value<T> val, int)
{
  boost::program_options::validators::check_first_occurrence(v);
  const std::string& s= boost::program_options::validators::get_single_string(values);

  val= boost::lexical_cast<T>(s);
  v= boost::any(val);
}


