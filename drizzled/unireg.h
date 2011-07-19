/* -*- mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; -*-
 *  vim:expandtab:shiftwidth=2:tabstop=2:smarttab:
 *
 *  Copyright (C) 2008 Sun Microsystems, Inc.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
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

/*  Extra functions used by unireg library */

#pragma once

#include <drizzled/visibility.h>
#include <sstream>

namespace drizzled
{

void unireg_exit() __attribute__((noreturn));
DRIZZLED_API void unireg_actual_abort(const char *file, int line, const char *func, const std::string& message) __attribute__((noreturn));
void unireg_startup_finished();

namespace stream {

namespace detail {

template<class Ch, class Tr, class A>
  class _unireg {
  private:

  public:
    typedef std::basic_ostringstream<Ch, Tr, A> stream_buffer;

  public:
    void operator()(const stream_buffer &s, const char *filename, int line, const char *func)
    {
      unireg_actual_abort(filename, line, func, s.str());
    }
  };

template<template <class Ch, class Tr, class A> class OutputPolicy, class Ch = char, class Tr = std::char_traits<Ch>, class A = std::allocator<Ch> >
  class log {
  private:
    typedef OutputPolicy<Ch, Tr, A> output_policy;
    const char *_filename;
    int _line_number;
    const char *_func;

  public:
    log() :
      _filename(NULL),
      _line_number(0),
      _func(NULL)
    { }

    void set_filename(const char *filename, int line_number, const char *func)
    {
      _filename= filename;
      _line_number= line_number;
      _func= func;
    }

    ~log()
    {
      output_policy()(arg, _filename, _line_number, _func);
    }

  public:
    template<class T>
      log &operator<<(const T &x)
      {
        arg << x;
        return *this;
      }

  private:
    typename output_policy::stream_buffer arg;
  };
} // namespace detail

class _unireg : public detail::log<detail::_unireg> {
public:
  _unireg(const char *filename, int line_number, const char *func)
  {
    set_filename(filename, line_number, func);
  }
};

} // namespace stream

#define unireg_abort stream::_unireg(__FILE__, __LINE__, __func__)

} /* namespace drizzled */
