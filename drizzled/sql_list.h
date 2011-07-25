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
#include <cassert>
#include <utility>
#include <algorithm>
#include <drizzled/memory/sql_alloc.h>
#include <drizzled/visibility.h>

namespace drizzled {

typedef struct st_sql_list 
{
  uint32_t elements;
  unsigned char *first;
  unsigned char **next;

  void clear()
  {
    elements=0;
    first=0;
    next= &first;
  }

  size_t size() const
  {
    return elements;
  }

  void link_in_list(unsigned char *element,unsigned char **next_ptr)
  {
    elements++;
    (*next)=element;
    next= next_ptr;
    *next=0;
  }
  void save_and_clear(struct st_sql_list *save)
  {
    *save= *this;
    clear();
  }
  void push_front(struct st_sql_list *save)
  {
    *save->next= first;				/* link current list last */
    first= save->first;
    elements+= save->elements;
  }
  void push_back(struct st_sql_list *save)
  {
    if (save->first)
    {
      *next= save->first;
      next= save->next;
      elements+= save->elements;
    }
  }
} SQL_LIST;

/*
  Basic single linked list
  Used for item and item_buffs.
  All list ends with a pointer to the 'end_of_list' element, which
  data pointer is a null pointer and the next pointer points to itself.
  This makes it very fast to traverse lists as we don't have to
  test for a specialend condition for list that can't contain a null
  pointer.
*/


/**
  list_node - a node of a single-linked list.
  @note We never call a destructor for instances of this class.
*/

struct list_node : public memory::SqlAlloc
{
  list_node *next;
  void *info;
  list_node(void *info_par,list_node *next_par)
    :next(next_par),info(info_par)
  {}
  list_node()					/* For end_of_list */
  {
    info= 0;
    next= this;
  }
};

extern DRIZZLED_API list_node end_of_list;

class base_list :public memory::SqlAlloc
{
protected:
  list_node *first,**last;
  uint32_t elements;
public:

  void clear() { elements=0; first= &end_of_list; last=&first;}
  base_list() { clear(); }
  /**
    This is a shallow copy constructor that implicitly passes the ownership
    from the source list to the new instance. The old instance is not
    updated, so both objects end up sharing the same nodes. If one of
    the instances then adds or removes a node, the other becomes out of
    sync ('last' pointer), while still operational. Some old code uses and
    relies on this behaviour. This logic is quite tricky: please do not use
    it in any new code.
  */
  base_list(const base_list &tmp) :memory::SqlAlloc()
  {
    elements= tmp.elements;
    first= tmp.first;
    last= elements ? tmp.last : &first;
  }
  base_list(bool) { }
  void push_back(void *info)
  {
    *last = new list_node(info, &end_of_list);
    last= &(*last)->next;
    elements++;
  }
  void push_back(void *info, memory::Root& mem)
  {
    *last = new (mem) list_node(info, &end_of_list);
    last= &(*last)->next;
    elements++;
  }
  void push_front(void *info)
  {
    list_node *node=new list_node(info,first);
    if (last == &first)
			last= &node->next;
      first=node;
      elements++;
  }
  void remove(list_node **prev)
  {
    list_node *node=(*prev)->next;
    if (!--elements)
      last= &first;
    else if (last == &(*prev)->next)
      last= prev;
    delete *prev;
    *prev=node;
  }
  void concat(base_list *list)
  {
    if (!list->is_empty())
    {
      *last= list->first;
      last= list->last;
      elements+= list->elements;
    }
  }
  void *pop()
  {
    if (first == &end_of_list) return 0;
    list_node *tmp=first;
    first=first->next;
    if (!--elements)
      last= &first;
    return tmp->info;
  }
  void disjoin(base_list *list)
  {
    list_node **prev= &first;
    list_node *node= first;
    list_node *list_first= list->first;
    elements=0;
    while (node && node != list_first)
    {
      prev= &node->next;
      node= node->next;
      elements++;
    }
    *prev= *last;
    last= prev;
  }
  void prepand(base_list *list)
  {
    if (!list->is_empty())
    {
      *list->last= first;
      first= list->first;
      elements+= list->elements;
    }
  }
  /**
    Swap two lists.
  */
  void swap(base_list &rhs)
  {
    std::swap(first, rhs.first);
    std::swap(last, rhs.last);
    std::swap(elements, rhs.elements);
  }
  bool is_empty() { return first == &end_of_list ; }
  friend class base_list_iterator;

#ifdef LIST_EXTRA_DEBUG
  /*
    Check list invariants and print results into trace. Invariants are:
      - (*last) points to end_of_list
      - There are no NULLs in the list.
      - base_list::elements is the number of elements in the list.

    SYNOPSIS
      check_list()
        name  Name to print to trace file

    RETURN
      1  The list is Ok.
      0  List invariants are not met.
  */

