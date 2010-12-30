/* - mode: c; c-basic-offset: 2; indent-tabs-mode: nil; -*-
 *  vim:expandtab:shiftwidth=2:tabstop=2:smarttab:
 *
 * Copyright (C) 2010 Brian Aker
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in the
 *       documentation and/or other materials provided with the distribution.
 *     * Neither the name of the <organization> nor the
 *       names of its contributors may be used to endorse or promote products
 *       derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL <COPYRIGHT HOLDER> BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */

/*
  I've looked at a number of examples on the web, this is a composite of what I have seen/liked.
  -Brian
*/

#include <boost/checked_delete.hpp>
#include <boost/detail/atomic_count.hpp>
#include <boost/intrusive_ptr.hpp>
#include <boost/thread.hpp>

#ifndef DRIZZLED_UTIL_INTRUSIVE_BASE_PTR_H
#define DRIZZLED_UTIL_INTRUSIVE_BASE_PTR_H

namespace drizzled {
namespace util {

template<class T>
class intrusive_ptr_base
{
public:
  intrusive_ptr_base():
    _reference_count(0)
  {
  }

  intrusive_ptr_base(intrusive_ptr_base<T> const&) :
    _reference_count(0)
  {
  }

  intrusive_ptr_base& operator=(intrusive_ptr_base const& rhs)
  { 
    return *this; 
  }

  friend void intrusive_ptr_add_ref(intrusive_ptr_base<T> const* s)
  {
    assert(s->_reference_count >= 0);
    assert(s != 0);
    ++s->_reference_count;
  }

  friend void intrusive_ptr_release(intrusive_ptr_base<T> const* s)
  {
    assert(s->_reference_count > 0);
    assert(s != 0);

    if (--s->_reference_count == 0)
      boost::checked_delete(static_cast<T const*>(s));
  }

  boost::intrusive_ptr<T> self()
  { 
    return boost::intrusive_ptr<T>((T*)this); 
  }

  boost::intrusive_ptr<const T> self() const
  { 
    return boost::intrusive_ptr<const T>((T const*)this); 
  }

  bool unique() const 
  {
    return _reference_count == 0;
  }

  int use_count() const 
  { 
    return _reference_count; 
  }

private:
  mutable boost::detail::atomic_count _reference_count;
};

} // namespace util
} // namespace drizzled

#endif /* DRIZZLED_UTIL_INTRUSIVE_BASE_PTR_H */
