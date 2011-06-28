/* -*- mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; -*-
 *  vim:expandtab:shiftwidth=2:tabstop=2:smarttab:
 *
 *  Copyright (C) 2008-2009 Sun Microsystems, Inc.
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

#include <config.h>
#include <drizzled/optimizer/range.h>
#include <drizzled/optimizer/range_param.h>
#include <drizzled/optimizer/sel_arg.h>
#include <drizzled/util/test.h>

namespace drizzled {

/* Functions to fix up the tree after insert and delete */
static void left_rotate(optimizer::SEL_ARG **root, optimizer::SEL_ARG *leaf)
{
  optimizer::SEL_ARG *y= leaf->right;
  leaf->right= y->left;
  if (y->left != &optimizer::null_element)
    y->left->parent= leaf;
  if (! (y->parent=leaf->parent))
    *root= y;
  else
    *leaf->parent_ptr()= y;
  y->left= leaf;
  leaf->parent= y;
}


static void right_rotate(optimizer::SEL_ARG **root, optimizer::SEL_ARG *leaf)
{
  optimizer::SEL_ARG *y= leaf->left;
  leaf->left= y->right;
  if (y->right != &optimizer::null_element)
    y->right->parent= leaf;
  if (! (y->parent=leaf->parent))
    *root= y;
  else
    *leaf->parent_ptr()= y;
  y->right= leaf;
  leaf->parent= y;
}


/* Get overlapping range */
optimizer::SEL_ARG *optimizer::SEL_ARG::clone_and(optimizer::SEL_ARG *arg)
{
  unsigned char *new_min= NULL;
  unsigned char *new_max= NULL;
  uint8_t flag_min= 0;
  uint8_t flag_max= 0;

  if (cmp_min_to_min(arg) >= 0)
  {
    new_min= min_value;
    flag_min= min_flag;
  }
  else
  {
    new_min=arg->min_value;
    flag_min=arg->min_flag;
  }
  if (cmp_max_to_max(arg) <= 0)
  {
    new_max= max_value;
    flag_max= max_flag;
  }
  else
  {
    new_max= arg->max_value;
    flag_max= arg->max_flag;
  }
  return new SEL_ARG(field, part, new_min, new_max, flag_min, flag_max, test(maybe_flag && arg->maybe_flag));
}


/* min <= X , arg->min */
optimizer::SEL_ARG *optimizer::SEL_ARG::clone_first(optimizer::SEL_ARG *arg)
{
  return new SEL_ARG(field,part, min_value, arg->min_value, min_flag, arg->min_flag & NEAR_MIN ? 0 : NEAR_MAX, maybe_flag | arg->maybe_flag);
}


/* min <= X <= key_max */
optimizer::SEL_ARG *optimizer::SEL_ARG::clone_last(optimizer::SEL_ARG *arg)
{
  return new SEL_ARG(field, part, min_value, arg->max_value, min_flag, arg->max_flag, maybe_flag | arg->maybe_flag);
}


/* Get overlapping range */
bool optimizer::SEL_ARG::copy_min(optimizer::SEL_ARG *arg)
{
  if (cmp_min_to_min(arg) > 0)
  {
    min_value= arg->min_value;
    min_flag=arg->min_flag;
    if ((max_flag & (NO_MAX_RANGE | NO_MIN_RANGE)) ==
        (NO_MAX_RANGE | NO_MIN_RANGE))
    {
      return 1;	// Full range
    }
  }
  maybe_flag|= arg->maybe_flag;
  return 0;
}

/* Get overlapping range */
bool optimizer::SEL_ARG::copy_max(optimizer::SEL_ARG *arg)
{
  if (cmp_max_to_max(arg) <= 0)
  {
    max_value= arg->max_value;
    max_flag= arg->max_flag;
    if ((max_flag & (NO_MAX_RANGE | NO_MIN_RANGE)) ==
        (NO_MAX_RANGE | NO_MIN_RANGE))
    {
      return 1;	// Full range
    }
  }
  maybe_flag|= arg->maybe_flag;
  return 0;
}

