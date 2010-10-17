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
#include <boost/exception/all.hpp>
#include <iostream>

namespace drizzled
{

template<class T>
class constrained_value
{
public:
  constrained_value<T>(T in_value= 0,
                       T in_max_val= std::numeric_limits<T>::max(),
                       T in_min_val= std::numeric_limits<T>::min(),
                       int in_align_to= 1) :
    m_val(in_value),
    m_max_val(in_max_val),
    m_min_val(in_min_val),
    m_align_to(in_align_to)
  { }

  constrained_value<T>(const constrained_value<T>& old) :
    m_val(old.m_val),
    m_max_val(old.m_max_val),
    m_min_val(old.m_min_val),
    m_align_to(old.m_align_to)
  { }

  constrained_value<T>& operator=(const constrained_value<T>& rhs)
  {
    (*this)= rhs.m_val;
    return *this;
  }

  constrained_value<T>& operator=(T rhs)
  {
    if ((rhs > m_max_val) || (rhs < m_min_val))
    {
      boost::throw_exception(boost::program_options::validation_error(boost::program_options::validation_error::invalid_option_value));
    }
    rhs-= rhs % m_align_to;
    m_val= rhs;
    return *this;
  }

  operator T()
  {
    return m_val;
  }

  template<class CharT, class Traits>
  friend std::basic_istream<CharT,Traits>&
  operator>>(std::basic_istream<CharT,Traits>& is,
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
    os << v.m_val;
    return os;
  }

private:
  T m_val;
  T m_max_val;
  T m_min_val;
  int m_align_to;
};

} /* namespace drizzled */

template<class T>
void validate(boost::any& v,
              const std::vector<std::string>& values,
              drizzled::constrained_value<T>, int)
{
  boost::program_options::validators::check_first_occurrence(v);
  const std::string& s= boost::program_options::validators::get_single_string(values);

  drizzled::constrained_value<T> &val= boost::any_cast<T>(v);
  try
  {
    val= boost::lexical_cast<T>(s);
  }
  catch (...)
  {
    boost::throw_exception(boost::program_options::validation_error(boost::program_options::validation_error::invalid_option_value));
  }
  v= boost::any(val);
}

#endif /* DRIZZLED_CONSTRAINED_VALUE_H */
