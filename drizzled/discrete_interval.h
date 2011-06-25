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

#include <cstdlib>

#include <drizzled/definitions.h>

namespace drizzled
{

/*
  Such interval is "discrete": it is the set of
  { auto_inc_interval_min + k * increment,
    0 <= k <= (auto_inc_interval_values-1) }
  Where "increment" is maintained separately by the user of this class (and is
  currently only session->variables.auto_increment_increment).
  It mustn't derive from memory::SqlAlloc, because SET INSERT_ID needs to
  allocate memory which must stay allocated for use by the next statement.
*/
class Discrete_interval {
private:
  uint64_t interval_min;
  uint64_t interval_values;
  uint64_t  interval_max;    // excluded bound. Redundant.
public:
  Discrete_interval *next;    // used when linked into Discrete_intervals_list
  void replace(uint64_t start, uint64_t val, uint64_t incr)
  {
    interval_min=    start;
    interval_values= val;
    interval_max=    (val == UINT64_MAX) ? val : start + val * incr;
  }
  Discrete_interval(uint64_t start, uint64_t val, uint64_t incr) :
    interval_min(start), interval_values(val),
    interval_max((val == UINT64_MAX) ? val : start + val * incr),
    next(NULL)
  {}
  Discrete_interval() :
    interval_min(0), interval_values(0),
    interval_max(0), next(NULL)
  {}
  uint64_t minimum() const { return interval_min;    }
  uint64_t values()  const { return interval_values; }
  uint64_t maximum() const { return interval_max;    }
  /*
    If appending [3,5] to [1,2], we merge both in [1,5] (they should have the
    same increment for that, user of the class has to ensure that). That is
    just a space optimization. Returns 0 if merge succeeded.
  */
  bool merge_if_contiguous(uint64_t start, uint64_t val, uint64_t incr)
  {
    if (interval_max == start)
    {
      if (val == UINT64_MAX)
      {
        interval_values=   interval_max= val;
      }
      else
      {
        interval_values+=  val;
        interval_max=      start + val * incr;
      }
      return 0;
    }
    return 1;
  }
};



/* List of Discrete_interval objects */
class Discrete_intervals_list {
private:
  Discrete_interval        *head;
  Discrete_interval        *tail;
  /*
    When many intervals are provided at the beginning of the execution of a
    statement (in a replication slave or SET INSERT_ID), "current" points to
    the interval being consumed by the thread now (so "current" goes from
    "head" to "tail" then to NULL).
  */
  Discrete_interval        *current;
  uint32_t                  elements; // number of elements

  /* helper function for copy construct and assignment operator */
  void copy_(const Discrete_intervals_list& from)
  {
    for (Discrete_interval *i= from.head; i; i= i->next)
    {
      Discrete_interval j= *i;
      append(&j);
    }
  }
public:
  Discrete_intervals_list() :
    head(NULL), tail(NULL),
    current(NULL), elements(0) {}
  Discrete_intervals_list(const Discrete_intervals_list& from) :
    head(NULL), tail(NULL),
    current(NULL), elements(0)
  {
    copy_(from);
  }
  Discrete_intervals_list& operator=(const Discrete_intervals_list& from)
  {
    empty();
    copy_(from);
    return *this;
  }
  void empty_no_free()
  {
    head= current= NULL;
    elements= 0;
  }
  void empty()
  {
    for (Discrete_interval *i= head; i;)
    {
      Discrete_interval *next= i->next;
      delete i;
      i= next;
    }
    empty_no_free();
  }

  const Discrete_interval* get_next()
  {
    Discrete_interval *tmp= current;
    if (current != NULL)
      current= current->next;
    return tmp;
  }
  ~Discrete_intervals_list() { empty(); }
  uint64_t minimum()     const { return (head ? head->minimum() : 0); }
  uint64_t maximum()     const { return (head ? tail->maximum() : 0); }
  uint32_t      nb_elements() const { return elements; }

  bool append(uint64_t start, uint64_t val, uint64_t incr)
  {
    /* first, see if this can be merged with previous */
    if ((head == NULL) || tail->merge_if_contiguous(start, val, incr))
    {
      /* it cannot, so need to add a new interval */
      Discrete_interval *new_interval= new Discrete_interval(start, val, incr);
      return(append(new_interval));
    }
    return 0;
  }

  bool append(Discrete_interval *new_interval)
  {
    if (unlikely(new_interval == NULL))
      return 1;
    if (head == NULL)
      head= current= new_interval;
    else
      tail->next= new_interval;
    tail= new_interval;
    elements++;
    return 0;
  }

};

} /* namespace drizzled */

