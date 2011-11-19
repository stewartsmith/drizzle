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
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA */

/*
  Code for handling red-black (balanced) binary trees.
  key in tree is allocated according to following:

  1) If size < 0 then tree will not allocate keys and only a pointer to
     each key is saved in tree.
     compare and search functions uses and returns key-pointer

  2) If size == 0 then there are two options:
       - key_size != 0 to tree_insert: The key will be stored in the tree.
       - key_size == 0 to tree_insert:  A pointer to the key is stored.
     compare and search functions uses and returns key-pointer.

  3) if key_size is given to init_tree then each node will continue the
     key and calls to insert_key may increase length of key.
     if key_size > sizeof(pointer) and key_size is a multiple of 8 (double
     align) then key will be put on a 8 aligned address. Else
     the key will be on address (element+1). This is transparent for user
     compare and search functions uses a pointer to given key-argument.

  - If you use a free function for tree-elements and you are freeing
    the element itself, you should use key_size = 0 to init_tree and
    tree_search

  The actual key in TREE_ELEMENT is saved as a pointer or after the
  TREE_ELEMENT struct.
  If one uses only pointers in tree one can use tree_set_pointer() to
  change address of data.

  Implemented by monty.
*/

/*
  NOTE:
  tree->compare function should be ALWAYS called as
    (*compare)(custom_arg, element_key(element), key)
  and NOT the other way around, as
    (*compare)(custom_arg, key, element_key(element))
*/

#include <config.h>

#include <drizzled/tree.h>
#include <drizzled/internal/my_sys.h>
#include <drizzled/internal/m_string.h>

#define BLACK		1
#define RED		0
#define DEFAULT_ALLOC_SIZE 8192
#define DEFAULT_ALIGN_SIZE 8192


