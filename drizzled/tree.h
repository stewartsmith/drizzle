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

#pragma once

#include <unistd.h>

#include <drizzled/base.h>		/* get 'enum ha_rkey_function' */
#include <drizzled/qsort_cmp.h>
#include <drizzled/memory/root.h>

namespace drizzled
{

/* Worst case tree is half full. This gives use 2^(MAX_TREE_HEIGHT/2) leafs */
static const int MAX_TREE_HEIGHT= 64;

static const int TREE_NO_DUPS= 1;

typedef enum { left_root_right, right_root_left } TREE_WALK;
typedef int (*tree_walk_action)(void *,uint32_t,void *);

typedef enum { free_init, free_free, free_end } TREE_FREE;
typedef void (*tree_element_free)(void*, TREE_FREE, void *);

/*
typedef struct st_tree_element {
  struct st_tree_element *left,*right;
  uint32_t count:31,
	 colour:1;			// black is marked as 1
} TREE_ELEMENT;


typedef struct st_tree {
  TREE_ELEMENT *root,null_element;
  TREE_ELEMENT **parents[MAX_TREE_HEIGHT];
  uint32_t offset_to_key, elements_in_tree, size_of_element;
  size_t memory_limit;
  size_t allocated;
  qsort_cmp2 compare;
  void *custom_arg;
  memory::Root mem_root;
  bool with_delete;  // directly free leaf memory?
  tree_element_free free;  // leaf deletion callback
  uint32_t flag;
} TREE;

// Functions on whole tree
void init_tree(TREE *tree, size_t default_alloc_size, uint32_t memory_limit,
               uint32_t size, qsort_cmp2 compare, bool with_delete,
	       tree_element_free free_element, void *custom_arg);
void delete_tree(TREE*);
void reset_tree(TREE*);

//
//  similar to delete tree, except we do not free() blocks in mem_root
//
#define is_tree_inited(tree) ((tree)->root != 0)

// Functions on leaves

TREE_ELEMENT *tree_insert(TREE *tree,void *key, uint32_t key_size,
                          void *custom_arg);
int tree_walk(TREE *tree,tree_walk_action action,
	      void *argument, TREE_WALK visit);
*/
class Tree_Element
{
public:
	Tree_Element *left,*right;
	uint32_t count:31,
		 	 colour:1;			/* black is marked as 1 */
};

static const int TREE_ELEMENT_EXTRA_SIZE= (sizeof(Tree_Element) + sizeof(void*));

class Tree
{
public:
	Tree_Element *root, null_element;
	Tree_Element **parents[MAX_TREE_HEIGHT];
	uint32_t offset_to_key, elements_in_tree, size_of_element;
	size_t memory_limit;
	size_t allocated;
	qsort_cmp2 compare;
	void *custom_arg;
	memory::Root mem_root;
	bool with_delete;
	tree_element_free free;
	uint32_t flag;

	// tree methods
	void init_tree(size_t default_alloc_size, uint32_t memory_limit,
				   uint32_t size, qsort_cmp2 compare, bool with_delete,
				   tree_element_free free_element, void *custom_arg);
	bool is_inited()
	{
		return this->root != 0;
	}
	void delete_tree();
	void reset_tree();

	// leaf methods
	Tree_Element *tree_insert(void *key, uint32_t key_size, void *custom_arg);
	int tree_walk(tree_walk_action action, void *argument, TREE_WALK visit);
	// TODO: rewrite static .cc functions into private methods
private:
	void delete_tree_element(Tree_Element *);
	int tree_walk_left_root_right(Tree_Element *, tree_walk_action,void *);
	int tree_walk_right_root_left(Tree_Element *, tree_walk_action,void *);
	void left_rotate(Tree_Element **parent,Tree_Element *leaf);
	void right_rotate(Tree_Element **parent, Tree_Element *leaf);
	void rb_insert(Tree_Element ***parent, Tree_Element *leaf);
	void free_tree(myf free_flags);

};

} /* namespace drizzled */