void optimizer::SEL_ARG::copy_min_to_min(optimizer::SEL_ARG *arg)
{
  min_value= arg->min_value;
  min_flag= arg->min_flag;
}


void optimizer::SEL_ARG::copy_min_to_max(optimizer::SEL_ARG *arg)
{
  max_value= arg->min_value;
  max_flag= arg->min_flag & NEAR_MIN ? 0 : NEAR_MAX;
}

void optimizer::SEL_ARG::copy_max_to_min(optimizer::SEL_ARG *arg)
{
  min_value= arg->max_value;
  min_flag= arg->max_flag & NEAR_MAX ? 0 : NEAR_MIN;
}


int optimizer::SEL_ARG::store_min(uint32_t length, unsigned char **min_key, uint32_t min_key_flag)
{
  /* "(kp1 > c1) AND (kp2 OP c2) AND ..." -> (kp1 > c1) */
  if ((! (min_flag & NO_MIN_RANGE) &&
      ! (min_key_flag & (NO_MIN_RANGE | NEAR_MIN))))
  {
    if (maybe_null && *min_value)
    {
      **min_key= 1;
      memset(*min_key+1, 0, length-1);
    }
    else
    {
      memcpy(*min_key,min_value,length);
    }
    (*min_key)+= length;
    return 1;
  }
  return 0;
}


int optimizer::SEL_ARG::store_max(uint32_t length, unsigned char **max_key, uint32_t max_key_flag)
{
  if (! (max_flag & NO_MAX_RANGE) &&
      ! (max_key_flag & (NO_MAX_RANGE | NEAR_MAX)))
  {
    if (maybe_null && *max_value)
    {
      **max_key= 1;
      memset(*max_key + 1, 0, length-1);
    }
    else
    {
      memcpy(*max_key,max_value,length);
    }
    (*max_key)+= length;
    return 1;
  }
  return 0;
}


int optimizer::SEL_ARG::store_min_key(KEY_PART *key, unsigned char **range_key, uint32_t *range_key_flag)
{
  optimizer::SEL_ARG *key_tree= first();
  uint32_t res= key_tree->store_min(key[key_tree->part].store_length,
                                    range_key,
                                    *range_key_flag);
  *range_key_flag|= key_tree->min_flag;

  if (key_tree->next_key_part &&
      key_tree->next_key_part->part == key_tree->part+1 &&
      ! (*range_key_flag & (NO_MIN_RANGE | NEAR_MIN)) &&
      key_tree->next_key_part->type == optimizer::SEL_ARG::KEY_RANGE)
  {
    res+= key_tree->next_key_part->store_min_key(key,
                                                 range_key,
                                                 range_key_flag);
  }
  return res;
}

int optimizer::SEL_ARG::store_max_key(KEY_PART *key, unsigned char **range_key, uint32_t *range_key_flag)
{
  SEL_ARG *key_tree= last();
  uint32_t res= key_tree->store_max(key[key_tree->part].store_length,
                                    range_key,
                                    *range_key_flag);
  (*range_key_flag)|= key_tree->max_flag;
  if (key_tree->next_key_part &&
      key_tree->next_key_part->part == key_tree->part+1 &&
      ! (*range_key_flag & (NO_MAX_RANGE | NEAR_MAX)) &&
      key_tree->next_key_part->type == SEL_ARG::KEY_RANGE)
    res+= key_tree->next_key_part->store_max_key(key,
                                                 range_key,
                                                 range_key_flag);
  return res;
}

optimizer::SEL_ARG::SEL_ARG(optimizer::SEL_ARG &arg)
  :
    memory::SqlAlloc()
{
  type= arg.type;
  min_flag= arg.min_flag;
  max_flag= arg.max_flag;
  maybe_flag= arg.maybe_flag;
  maybe_null= arg.maybe_null;
  part= arg.part;
  field= arg.field;
  min_value= arg.min_value;
  max_value= arg.max_value;
  next_key_part= arg.next_key_part;
  use_count=1;
  elements=1;
}


void optimizer::SEL_ARG::make_root()
{
  left= right= &optimizer::null_element;
  color= BLACK;
  next= prev =0;
  use_count= 0;
  elements= 1;
}