  bool check_list(const char *name)
  {
    base_list *list= this;
    list_node *node= first;
    uint32_t cnt= 0;

    while (node->next != &end_of_list)
    {
      if (!node->info)
      {
        return false;
      }
      node= node->next;
      cnt++;
    }
    if (last != &(node->next))
    {
      return false;
    }
    if (cnt+1 != elements)
    {
      return false;
    }
    return true;
  }
#endif // LIST_EXTRA_DEBUG

protected:
  void after(void *info,list_node *node)
  {
    list_node *new_node=new list_node(info,node->next);
    node->next=new_node;
    elements++;
    if (last == &(node->next))
      last= &new_node->next;
  }
};


class base_list_iterator
{
protected:
  base_list *list;
  list_node **el,**prev,*current;
public:
  void sublist(base_list &ls, uint32_t elm)
  {
    ls.first= *el;
    ls.last= list->last;
    ls.elements= elm;
  }
  base_list_iterator()
    :list(0), el(0), prev(0), current(0)
  {}

  base_list_iterator(base_list &list_par, list_node** el0)
    :list(&list_par), el(el0), prev(0), current(0)
  {
  }

  void *replace(base_list &new_list)
  {
    void *ret_value=current->info;
    if (!new_list.is_empty())
    {
      *new_list.last=current->next;
      current->info=new_list.first->info;
      current->next=new_list.first->next;
      if (list->last == &current->next && new_list.elements > 1)
        list->last= new_list.last;
      list->elements+=new_list.elements-1;
    }
    return ret_value;				// return old element
  }
  void remove()			// Remove current
  {
    list->remove(prev);
    el=prev;
    current=0;					// Safeguard
  }
  void after(void *element)			// Insert element after current
  {
    list->after(element,current);
    current=current->next;
    el= &current->next;
  }
};

template <class T> class List_iterator;

template <class T> class List : public base_list
{
public:
  typedef List_iterator<T> iterator;

  friend class List_iterator<T>;

  List() {}
  List(const List<T> &tmp) : base_list(tmp) {}
  List(const List<T> &tmp, memory::Root *mem_root) : base_list(tmp, mem_root) {}
  T& front() {return *static_cast<T*>(first->info); }
  T* pop()  {return static_cast<T*>(base_list::pop()); }
  void concat(List<T> *list) { base_list::concat(list); }
  void disjoin(List<T> *list) { base_list::disjoin(list); }
  void prepand(List<T> *list) { base_list::prepand(list); }
  void delete_elements()
  {
    list_node *element,*next;
    for (element=first; element != &end_of_list; element=next)
    {
      next=element->next;
      delete (T*) element->info;
    }
    clear();
  }

  iterator begin()
  {
    return iterator(*this, &first);
  }

  size_t size() const
  {
    return elements;
  }

  void set_size(size_t v)
  {
    elements = v;
  }
};

template <class T> class List_iterator : public base_list_iterator
{
public:
  List_iterator(List<T>& a, list_node** b) : base_list_iterator(a, b) {};
  List_iterator() {};
  T *operator++(int) { prev=el; current= *el; el= &current->next; return (T*)current->info; }
  T *replace(T *a)   { T* old = (T*) current->info; current->info= a; return old; }
  void replace(List<T> &a) { base_list_iterator::replace(a); }
  T** ref() { return (T**) &current->info; }

  T& operator*()
  {
    return *(T*)current->info;
  }

  T* operator->()
  {
    return (T*)current->info;
  }
};

} /* namespace drizzled */

