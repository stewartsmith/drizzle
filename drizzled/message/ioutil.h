/* -*- mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; -*-
 *  vim:expandtab:shiftwidth=2:tabstop=2:smarttab:
 *
 *  Copyright (C) 2009 Sun Microsystems, Inc.
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

#include <iosfwd>
#include <string>

namespace drizzled
{
namespace message
{
namespace ioutil
{

  /**
     Helper class for join() I/O manipulator.

     This is a join I/O manipulator with arguments for @c join(). These has to be
  */
  template <class FwdIter> class joiner {
    friend std::ostream& operator<<(std::ostream& out, const joiner& j) {
      j.write(out);
      return out;
    }

  public:
    explicit joiner(const std::string& separator, FwdIter start, FwdIter finish)
      : m_sep(separator), m_start(start), m_finish(finish)
    { }


  private:
    std::string m_sep;
    FwdIter m_start, m_finish;

    void write(std::ostream& out) const {
      if (m_start == m_finish)
        return;

      FwdIter fi = m_start;
      while (true) {
        out << *fi;
        if (++fi == m_finish)
          break;
        out << m_sep;
      }
    }
  };


  /**
     Join manipulators for writing delimiter-separated strings to an
     ostream object.

     Use the manipulator as follows:
     @code
     std::cout << ioutil::join(",", list.begin(), list.end()) << std::endl;
     std::cout << ioutil::join(",", list) << std::endl;
     @endcode
  */
  template <class FwdIter>
  joiner<FwdIter>
  join(const std::string& delim, FwdIter start, FwdIter finish) {
    return joiner<FwdIter>(delim, start, finish);
  }

  /** @overload */
  template <class Container>
  joiner<typename Container::const_iterator>
  join(const std::string& delim, Container seq) {
    typedef typename Container::const_iterator FwdIter;
    return joiner<FwdIter>(delim, seq.begin(), seq.end());
  }

} /* namespace ioutil */
} /* namespace message */
} /* namespace drizzled */