optimizer::SEL_ARG::SEL_ARG(Field *f,
                            const unsigned char *min_value_arg,
                            const unsigned char *max_value_arg)
  :
    min_flag(0),
    max_flag(0),
    maybe_flag(0),
    maybe_null(f->real_maybe_null()),
    elements(1),
    use_count(1),
    field(f),
    min_value((unsigned char*) min_value_arg),
    max_value((unsigned char*) max_value_arg),
    next(0),
    prev(0),
    next_key_part(0),
    color(BLACK),
    type(KEY_RANGE)
{
  left= right= &optimizer::null_element;
}

optimizer::SEL_ARG::SEL_ARG(Field *field_,
                            uint8_t part_,
                            unsigned char *min_value_,
                            unsigned char *max_value_,
		            uint8_t min_flag_,
                            uint8_t max_flag_,
                            uint8_t maybe_flag_)
  :
    min_flag(min_flag_),
    max_flag(max_flag_),
    maybe_flag(maybe_flag_),
    part(part_),
    maybe_null(field_->real_maybe_null()),
    elements(1),
    use_count(1),
    field(field_),
    min_value(min_value_),
    max_value(max_value_),
    next(0),
    prev(0),
    next_key_part(0),
    color(BLACK),
    type(KEY_RANGE)
{
  left= right= &optimizer::null_element;
}

optimizer::SEL_ARG *optimizer::SEL_ARG::clone(RangeParameter *param, optimizer::SEL_ARG *new_parent, optimizer::SEL_ARG **next_arg)
{
  optimizer::SEL_ARG *tmp= NULL;

  /* Bail out if we have already generated too many SEL_ARGs */
  if (++param->alloced_sel_args > MAX_SEL_ARGS)
    return 0;

  if (type != KEY_RANGE)
  {
    tmp= new (*param->mem_root) optimizer::SEL_ARG(type);
    tmp->prev= *next_arg; // Link into next/prev chain
    (*next_arg)->next= tmp;
    (*next_arg)= tmp;
  }
  else
  {
    tmp= new (*param->mem_root) optimizer::SEL_ARG(field, part, min_value, max_value, min_flag, max_flag, maybe_flag);
    tmp->parent= new_parent;
    tmp->next_key_part= next_key_part;
    if (left != &optimizer::null_element)
      if (! (tmp->left= left->clone(param, tmp, next_arg)))
	return 0; // OOM

    tmp->prev= *next_arg; // Link into next/prev chain
    (*next_arg)->next= tmp;
    (*next_arg)= tmp;

    if (right != &optimizer::null_element)
      if (! (tmp->right= right->clone(param, tmp, next_arg)))
	return 0; // OOM
  }
  increment_use_count(1);
  tmp->color= color;
  tmp->elements= this->elements;
  return tmp;
}

optimizer::SEL_ARG *optimizer::SEL_ARG::first()
{
  optimizer::SEL_ARG *next_arg= this;
  if (! next_arg->left)
    return 0; // MAYBE_KEY
  while (next_arg->left != &optimizer::null_element)
    next_arg= next_arg->left;
  return next_arg;
}

optimizer::SEL_ARG *optimizer::SEL_ARG::last()
{
  SEL_ARG *next_arg= this;
  if (! next_arg->right)
    return 0; // MAYBE_KEY
  while (next_arg->right != &optimizer::null_element)
    next_arg=next_arg->right;
  return next_arg;
}


optimizer::SEL_ARG *optimizer::SEL_ARG::clone_tree(RangeParameter *param)
{
  optimizer::SEL_ARG tmp_link;
  optimizer::SEL_ARG* next_arg= NULL;
  next_arg= &tmp_link;
  optimizer::SEL_ARG* root= clone(param, (SEL_ARG *) 0, &next_arg);
  if (not root)
    return 0;
  next_arg->next= 0; // Fix last link
  tmp_link.next->prev= 0; // Fix first link
  if (root) // If not OOM
    root->use_count= 0;
  return root;
}