namespace drizzled
{

/**
 * Tree class public methods
 */

void Tree::init_tree(size_t default_alloc_size, uint32_t mem_limit,
               uint32_t size, qsort_cmp2 compare_callback, bool free_with_tree,
	       tree_element_free free_callback, void *caller_arg)
{
  if (default_alloc_size < DEFAULT_ALLOC_SIZE)
    default_alloc_size= DEFAULT_ALLOC_SIZE;
  default_alloc_size= MY_ALIGN(default_alloc_size, DEFAULT_ALIGN_SIZE);
  memset(&this->null_element, 0, sizeof(this->null_element));
  root= &this->null_element;
  compare= compare_callback;
  size_of_element= size > 0 ? (uint32_t) size : 0;
  memory_limit= mem_limit;
  free= free_callback;
  allocated= 0;
  elements_in_tree= 0;
  custom_arg = caller_arg;
  null_element.colour= BLACK;
  null_element.left=this->null_element.right= 0;
  flag= 0;
  if (!free_callback &&
      (size <= sizeof(void*) || ((uint32_t) size & (sizeof(void*)-1))))
  {
    /*
      We know that the data doesn't have to be aligned (like if the key
      contains a double), so we can store the data combined with the
      Tree_Element.
    */
    offset_to_key= sizeof(Tree_Element); /* Put key after element */
    /* Fix allocation size so that we don't lose any memory */
    default_alloc_size/= (sizeof(Tree_Element)+size);
    if (!default_alloc_size)
      default_alloc_size= 1;
    default_alloc_size*= (sizeof(Tree_Element)+size);
  }
  else
  {
    offset_to_key= 0;		/* use key through pointer */
    size_of_element+= sizeof(void*);
  }
  if (! (with_delete= free_with_tree))
  {
    mem_root.init(default_alloc_size);
    mem_root.min_malloc= (sizeof(Tree_Element)+size_of_element);
  }
}

void Tree::delete_tree()
{
  free_tree(MYF(0)); /* free() mem_root if applicable */
}

void Tree::reset_tree()
{
  /* do not free mem_root, just mark blocks as free */
  free_tree(MYF(memory::MARK_BLOCKS_FREE));
}

Tree_Element* Tree::tree_insert(void* key, uint32_t key_size, void* caller_arg)
{
  int cmp;
  Tree_Element *element,***parent;

  parent= this->parents;
  *parent = &this->root; element= this->root;
  for (;;)
  {
    if (element == &this->null_element ||
	(cmp = (*compare)(caller_arg, element_key(element), key)) == 0)
      break;
    if (cmp < 0)
    {
      *++parent= &element->right; element= element->right;
    }
    else
    {
      *++parent = &element->left; element= element->left;
    }
  }
  if (element == &this->null_element)
  {
    size_t alloc_size= sizeof(Tree_Element)+key_size+this->size_of_element;
    this->allocated+= alloc_size;

    if (this->memory_limit && this->elements_in_tree
                           && this->allocated > this->memory_limit)
    {
      reset_tree();
      return tree_insert(key, key_size, caller_arg);
    }

    key_size+= this->size_of_element;
    if (this->with_delete)
      element= (Tree_Element *) malloc(alloc_size);
    else
      element= (Tree_Element *) this->mem_root.alloc(alloc_size);
    **parent= element;
    element->left= element->right= &this->null_element;
    if (!this->offset_to_key)
    {
      if (key_size == sizeof(void*))		 /* no length, save pointer */
	*((void**) (element+1))= key;
      else
      {
	*((void**) (element+1))= (void*) ((void **) (element+1)+1);
	memcpy(*((void **) (element+1)),key, key_size - sizeof(void*));
      }
    }
    else
      memcpy((unsigned char*) element + this->offset_to_key, key, key_size);
    element->count= 1;			/* May give warning in purify */
    this->elements_in_tree++;
    rb_insert(parent,element);	/* rebalance tree */
  }
  else
  {
    if (this->flag & TREE_NO_DUPS)
      return(NULL);
    element->count++;
    /* Avoid a wrap over of the count. */
    if (! element->count)
      element->count--;
  }

  return element;
}

int Tree::tree_walk(tree_walk_action action, void *argument, TREE_WALK visit)
{
	  switch (visit) {
	  case left_root_right:
	    return tree_walk_left_root_right(root,action,argument);
	  case right_root_left:
	    return tree_walk_right_root_left(root,action,argument);
  }

  return 0;			/* Keep gcc happy */
}

/**
 * Tree class private methods
 */

void Tree::free_tree(myf free_flags)
{
  if (root)				/* If initialized */
  {
    if (with_delete)
      delete_tree_element(root);
    else
    {
      if (free)
      {
        if (memory_limit)
          (*free)(NULL, free_init, custom_arg);
        delete_tree_element(root);
        if (memory_limit)
          (*free)(NULL, free_end, custom_arg);
      }
      mem_root.free_root(free_flags);
    }
  }
  root= &null_element;
  elements_in_tree= 0;
  allocated= 0;
}

void* Tree::element_key(Tree_Element* element)
{
	return offset_to_key ? (void*)((unsigned char*) element + offset_to_key)
						 : *((void**)(element + 1));
}

void Tree::delete_tree_element(Tree_Element *element)
{
  if (element != &null_element)
  {
    delete_tree_element(element->left);
    if (free)
      (*free)(element_key(element), free_free, custom_arg);
    delete_tree_element(element->right);
    if (with_delete)
      delete element;
  }
}

int Tree::tree_walk_left_root_right(Tree_Element *element, tree_walk_action action, void *argument)
{
  int error;
  if (element->left)				/* Not null_element */
  {
    if ((error=tree_walk_left_root_right(element->left,action,
					  argument)) == 0 &&
	(error=(*action)(element_key(element), element->count, argument)) == 0)
      error=tree_walk_left_root_right(element->right,action,argument);
    return error;
  }

  return 0;
}

int Tree::tree_walk_right_root_left(Tree_Element *element, tree_walk_action action, void *argument)
{
  int error;
  if (element->right)				/* Not null_element */
  {
    if ((error=tree_walk_right_root_left(element->right,action,
					  argument)) == 0 &&
	(error=(*action)(element_key(element),
			  element->count,
			  argument)) == 0)
     error=tree_walk_right_root_left(element->left,action,argument);
    return error;
  }

  return 0;
}

void Tree::left_rotate(Tree_Element **parent, Tree_Element *element)
{
  Tree_Element *y;

  y= element->right;
  element->right= y->left;
  parent[0]= y;
  y->left= element;
}

void Tree::right_rotate(Tree_Element **parent, Tree_Element *element)
{
	Tree_Element *x;

  x= element->left;
  element->left= x->right;
  parent[0]= x;
  x->right= element;
}

void Tree::rb_insert(Tree_Element ***parent, Tree_Element *element)
{
	Tree_Element *y,*par,*par2;

  element->colour=RED;
  while (element != root && (par=parent[-1][0])->colour == RED)
  {
    if (par == (par2=parent[-2][0])->left)
    {
      y= par2->right;
      if (y->colour == RED)
      {
	par->colour= BLACK;
	y->colour= BLACK;
	element= par2;
	parent-= 2;
	element->colour= RED;		/* And the loop continues */
      }
      else
      {
	if (element == par->right)
	{
	  left_rotate(parent[-1],par);
	  par= element;			/* element is now parent to old element */
	}
	par->colour= BLACK;
	par2->colour= RED;
	right_rotate(parent[-2],par2);
	break;
      }
    }
    else
    {
      y= par2->left;
      if (y->colour == RED)
      {
	par->colour= BLACK;
	y->colour= BLACK;
	element= par2;
	parent-= 2;
	element->colour= RED;		/* And the loop continues */
      }
      else
      {
	if (element == par->left)
	{
	  right_rotate(parent[-1],par);
	  par= element;
	}
	par->colour= BLACK;
	par2->colour= RED;
	left_rotate(parent[-2],par2);
	break;
      }
    }
  }
  root->colour=BLACK;
}

} /* namespace drizzled */
