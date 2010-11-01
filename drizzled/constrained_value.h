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

#ifndef DRIZZLED_CONSTRAINED_VALUE_H
#define DRIZZLED_CONSTRAINED_VALUE_H

#include <boost/program_options.hpp>
#include <boost/program_options/errors.hpp>
#include <iostream>

namespace drizzled
{

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

  T getVal() const
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
    os << v.getVal();
    return os;
  }
};

template<class T,
  T MAXVAL= std::numeric_limits<T>::max(),
  T MINVAL= std::numeric_limits<T>::min(), unsigned int ALIGN= 1>
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
    return set_value(rhs.getVal());
  }

  constrained_value<T>& set_value(T rhs)
  {
    if ((rhs > MAXVAL) || (rhs < MINVAL))
    {
      boost::throw_exception(boost::program_options::invalid_option_value(boost::lexical_cast<std::string>(rhs)));
    }
    rhs-= rhs % ALIGN;
    this->setVal(rhs);
    return *this;
  }


};

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


#endif /* DRIZZLED_CONSTRAINED_VALUE_H */