optimizer::SEL_ARG *
optimizer::SEL_ARG::insert(optimizer::SEL_ARG *key)
{
  optimizer::SEL_ARG *element= NULL;
  optimizer::SEL_ARG **par= NULL;
  optimizer::SEL_ARG *last_element= NULL;

  for (element= this; element != &optimizer::null_element; )
  {
    last_element= element;
    if (key->cmp_min_to_min(element) > 0)
    {
      par= &element->right; element= element->right;
    }
    else
    {
      par= &element->left; element= element->left;
    }
  }
  *par= key;
  key->parent= last_element;
	/* Link in list */
  if (par == &last_element->left)
  {
    key->next= last_element;
    if ((key->prev=last_element->prev))
      key->prev->next= key;
    last_element->prev= key;
  }
  else
  {
    if ((key->next= last_element->next))
      key->next->prev= key;
    key->prev= last_element;
    last_element->next= key;
  }
  key->left= key->right= &optimizer::null_element;
  optimizer::SEL_ARG *root= rb_insert(key); // rebalance tree
  root->use_count= this->use_count;		// copy root info
  root->elements= this->elements+1;
  root->maybe_flag= this->maybe_flag;
  return root;
}


/*
** Find best key with min <= given key
** Because the call context this should never return 0 to get_range
*/
optimizer::SEL_ARG *
optimizer::SEL_ARG::find_range(optimizer::SEL_ARG *key)
{
  optimizer::SEL_ARG *element= this;
  optimizer::SEL_ARG *found= NULL;

  for (;;)
  {
    if (element == &optimizer::null_element)
      return found;
    int cmp= element->cmp_min_to_min(key);
    if (cmp == 0)
      return element;
    if (cmp < 0)
    {
      found= element;
      element= element->right;
    }
    else
      element= element->left;
  }
}


/*
  Remove a element from the tree

  SYNOPSIS
    tree_delete()
    key		Key that is to be deleted from tree (this)

  NOTE
    This also frees all sub trees that is used by the element

  RETURN
    root of new tree (with key deleted)
*/
optimizer::SEL_ARG *
optimizer::SEL_ARG::tree_delete(optimizer::SEL_ARG *key)
{
  enum leaf_color remove_color;
  optimizer::SEL_ARG *root= NULL;
  optimizer::SEL_ARG *nod= NULL;
  optimizer::SEL_ARG **par= NULL;
  optimizer::SEL_ARG *fix_par= NULL;

  root= this;
  this->parent= 0;

  /* Unlink from list */
  if (key->prev)
    key->prev->next= key->next;
  if (key->next)
    key->next->prev= key->prev;
  key->increment_use_count(-1);
  if (! key->parent)
    par= &root;
  else
    par= key->parent_ptr();

  if (key->left == &optimizer::null_element)
  {
    *par= nod= key->right;
    fix_par= key->parent;
    if (nod != &optimizer::null_element)
      nod->parent= fix_par;
    remove_color= key->color;
  }
  else if (key->right == &optimizer::null_element)
  {
    *par= nod= key->left;
    nod->parent= fix_par= key->parent;
    remove_color= key->color;
  }
  else
  {
    optimizer::SEL_ARG *tmp= key->next; // next bigger key (exist!)
    nod= *tmp->parent_ptr()= tmp->right;	// unlink tmp from tree
    fix_par= tmp->parent;
    if (nod != &optimizer::null_element)
      nod->parent= fix_par;
    remove_color= tmp->color;

    tmp->parent= key->parent;			// Move node in place of key
    (tmp->left= key->left)->parent= tmp;
    if ((tmp->right=key->right) != &optimizer::null_element)
      tmp->right->parent= tmp;
    tmp->color= key->color;
    *par= tmp;
    if (fix_par == key)				// key->right == key->next
      fix_par= tmp;				// new parent of nod
  }

  if (root == &optimizer::null_element)
    return 0;				// Maybe root later
  if (remove_color == BLACK)
    root= rb_delete_fixup(root, nod, fix_par);

  root->use_count= this->use_count;		// Fix root counters
  root->elements= this->elements-1;
  root->maybe_flag= this->maybe_flag;
  return root;
}


