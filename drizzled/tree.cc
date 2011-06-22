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
  key in tree is allocated accrding to following:

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
     allign) then key will be put on a 8 alligned adress. Else
     the key will be on adress (element+1). This is transparent for user
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
    (*tree->compare)(custom_arg, ELEMENT_KEY(tree,element), key)
  and not other way around, as
    (*tree->compare)(custom_arg, key, ELEMENT_KEY(tree,element))
*/

#include <config.h>

#include <drizzled/tree.h>
#include <drizzled/internal/my_sys.h>
#include <drizzled/internal/m_string.h>
#include <drizzled/memory/root.h>

#define BLACK		1
#define RED		0
#define DEFAULT_ALLOC_SIZE 8192
#define DEFAULT_ALIGN_SIZE 8192

#define ELEMENT_KEY(tree,element)\
(tree->offset_to_key ? (void*)((unsigned char*) element+tree->offset_to_key) :\
			*((void**) (element+1)))
#define ELEMENT_CHILD(element, offs) (*(TREE_ELEMENT**)((char*)element + offs))

namespace drizzled
{


static void delete_tree_element(TREE *,TREE_ELEMENT *);
static int tree_walk_left_root_right(TREE *,TREE_ELEMENT *,
				     tree_walk_action,void *);
static int tree_walk_right_root_left(TREE *,TREE_ELEMENT *,
				     tree_walk_action,void *);
static void left_rotate(TREE_ELEMENT **parent,TREE_ELEMENT *leaf);
static void right_rotate(TREE_ELEMENT **parent, TREE_ELEMENT *leaf);
static void rb_insert(TREE *tree,TREE_ELEMENT ***parent,
		      TREE_ELEMENT *leaf);


void init_tree(TREE *tree, size_t default_alloc_size, uint32_t memory_limit,
               uint32_t size, qsort_cmp2 compare, bool with_delete,
	       tree_element_free free_element, void *custom_arg)
{
  if (default_alloc_size < DEFAULT_ALLOC_SIZE)
    default_alloc_size= DEFAULT_ALLOC_SIZE;
  default_alloc_size= MY_ALIGN(default_alloc_size, DEFAULT_ALIGN_SIZE);
  memset(&tree->null_element, 0, sizeof(tree->null_element));
  tree->root= &tree->null_element;
  tree->compare= compare;
  tree->size_of_element= size > 0 ? (uint32_t) size : 0;
  tree->memory_limit= memory_limit;
  tree->free= free_element;
  tree->allocated= 0;
  tree->elements_in_tree= 0;
  tree->custom_arg = custom_arg;
  tree->null_element.colour= BLACK;
  tree->null_element.left=tree->null_element.right= 0;
  tree->flag= 0;
  if (!free_element &&
      (size <= sizeof(void*) || ((uint32_t) size & (sizeof(void*)-1))))
  {
    /*
      We know that the data doesn't have to be aligned (like if the key
      contains a double), so we can store the data combined with the
      TREE_ELEMENT.
    */
    tree->offset_to_key= sizeof(TREE_ELEMENT); /* Put key after element */
    /* Fix allocation size so that we don't lose any memory */
    default_alloc_size/= (sizeof(TREE_ELEMENT)+size);
    if (!default_alloc_size)
      default_alloc_size= 1;
    default_alloc_size*= (sizeof(TREE_ELEMENT)+size);
  }
  else
  {
    tree->offset_to_key= 0;		/* use key through pointer */
    tree->size_of_element+= sizeof(void*);
  }
  if (! (tree->with_delete= with_delete))
  {
    tree->mem_root.init(default_alloc_size);
    tree->mem_root.min_malloc= (sizeof(TREE_ELEMENT)+tree->size_of_element);
  }
}

static void free_tree(TREE *tree, myf free_flags)
{
  if (tree->root)				/* If initialized */
  {
    if (tree->with_delete)
      delete_tree_element(tree,tree->root);
    else
    {
      if (tree->free)
      {
        if (tree->memory_limit)
          (*tree->free)(NULL, free_init, tree->custom_arg);
	delete_tree_element(tree,tree->root);
        if (tree->memory_limit)
          (*tree->free)(NULL, free_end, tree->custom_arg);
      }
      tree->mem_root.free_root(free_flags);
    }
  }
  tree->root= &tree->null_element;
  tree->elements_in_tree= 0;
  tree->allocated= 0;
}

void delete_tree(TREE* tree)
{
  free_tree(tree, MYF(0)); /* free() mem_root if applicable */
}

void reset_tree(TREE* tree)
{
  /* do not free mem_root, just mark blocks as free */
  free_tree(tree, MYF(memory::MARK_BLOCKS_FREE));
}


static void delete_tree_element(TREE *tree, TREE_ELEMENT *element)
{
  if (element != &tree->null_element)
  {
    delete_tree_element(tree,element->left);
    if (tree->free)
      (*tree->free)(ELEMENT_KEY(tree,element), free_free, tree->custom_arg);
    delete_tree_element(tree,element->right);
    if (tree->with_delete)
      free((char*) element);
  }
}


/*
  insert, search and delete of elements

  The following should be true:
    parent[0] = & parent[-1][0]->left ||
    parent[0] = & parent[-1][0]->right
*/

TREE_ELEMENT *tree_insert(TREE *tree, void *key, uint32_t key_size,
                          void* custom_arg)
{
  int cmp;
  TREE_ELEMENT *element,***parent;

  parent= tree->parents;
  *parent = &tree->root; element= tree->root;
  for (;;)
  {
    if (element == &tree->null_element ||
	(cmp = (*tree->compare)(custom_arg, ELEMENT_KEY(tree,element),
                                key)) == 0)
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
  if (element == &tree->null_element)
  {
    size_t alloc_size= sizeof(TREE_ELEMENT)+key_size+tree->size_of_element;
    tree->allocated+= alloc_size;

    if (tree->memory_limit && tree->elements_in_tree
                           && tree->allocated > tree->memory_limit)
    {
      reset_tree(tree);
      return tree_insert(tree, key, key_size, custom_arg);
    }

    key_size+= tree->size_of_element;
    if (tree->with_delete)
      element= (TREE_ELEMENT *) malloc(alloc_size);
    else
      element= (TREE_ELEMENT *) tree->mem_root.alloc(alloc_size);
    **parent= element;
    element->left= element->right= &tree->null_element;
    if (!tree->offset_to_key)
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
      memcpy((unsigned char*) element + tree->offset_to_key, key, key_size);
    element->count= 1;			/* May give warning in purify */
    tree->elements_in_tree++;
    rb_insert(tree,parent,element);	/* rebalance tree */
  }
  else
  {
    if (tree->flag & TREE_NO_DUPS)
      return(NULL);
    element->count++;
    /* Avoid a wrap over of the count. */
    if (! element->count)
      element->count--;
  }

  return element;
}

void *tree_search_key(TREE *tree, const void *key,
                      TREE_ELEMENT **parents, TREE_ELEMENT ***last_pos,
                      enum ha_rkey_function flag, void *custom_arg)
{
  TREE_ELEMENT *element= tree->root;
  TREE_ELEMENT **last_left_step_parent= NULL, **last_right_step_parent= NULL;
  TREE_ELEMENT **last_equal_element= NULL;

/*
  TODO: support for HA_READ_KEY_OR_PREV, HA_READ_PREFIX flags if needed.
*/

  *parents = &tree->null_element;
  while (element != &tree->null_element)
  {
    int cmp;

    *++parents= element;

    if ((cmp= (*tree->compare)(custom_arg, ELEMENT_KEY(tree, element),
			       key)) == 0)
    {
      switch (flag) {
      case HA_READ_KEY_EXACT:
      case HA_READ_KEY_OR_NEXT:
      case HA_READ_BEFORE_KEY:
	last_equal_element= parents;
	cmp= 1;
	break;
      case HA_READ_AFTER_KEY:
	cmp= -1;
	break;
      case HA_READ_PREFIX_LAST:
      case HA_READ_PREFIX_LAST_OR_PREV:
	last_equal_element= parents;
	cmp= -1;
	break;
      default:
	return NULL;
      }
    }
    if (cmp < 0) /* element < key */
    {
      last_right_step_parent= parents;
      element= element->right;
    }
    else
    {
      last_left_step_parent= parents;
      element= element->left;
    }
  }
  switch (flag) {
  case HA_READ_KEY_EXACT:
  case HA_READ_PREFIX_LAST:
    *last_pos= last_equal_element;
    break;
  case HA_READ_KEY_OR_NEXT:
    *last_pos= last_equal_element ? last_equal_element : last_left_step_parent;
    break;
  case HA_READ_AFTER_KEY:
    *last_pos= last_left_step_parent;
    break;
  case HA_READ_PREFIX_LAST_OR_PREV:
    *last_pos= last_equal_element ? last_equal_element : last_right_step_parent;
    break;
  case HA_READ_BEFORE_KEY:
    *last_pos= last_right_step_parent;
    break;
  default:
    return NULL;
  }

  return *last_pos ? ELEMENT_KEY(tree, **last_pos) : NULL;
}

void *tree_search_next(TREE *tree, TREE_ELEMENT ***last_pos, int l_offs,
                       int r_offs)
{
  TREE_ELEMENT *x= **last_pos;

  if (ELEMENT_CHILD(x, r_offs) != &tree->null_element)
  {
    x= ELEMENT_CHILD(x, r_offs);
    *++*last_pos= x;
    while (ELEMENT_CHILD(x, l_offs) != &tree->null_element)
    {
      x= ELEMENT_CHILD(x, l_offs);
      *++*last_pos= x;
    }
    return ELEMENT_KEY(tree, x);
  }
  else
  {
    TREE_ELEMENT *y= *--*last_pos;
    while (y != &tree->null_element && x == ELEMENT_CHILD(y, r_offs))
    {
      x= y;
      y= *--*last_pos;
    }
    return y == &tree->null_element ? NULL : ELEMENT_KEY(tree, y);
  }
}

/*
  Expected that tree is fully balanced
  (each path from root to leaf has the same length)
*/
ha_rows tree_record_pos(TREE *tree, const void *key,
			enum ha_rkey_function flag, void *custom_arg)
{
  TREE_ELEMENT *element= tree->root;
  double left= 1;
  double right= tree->elements_in_tree;

  while (element != &tree->null_element)
  {
    int cmp;

    if ((cmp= (*tree->compare)(custom_arg, ELEMENT_KEY(tree, element),
			       key)) == 0)
    {
      switch (flag) {
      case HA_READ_KEY_EXACT:
      case HA_READ_BEFORE_KEY:
        cmp= 1;
        break;
      case HA_READ_AFTER_KEY:
        cmp= -1;
        break;
      default:
        return HA_POS_ERROR;
      }
    }
    if (cmp < 0) /* element < key */
    {
      element= element->right;
      left= (left + right) / 2;
    }
    else
    {
      element= element->left;
      right= (left + right) / 2;
    }
  }

  switch (flag) {
  case HA_READ_KEY_EXACT:
  case HA_READ_BEFORE_KEY:
    return (ha_rows) right;
  case HA_READ_AFTER_KEY:
    return (ha_rows) left;
  default:
    return HA_POS_ERROR;
  }
}

int tree_walk(TREE *tree, tree_walk_action action, void *argument, TREE_WALK visit)
{
  switch (visit) {
  case left_root_right:
    return tree_walk_left_root_right(tree,tree->root,action,argument);
  case right_root_left:
    return tree_walk_right_root_left(tree,tree->root,action,argument);
  }

  return 0;			/* Keep gcc happy */
}

static int tree_walk_left_root_right(TREE *tree, TREE_ELEMENT *element, tree_walk_action action, void *argument)
{
  int error;
  if (element->left)				/* Not null_element */
  {
    if ((error=tree_walk_left_root_right(tree,element->left,action,
					  argument)) == 0 &&
	(error=(*action)(ELEMENT_KEY(tree,element),
			  element->count,
			  argument)) == 0)
      error=tree_walk_left_root_right(tree,element->right,action,argument);
    return error;
  }

  return 0;
}

static int tree_walk_right_root_left(TREE *tree, TREE_ELEMENT *element, tree_walk_action action, void *argument)
{
  int error;
  if (element->right)				/* Not null_element */
  {
    if ((error=tree_walk_right_root_left(tree,element->right,action,
					  argument)) == 0 &&
	(error=(*action)(ELEMENT_KEY(tree,element),
			  element->count,
			  argument)) == 0)
     error=tree_walk_right_root_left(tree,element->left,action,argument);
    return error;
  }

  return 0;
}


/* Functions to fix up the tree after insert and delete */

static void left_rotate(TREE_ELEMENT **parent, TREE_ELEMENT *leaf)
{
  TREE_ELEMENT *y;

  y= leaf->right;
  leaf->right= y->left;
  parent[0]= y;
  y->left= leaf;
}

static void right_rotate(TREE_ELEMENT **parent, TREE_ELEMENT *leaf)
{
  TREE_ELEMENT *x;

  x= leaf->left;
  leaf->left= x->right;
  parent[0]= x;
  x->right= leaf;
}

static void rb_insert(TREE *tree, TREE_ELEMENT ***parent, TREE_ELEMENT *leaf)
{
  TREE_ELEMENT *y,*par,*par2;

  leaf->colour=RED;
  while (leaf != tree->root && (par=parent[-1][0])->colour == RED)
  {
    if (par == (par2=parent[-2][0])->left)
    {
      y= par2->right;
      if (y->colour == RED)
      {
	par->colour= BLACK;
	y->colour= BLACK;
	leaf= par2;
	parent-= 2;
	leaf->colour= RED;		/* And the loop continues */
      }
      else
      {
	if (leaf == par->right)
	{
	  left_rotate(parent[-1],par);
	  par= leaf;			/* leaf is now parent to old leaf */
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
	leaf= par2;
	parent-= 2;
	leaf->colour= RED;		/* And the loop continues */
      }
      else
      {
	if (leaf == par->left)
	{
	  right_rotate(parent[-1],par);
	  par= leaf;
	}
	par->colour= BLACK;
	par2->colour= RED;
	left_rotate(parent[-2],par2);
	break;
      }
    }
  }
  tree->root->colour=BLACK;
}

} /* namespace drizzled */
