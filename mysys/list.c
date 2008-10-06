/* Copyright (C) 2000 MySQL AB

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA */

/*
  Code for handling dubble-linked lists in C
*/

#include "mysys_priv.h"
#include <my_list.h>



	/* Add a element to start of list */

LIST *list_add(LIST *root, LIST *element)
{
  if (root)
  {
    if (root->prev)			/* If add in mid of list */
      root->prev->next= element;
    element->prev=root->prev;
    root->prev=element;
  }
  else
    element->prev=0;
  element->next=root;
  return(element);			/* New root */
}


LIST *list_delete(LIST *root, LIST *element)
{
  if (element->prev)
    element->prev->next=element->next;
  else
    root=element->next;
  if (element->next)
    element->next->prev=element->prev;
  return root;
}


void list_free(LIST *root, uint free_data)
{
  LIST *next;
  while (root)
  {
    next=root->next;
    if (free_data)
      free((uchar*) root->data);
    free((uchar*) root);
    root=next;
  }
}


LIST *list_cons(void *data, LIST *list)
{
  LIST * const new_charset=(LIST*) my_malloc(sizeof(LIST),MYF(MY_FAE));
  if (!new_charset)
    return NULL;
  new_charset->data=data;
  return list_add(list,new_charset);
}


LIST *list_reverse(LIST *root)
{
  LIST *last;

  last=root;
  while (root)
  {
    last=root;
    root=root->next;
    last->next=last->prev;
    last->prev=root;
  }
  return last;
}

int list_walk(LIST *list, list_walk_action action, uchar* argument)
{
  while (list)
  {
    int error;
    if ((error = (*action)(list->data,argument)))
      return error;
    list=list_rest(list);
  }
  return 0;
}