optimizer::SEL_ARG *
optimizer::SEL_ARG::rb_insert(optimizer::SEL_ARG *leaf)
{
  optimizer::SEL_ARG *y= NULL;
  optimizer::SEL_ARG *par= NULL;
  optimizer::SEL_ARG *par2= NULL;
  optimizer::SEL_ARG *root= NULL;

  root= this;
  root->parent= 0;

  leaf->color= RED;
  while (leaf != root && (par= leaf->parent)->color == RED)
  {					// This can't be root or 1 level under
    if (par == (par2= leaf->parent->parent)->left)
    {
      y= par2->right;
      if (y->color == RED)
      {
        par->color= BLACK;
        y->color= BLACK;
        leaf= par2;
        leaf->color= RED;		/* And the loop continues */
      }
      else
      {
        if (leaf == par->right)
        {
          left_rotate(&root,leaf->parent);
          par= leaf; /* leaf is now parent to old leaf */
        }
        par->color= BLACK;
        par2->color= RED;
        right_rotate(&root, par2);
        break;
      }
    }
    else
    {
      y= par2->left;
      if (y->color == RED)
      {
        par->color= BLACK;
        y->color= BLACK;
        leaf= par2;
        leaf->color= RED;		/* And the loop continues */
      }
      else
      {
        if (leaf == par->left)
        {
          right_rotate(&root,par);
          par= leaf;
        }
        par->color= BLACK;
        par2->color= RED;
        left_rotate(&root, par2);
        break;
      }
    }
  }
  root->color= BLACK;

  return root;
}


optimizer::SEL_ARG *optimizer::rb_delete_fixup(optimizer::SEL_ARG *root,
                                               optimizer::SEL_ARG *key,
                                               optimizer::SEL_ARG *par)
{
  optimizer::SEL_ARG *x= NULL;
  optimizer::SEL_ARG *w= NULL;
  root->parent= 0;

  x= key;
  while (x != root && x->color == optimizer::SEL_ARG::BLACK)
  {
    if (x == par->left)
    {
      w= par->right;
      if (w->color == optimizer::SEL_ARG::RED)
      {
        w->color= optimizer::SEL_ARG::BLACK;
        par->color= optimizer::SEL_ARG::RED;
        left_rotate(&root, par);
        w= par->right;
      }
      if (w->left->color == optimizer::SEL_ARG::BLACK &&
          w->right->color == optimizer::SEL_ARG::BLACK)
      {
        w->color= optimizer::SEL_ARG::RED;
        x= par;
      }
      else
      {
        if (w->right->color == optimizer::SEL_ARG::BLACK)
        {
          w->left->color= optimizer::SEL_ARG::BLACK;
          w->color= optimizer::SEL_ARG::RED;
          right_rotate(&root, w);
          w= par->right;
        }
        w->color= par->color;
        par->color= optimizer::SEL_ARG::BLACK;
        w->right->color= optimizer::SEL_ARG::BLACK;
        left_rotate(&root, par);
        x= root;
        break;
      }
    }
    else
    {
      w= par->left;
      if (w->color == optimizer::SEL_ARG::RED)
      {
        w->color= optimizer::SEL_ARG::BLACK;
        par->color= optimizer::SEL_ARG::RED;
        right_rotate(&root, par);
        w= par->left;
      }
      if (w->right->color == optimizer::SEL_ARG::BLACK &&
          w->left->color == optimizer::SEL_ARG::BLACK)
      {
        w->color= optimizer::SEL_ARG::RED;
        x= par;
      }
      else
      {
        if (w->left->color == SEL_ARG::BLACK)
        {
          w->right->color= optimizer::SEL_ARG::BLACK;
          w->color= optimizer::SEL_ARG::RED;
          left_rotate(&root, w);
          w= par->left;
        }
        w->color= par->color;
        par->color= optimizer::SEL_ARG::BLACK;
        w->left->color= optimizer::SEL_ARG::BLACK;
        right_rotate(&root, par);
        x= root;
        break;
      }
    }
    par= x->parent;
  }
  x->color= optimizer::SEL_ARG::BLACK;
  return root;
}


} /* namespace drizzled */
