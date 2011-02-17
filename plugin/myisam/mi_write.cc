/* Copyright (C) 2000-2006 MySQL AB

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

/* Write a row to a MyISAM table */

#include "myisam_priv.h"

#include <drizzled/internal/m_string.h>
#include <drizzled/util/test.h>

using namespace drizzled;

#define MAX_POINTER_LENGTH 8

	/* Functions declared in this file */

static int w_search(MI_INFO *info,MI_KEYDEF *keyinfo,
		    uint32_t comp_flag, unsigned char *key,
		    uint32_t key_length, internal::my_off_t pos, unsigned char *father_buff,
		    unsigned char *father_keypos, internal::my_off_t father_page,
		    bool insert_last);
static int _mi_balance_page(MI_INFO *info,MI_KEYDEF *keyinfo,unsigned char *key,
			    unsigned char *curr_buff,unsigned char *father_buff,
			    unsigned char *father_keypos,internal::my_off_t father_page);
static unsigned char *_mi_find_last_pos(MI_KEYDEF *keyinfo, unsigned char *page,
				unsigned char *key, uint32_t *return_key_length,
				unsigned char **after_key);
int _mi_ck_write_tree(register MI_INFO *info, uint32_t keynr,unsigned char *key,
		      uint32_t key_length);
int _mi_ck_write_btree(register MI_INFO *info, uint32_t keynr,unsigned char *key,
		       uint32_t key_length);

	/* Write new record to database */

int mi_write(MI_INFO *info, unsigned char *record)
{
  MYISAM_SHARE *share=info->s;
  uint32_t i;
  int save_errno;
  internal::my_off_t filepos;
  unsigned char *buff;
  bool lock_tree= share->concurrent_insert;

  if (share->options & HA_OPTION_READ_ONLY_DATA)
  {
    return(errno=EACCES);
  }
  if (_mi_readinfo(info,F_WRLCK,1))
    return(errno);
  filepos= ((share->state.dellink != HA_OFFSET_ERROR &&
             !info->append_insert_at_end) ?
	    share->state.dellink :
	    info->state->data_file_length);

  if (share->base.reloc == (ha_rows) 1 &&
      share->base.records == (ha_rows) 1 &&
      info->state->records == (ha_rows) 1)
  {						/* System file */
    errno=HA_ERR_RECORD_FILE_FULL;
    goto err2;
  }
  if (info->state->key_file_length >= share->base.margin_key_file_length)
  {
    errno=HA_ERR_INDEX_FILE_FULL;
    goto err2;
  }
  if (_mi_mark_file_changed(info))
    goto err2;

  /* Calculate and check all unique constraints */
  for (i=0 ; i < share->state.header.uniques ; i++)
  {
    if (mi_check_unique(info,share->uniqueinfo+i,record,
		     mi_unique_hash(share->uniqueinfo+i,record),
		     HA_OFFSET_ERROR))
      goto err2;
  }

	/* Write all keys to indextree */

  buff=info->lastkey2;
  for (i=0 ; i < share->base.keys ; i++)
  {
    if (mi_is_key_active(share->state.key_map, i))
    {
      bool local_lock_tree= (lock_tree &&
                                !(info->bulk_insert &&
                                  is_tree_inited(&info->bulk_insert[i])));
      if (local_lock_tree)
      {
	share->keyinfo[i].version++;
      }
      {
        if (share->keyinfo[i].ck_insert(info,i,buff,
			_mi_make_key(info,i,buff,record,filepos)))
        {
          goto err;
        }
      }

      /* The above changed info->lastkey2. Inform mi_rnext_same(). */
      info->update&= ~HA_STATE_RNEXT_SAME;
    }
  }
  if (share->calc_checksum)
    info->checksum=(*share->calc_checksum)(info,record);
  if (!(info->opt_flag & OPT_NO_ROWS))
  {
    if ((*share->write_record)(info,record))
      goto err;
    info->state->checksum+=info->checksum;
  }
  if (share->base.auto_key)
    set_if_bigger(info->s->state.auto_increment,
                  retrieve_auto_increment(info, record));
  info->update= (HA_STATE_CHANGED | HA_STATE_AKTIV | HA_STATE_WRITTEN |
		 HA_STATE_ROW_CHANGED);
  info->state->records++;
  info->lastpos=filepos;
  _mi_writeinfo(info, WRITEINFO_UPDATE_KEYFILE);

  /*
    Update status of the table. We need to do so after each row write
    for the log tables, as we want the new row to become visible to
    other threads as soon as possible. We don't lock mutex here
    (as it is required by pthread memory visibility rules) as (1) it's
    not critical to use outdated share->is_log_table value (2) locking
    mutex here for every write is too expensive.
  */
  if (share->is_log_table) // Log table do not exist in Drizzle
    assert(0);

  return(0);

err:
  save_errno=errno;
  if (errno == HA_ERR_FOUND_DUPP_KEY || errno == HA_ERR_RECORD_FILE_FULL ||
      errno == HA_ERR_NULL_IN_SPATIAL || errno == HA_ERR_OUT_OF_MEM)
  {
    if (info->bulk_insert)
    {
      uint32_t j;
      for (j=0 ; j < share->base.keys ; j++)
        mi_flush_bulk_insert(info, j);
    }
    info->errkey= (int) i;
    while ( i-- > 0)
    {
      if (mi_is_key_active(share->state.key_map, i))
      {
	{
	  uint32_t key_length=_mi_make_key(info,i,buff,record,filepos);
	  if (_mi_ck_delete(info,i,buff,key_length))
	  {
	    break;
	  }
	}
      }
    }
  }
  else
  {
    mi_print_error(info->s, HA_ERR_CRASHED);
    mi_mark_crashed(info);
  }
  info->update= (HA_STATE_CHANGED | HA_STATE_WRITTEN | HA_STATE_ROW_CHANGED);
  errno=save_errno;
err2:
  save_errno=errno;
  _mi_writeinfo(info,WRITEINFO_UPDATE_KEYFILE);
  return(errno=save_errno);
} /* mi_write */


	/* Write one key to btree */

int _mi_ck_write(MI_INFO *info, uint32_t keynr, unsigned char *key, uint32_t key_length)
{
  if (info->bulk_insert && is_tree_inited(&info->bulk_insert[keynr]))
  {
    return(_mi_ck_write_tree(info, keynr, key, key_length));
  }
  else
  {
    return(_mi_ck_write_btree(info, keynr, key, key_length));
  }
} /* _mi_ck_write */


/**********************************************************************
 *                Normal insert code                                  *
 **********************************************************************/

int _mi_ck_write_btree(register MI_INFO *info, uint32_t keynr, unsigned char *key,
		       uint32_t key_length)
{
  uint32_t error;
  uint32_t comp_flag;
  MI_KEYDEF *keyinfo=info->s->keyinfo+keynr;
  internal::my_off_t  *root=&info->s->state.key_root[keynr];

  if (keyinfo->flag & HA_SORT_ALLOWS_SAME)
    comp_flag=SEARCH_BIGGER;			/* Put after same key */
  else if (keyinfo->flag & (HA_NOSAME))
  {
    comp_flag=SEARCH_FIND | SEARCH_UPDATE;	/* No duplicates */
    if (keyinfo->flag & HA_NULL_ARE_EQUAL)
      comp_flag|= SEARCH_NULL_ARE_EQUAL;
  }
  else
    comp_flag=SEARCH_SAME;			/* Keys in rec-pos order */

  error=_mi_ck_real_write_btree(info, keyinfo, key, key_length,
                                root, comp_flag);
  return(error);
} /* _mi_ck_write_btree */

int _mi_ck_real_write_btree(MI_INFO *info, MI_KEYDEF *keyinfo,
    unsigned char *key, uint32_t key_length, internal::my_off_t *root, uint32_t comp_flag)
{
  int error;
  /* key_length parameter is used only if comp_flag is SEARCH_FIND */
  if (*root == HA_OFFSET_ERROR ||
      (error=w_search(info, keyinfo, comp_flag, key, key_length,
		      *root, (unsigned char *) 0, (unsigned char*) 0,
		      (internal::my_off_t) 0, 1)) > 0)
    error=_mi_enlarge_root(info,keyinfo,key,root);
  return(error);
} /* _mi_ck_real_write_btree */


	/* Make a new root with key as only pointer */

int _mi_enlarge_root(MI_INFO *info, MI_KEYDEF *keyinfo, unsigned char *key,
                     internal::my_off_t *root)
{
  uint32_t t_length,nod_flag;
  MI_KEY_PARAM s_temp;
  MYISAM_SHARE *share=info->s;

  nod_flag= (*root != HA_OFFSET_ERROR) ?  share->base.key_reflength : 0;
  _mi_kpointer(info,info->buff+2,*root); /* if nod */
  t_length=(*keyinfo->pack_key)(keyinfo,nod_flag,(unsigned char*) 0,
				(unsigned char*) 0, (unsigned char*) 0, key,&s_temp);
  mi_putint(info->buff,t_length+2+nod_flag,nod_flag);
  (*keyinfo->store_key)(keyinfo,info->buff+2+nod_flag,&s_temp);
  info->buff_used=info->page_changed=1;		/* info->buff is used */
  if ((*root= _mi_new(info,keyinfo,DFLT_INIT_HITS)) == HA_OFFSET_ERROR ||
      _mi_write_keypage(info,keyinfo,*root,DFLT_INIT_HITS,info->buff))
    return(-1);
  return(0);
} /* _mi_enlarge_root */


	/*
	  Search after a position for a key and store it there
	  Returns -1 = error
		   0  = ok
		   1  = key should be stored in higher tree
	*/

static int w_search(register MI_INFO *info, register MI_KEYDEF *keyinfo,
		    uint32_t comp_flag, unsigned char *key, uint32_t key_length, internal::my_off_t page,
		    unsigned char *father_buff, unsigned char *father_keypos,
		    internal::my_off_t father_page, bool insert_last)
{
  int error,flag;
  uint32_t nod_flag, search_key_length;
  unsigned char *temp_buff,*keypos;
  unsigned char keybuff[MI_MAX_KEY_BUFF];
  bool was_last_key;
  internal::my_off_t next_page, dupp_key_pos;

  search_key_length= (comp_flag & SEARCH_FIND) ? key_length : USE_WHOLE_KEY;
  if (!(temp_buff= (unsigned char*) malloc(keyinfo->block_length+
				           MI_MAX_KEY_BUFF*2)))
    return(-1);
  if (!_mi_fetch_keypage(info,keyinfo,page,DFLT_INIT_HITS,temp_buff,0))
    goto err;

  flag=(*keyinfo->bin_search)(info,keyinfo,temp_buff,key,search_key_length,
			      comp_flag, &keypos, keybuff, &was_last_key);
  nod_flag=mi_test_if_nod(temp_buff);
  if (flag == 0)
  {
    uint32_t tmp_key_length;
	/* get position to record with duplicated key */
    tmp_key_length=(*keyinfo->get_key)(keyinfo,nod_flag,&keypos,keybuff);
    if (tmp_key_length)
      dupp_key_pos=_mi_dpos(info,0,keybuff+tmp_key_length);
    else
      dupp_key_pos= HA_OFFSET_ERROR;

    {
      info->dupp_key_pos= dupp_key_pos;
      free(temp_buff);
      errno=HA_ERR_FOUND_DUPP_KEY;
      return(-1);
    }
  }
  if (flag == MI_FOUND_WRONG_KEY)
    return(-1);
  if (!was_last_key)
    insert_last=0;
  next_page=_mi_kpos(nod_flag,keypos);
  if (next_page == HA_OFFSET_ERROR ||
      (error=w_search(info, keyinfo, comp_flag, key, key_length, next_page,
		      temp_buff, keypos, page, insert_last)) >0)
  {
    error=_mi_insert(info,keyinfo,key,temp_buff,keypos,keybuff,father_buff,
		     father_keypos,father_page, insert_last);
    if (_mi_write_keypage(info,keyinfo,page,DFLT_INIT_HITS,temp_buff))
      goto err;
  }
  free(temp_buff);
  return(error);
err:
  free(temp_buff);
  return (-1);
} /* w_search */


/*
  Insert new key.

  SYNOPSIS
    _mi_insert()
    info                        Open table information.
    keyinfo                     Key definition information.
    key                         New key.
    anc_buff                    Key page (beginning).
    key_pos                     Position in key page where to insert.
    key_buff                    Copy of previous key.
    father_buff                 parent key page for balancing.
    father_key_pos              position in parent key page for balancing.
    father_page                 position of parent key page in file.
    insert_last                 If to append at end of page.

  DESCRIPTION
    Insert new key at right of key_pos.

  RETURN
    2           if key contains key to upper level.
    0           OK.
    < 0         Error.
*/

int _mi_insert(register MI_INFO *info, register MI_KEYDEF *keyinfo,
	       unsigned char *key, unsigned char *anc_buff, unsigned char *key_pos, unsigned char *key_buff,
               unsigned char *father_buff, unsigned char *father_key_pos, internal::my_off_t father_page,
	       bool insert_last)
{
  uint32_t a_length,nod_flag;
  int t_length;
  unsigned char *endpos, *prev_key;
  MI_KEY_PARAM s_temp;

  nod_flag=mi_test_if_nod(anc_buff);
  a_length=mi_getint(anc_buff);
  endpos= anc_buff+ a_length;
  prev_key=(key_pos == anc_buff+2+nod_flag ? (unsigned char*) 0 : key_buff);
  t_length=(*keyinfo->pack_key)(keyinfo,nod_flag,
				(key_pos == endpos ? (unsigned char*) 0 : key_pos),
				prev_key, prev_key,
				key,&s_temp);

  if (t_length > 0)
  {
    if (t_length >= keyinfo->maxlength*2+MAX_POINTER_LENGTH)
    {
      mi_print_error(info->s, HA_ERR_CRASHED);
      errno=HA_ERR_CRASHED;
      return(-1);
    }
    internal::bmove_upp((unsigned char*) endpos+t_length,(unsigned char*) endpos,(uint) (endpos-key_pos));
  }
  else
  {
    if (-t_length >= keyinfo->maxlength*2+MAX_POINTER_LENGTH)
    {
      mi_print_error(info->s, HA_ERR_CRASHED);
      errno=HA_ERR_CRASHED;
      return(-1);
    }
    memmove(key_pos, key_pos - t_length, endpos - key_pos + t_length);
  }
  (*keyinfo->store_key)(keyinfo,key_pos,&s_temp);
  a_length+=t_length;
  mi_putint(anc_buff,a_length,nod_flag);
  if (a_length <= keyinfo->block_length)
  {
    return(0);				/* There is room on page */
  }
  /* Page is full */
  if (nod_flag)
    insert_last=0;
  if (!(keyinfo->flag & (HA_VAR_LENGTH_KEY | HA_BINARY_PACK_KEY)) &&
      father_buff && !insert_last)
    return(_mi_balance_page(info,keyinfo,key,anc_buff,father_buff,
				 father_key_pos,father_page));
  return(_mi_split_page(info,keyinfo,key,anc_buff,key_buff, insert_last));
} /* _mi_insert */


	/* split a full page in two and assign emerging item to key */

int _mi_split_page(register MI_INFO *info, register MI_KEYDEF *keyinfo,
		   unsigned char *key, unsigned char *buff, unsigned char *key_buff,
		   bool insert_last_key)
{
  uint32_t length,a_length,key_ref_length,t_length,nod_flag,key_length;
  unsigned char *key_pos,*pos, *after_key= NULL;
  internal::my_off_t new_pos;
  MI_KEY_PARAM s_temp;

  if (info->s->keyinfo+info->lastinx == keyinfo)
    info->page_changed=1;			/* Info->buff is used */
  info->buff_used=1;
  nod_flag=mi_test_if_nod(buff);
  key_ref_length=2+nod_flag;
  if (insert_last_key)
    key_pos=_mi_find_last_pos(keyinfo,buff,key_buff, &key_length, &after_key);
  else
    key_pos=_mi_find_half_pos(nod_flag,keyinfo,buff,key_buff, &key_length,
			      &after_key);
  if (!key_pos)
    return(-1);

  length=(uint) (key_pos-buff);
  a_length=mi_getint(buff);
  mi_putint(buff,length,nod_flag);

  key_pos=after_key;
  if (nod_flag)
  {
    pos=key_pos-nod_flag;
    memcpy(info->buff + 2, pos, nod_flag);
  }

	/* Move middle item to key and pointer to new page */
  if ((new_pos=_mi_new(info,keyinfo,DFLT_INIT_HITS)) == HA_OFFSET_ERROR)
    return(-1);
  _mi_kpointer(info,_mi_move_key(keyinfo,key,key_buff),new_pos);

	/* Store new page */
  if (!(*keyinfo->get_key)(keyinfo,nod_flag,&key_pos,key_buff))
    return(-1);

  t_length=(*keyinfo->pack_key)(keyinfo,nod_flag,(unsigned char *) 0,
				(unsigned char*) 0, (unsigned char*) 0,
				key_buff, &s_temp);
  length=(uint) ((buff+a_length)-key_pos);
  memcpy(info->buff+key_ref_length+t_length, key_pos, length);
  (*keyinfo->store_key)(keyinfo,info->buff+key_ref_length,&s_temp);
  mi_putint(info->buff,length+t_length+key_ref_length,nod_flag);

  if (_mi_write_keypage(info,keyinfo,new_pos,DFLT_INIT_HITS,info->buff))
    return(-1);
  return(2);				/* Middle key up */
} /* _mi_split_page */


	/*
	  Calculate how to much to move to split a page in two
	  Returns pointer to start of key.
	  key will contain the key.
	  return_key_length will contain the length of key
	  after_key will contain the position to where the next key starts
	*/

unsigned char *_mi_find_half_pos(uint32_t nod_flag, MI_KEYDEF *keyinfo, unsigned char *page,
			 unsigned char *key, uint32_t *return_key_length,
			 unsigned char **after_key)
{
  uint32_t keys,length,key_ref_length;
  unsigned char *end,*lastpos;

  key_ref_length=2+nod_flag;
  length=mi_getint(page)-key_ref_length;
  page+=key_ref_length;
  if (!(keyinfo->flag &
	(HA_PACK_KEY | HA_SPACE_PACK_USED | HA_VAR_LENGTH_KEY |
	 HA_BINARY_PACK_KEY)))
  {
    key_ref_length=keyinfo->keylength+nod_flag;
    keys=length/(key_ref_length*2);
    *return_key_length=keyinfo->keylength;
    end=page+keys*key_ref_length;
    *after_key=end+key_ref_length;
    memcpy(key,end,key_ref_length);
    return(end);
  }

  end=page+length/2-key_ref_length;		/* This is aprox. half */
  *key='\0';
  do
  {
    lastpos=page;
    if (!(length=(*keyinfo->get_key)(keyinfo,nod_flag,&page,key)))
      return(0);
  } while (page < end);
  *return_key_length=length;
  *after_key=page;
  return(lastpos);
} /* _mi_find_half_pos */


	/*
	  Split buffer at last key
	  Returns pointer to the start of the key before the last key
	  key will contain the last key
	*/

static unsigned char *_mi_find_last_pos(MI_KEYDEF *keyinfo, unsigned char *page,
				unsigned char *key, uint32_t *return_key_length,
				unsigned char **after_key)
{
  uint32_t keys;
  uint32_t length;
  uint32_t last_length= 0;
  uint32_t key_ref_length;
  unsigned char *end, *lastpos, *prevpos= NULL;
  unsigned char key_buff[MI_MAX_KEY_BUFF];

  key_ref_length=2;
  length=mi_getint(page)-key_ref_length;
  page+=key_ref_length;
  if (!(keyinfo->flag &
	(HA_PACK_KEY | HA_SPACE_PACK_USED | HA_VAR_LENGTH_KEY |
	 HA_BINARY_PACK_KEY)))
  {
    keys=length/keyinfo->keylength-2;
    *return_key_length=length=keyinfo->keylength;
    end=page+keys*length;
    *after_key=end+length;
    memcpy(key,end,length);
    return(end);
  }

  end= page + length - key_ref_length;
  *key='\0';
  length=0;
  lastpos=page;
  while (page < end)
  {
    prevpos=lastpos; lastpos=page;
    last_length=length;
    memcpy(key, key_buff, length);		/* previous key */
    if (!(length=(*keyinfo->get_key)(keyinfo,0,&page,key_buff)))
    {
      mi_print_error(keyinfo->share, HA_ERR_CRASHED);
      errno=HA_ERR_CRASHED;
      return(0);
    }
  }
  *return_key_length=last_length;
  *after_key=lastpos;
  return(prevpos);
} /* _mi_find_last_pos */


	/* Balance page with not packed keys with page on right/left */
	/* returns 0 if balance was done */

static int _mi_balance_page(register MI_INFO *info, MI_KEYDEF *keyinfo,
			    unsigned char *key, unsigned char *curr_buff, unsigned char *father_buff,
			    unsigned char *father_key_pos, internal::my_off_t father_page)
{
  bool right;
  uint32_t k_length,father_length,father_keylength,nod_flag,curr_keylength,
       right_length,left_length,new_right_length,new_left_length,extra_length,
       length,keys;
  unsigned char *pos,*buff,*extra_buff;
  internal::my_off_t next_page,new_pos;
  unsigned char tmp_part_key[MI_MAX_KEY_BUFF];

  k_length=keyinfo->keylength;
  father_length=mi_getint(father_buff);
  father_keylength=k_length+info->s->base.key_reflength;
  nod_flag=mi_test_if_nod(curr_buff);
  curr_keylength=k_length+nod_flag;
  info->page_changed=1;

  if ((father_key_pos != father_buff+father_length &&
       (info->state->records & 1)) ||
      father_key_pos == father_buff+2+info->s->base.key_reflength)
  {
    right=1;
    next_page= _mi_kpos(info->s->base.key_reflength,
			father_key_pos+father_keylength);
    buff=info->buff;
  }
  else
  {
    right=0;
    father_key_pos-=father_keylength;
    next_page= _mi_kpos(info->s->base.key_reflength,father_key_pos);
					/* Fix that curr_buff is to left */
    buff=curr_buff; curr_buff=info->buff;
  }					/* father_key_pos ptr to parting key */

  if (!_mi_fetch_keypage(info,keyinfo,next_page,DFLT_INIT_HITS,info->buff,0))
    goto err;

	/* Test if there is room to share keys */

  left_length=mi_getint(curr_buff);
  right_length=mi_getint(buff);
  keys=(left_length+right_length-4-nod_flag*2)/curr_keylength;

  if ((right ? right_length : left_length) + curr_keylength <=
      keyinfo->block_length)
  {						/* Merge buffs */
    new_left_length=2+nod_flag+(keys/2)*curr_keylength;
    new_right_length=2+nod_flag+((keys+1)/2)*curr_keylength;
    mi_putint(curr_buff,new_left_length,nod_flag);
    mi_putint(buff,new_right_length,nod_flag);

    if (left_length < new_left_length)
    {						/* Move keys buff -> leaf */
      pos=curr_buff+left_length;
      memcpy(pos, father_key_pos, k_length);
      length= new_left_length - left_length - k_length;
      memcpy(pos+k_length, buff+2, length);
      pos=buff+2+length;
      memcpy(father_key_pos, pos, k_length);
      memmove(buff+2, pos+k_length, new_right_length);
    }
    else
    {						/* Move keys -> buff */

      internal::bmove_upp((unsigned char*) buff+new_right_length,(unsigned char*) buff+right_length,
		right_length-2);
      length=new_right_length-right_length-k_length;
      memcpy(buff+2+length,father_key_pos, k_length);
      pos=curr_buff+new_left_length;
      memcpy(father_key_pos, pos, k_length);
      memcpy(buff+2, pos+k_length, length);
    }

    if (_mi_write_keypage(info,keyinfo,next_page,DFLT_INIT_HITS,info->buff) ||
	_mi_write_keypage(info,keyinfo,father_page,DFLT_INIT_HITS,father_buff))
      goto err;
    return(0);
  }

	/* curr_buff[] and buff[] are full, lets split and make new nod */

  extra_buff=info->buff+info->s->base.max_key_block_length;
  new_left_length=new_right_length=2+nod_flag+(keys+1)/3*curr_keylength;
  if (keys == 5)				/* Too few keys to balance */
    new_left_length-=curr_keylength;
  extra_length=nod_flag+left_length+right_length-
    new_left_length-new_right_length-curr_keylength;
  mi_putint(curr_buff,new_left_length,nod_flag);
  mi_putint(buff,new_right_length,nod_flag);
  mi_putint(extra_buff,extra_length+2,nod_flag);

  /* move first largest keys to new page  */
  pos=buff+right_length-extra_length;
  memcpy(extra_buff+2, pos, extra_length);
  /* Save new parting key */
  memcpy(tmp_part_key, pos-k_length,k_length);
  /* Make place for new keys */
  internal::bmove_upp((unsigned char*) buff+new_right_length,(unsigned char*) pos-k_length,
	    right_length-extra_length-k_length-2);
  /* Copy keys from left page */
  pos= curr_buff+new_left_length;
  length= left_length - new_left_length - k_length;
  memcpy(buff+2, pos+k_length, length);
  /* Copy old parting key */
  memcpy(buff+2+length, father_key_pos, k_length);

  /* Move new parting keys up to caller */
  memcpy((right ? key : father_key_pos), pos, k_length);
  memcpy((right ? father_key_pos : key), tmp_part_key, k_length);

  if ((new_pos=_mi_new(info,keyinfo,DFLT_INIT_HITS)) == HA_OFFSET_ERROR)
    goto err;
  _mi_kpointer(info,key+k_length,new_pos);
  if (_mi_write_keypage(info,keyinfo,(right ? new_pos : next_page),
			DFLT_INIT_HITS,info->buff) ||
      _mi_write_keypage(info,keyinfo,(right ? next_page : new_pos),
                        DFLT_INIT_HITS,extra_buff))
    goto err;

  return(1);				/* Middle key up */

err:
  return(-1);
} /* _mi_balance_page */

/**********************************************************************
 *                Bulk insert code                                    *
 **********************************************************************/

typedef struct {
  MI_INFO *info;
  uint32_t keynr;
} bulk_insert_param;

int _mi_ck_write_tree(register MI_INFO *info, uint32_t keynr, unsigned char *key,
		      uint32_t key_length)
{
  int error;

  error= tree_insert(&info->bulk_insert[keynr], key,
         key_length + info->s->rec_reflength,
         info->bulk_insert[keynr].custom_arg) ? 0 : HA_ERR_OUT_OF_MEM ;

  return(error);
} /* _mi_ck_write_tree */


/* typeof(_mi_keys_compare)=qsort_cmp2 */

static int keys_compare(bulk_insert_param *param, unsigned char *key1, unsigned char *key2)
{
  uint32_t not_used[2];
  return ha_key_cmp(param->info->s->keyinfo[param->keynr].seg,
                    key1, key2, USE_WHOLE_KEY, SEARCH_SAME,
                    not_used);
}


static int keys_free(unsigned char *key, TREE_FREE mode, bulk_insert_param *param)
{
  /*
    Probably I can use info->lastkey here, but I'm not sure,
    and to be safe I'd better use local lastkey.
  */
  unsigned char lastkey[MI_MAX_KEY_BUFF];
  uint32_t keylen;
  MI_KEYDEF *keyinfo;

  switch (mode) {
  case free_init:
    if (param->info->s->concurrent_insert)
    {
      param->info->s->keyinfo[param->keynr].version++;
    }
    return 0;
  case free_free:
    keyinfo=param->info->s->keyinfo+param->keynr;
    keylen=_mi_keylength(keyinfo, key);
    memcpy(lastkey, key, keylen);
    return _mi_ck_write_btree(param->info,param->keynr,lastkey,
			      keylen - param->info->s->rec_reflength);
  case free_end:
    return 0;
  }
  return -1;
}


int mi_init_bulk_insert(MI_INFO *info, uint32_t cache_size, ha_rows rows)
{
  MYISAM_SHARE *share=info->s;
  MI_KEYDEF *key=share->keyinfo;
  bulk_insert_param *params;
  uint32_t i, num_keys, total_keylength;
  uint64_t key_map;

  assert(!info->bulk_insert &&
	      (!rows || rows >= MI_MIN_ROWS_TO_USE_BULK_INSERT));

  mi_clear_all_keys_active(key_map);
  for (i=total_keylength=num_keys=0 ; i < share->base.keys ; i++)
  {
    if (! (key[i].flag & HA_NOSAME) && (share->base.auto_key != i + 1) &&
        mi_is_key_active(share->state.key_map, i))
    {
      num_keys++;
      mi_set_key_active(key_map, i);
      total_keylength+=key[i].maxlength+TREE_ELEMENT_EXTRA_SIZE;
    }
  }

  if (num_keys==0 ||
      num_keys * MI_MIN_SIZE_BULK_INSERT_TREE > cache_size)
    return(0);

  if (rows && rows*total_keylength < cache_size)
    cache_size= (uint32_t)rows;
  else
    cache_size/=total_keylength*16;

  info->bulk_insert=(TREE *)
    malloc((sizeof(TREE)*share->base.keys+
           sizeof(bulk_insert_param)*num_keys));

  if (!info->bulk_insert)
    return(HA_ERR_OUT_OF_MEM);

  params=(bulk_insert_param *)(info->bulk_insert+share->base.keys);
  for (i=0 ; i < share->base.keys ; i++)
  {
    if (mi_is_key_active(key_map, i))
    {
      params->info=info;
      params->keynr=i;
      /* Only allocate a 16'th of the buffer at a time */
      init_tree(&info->bulk_insert[i],
                cache_size * key[i].maxlength,
                cache_size * key[i].maxlength, 0,
		(qsort_cmp2)keys_compare, false,
		(tree_element_free) keys_free, (void *)params++);
    }
    else
     info->bulk_insert[i].root=0;
  }

  return(0);
}

void mi_flush_bulk_insert(MI_INFO *info, uint32_t inx)
{
  if (info->bulk_insert)
  {
    if (is_tree_inited(&info->bulk_insert[inx]))
      reset_tree(&info->bulk_insert[inx]);
  }
}

void mi_end_bulk_insert(MI_INFO *info)
{
  if (info->bulk_insert)
  {
    uint32_t i;
    for (i=0 ; i < info->s->base.keys ; i++)
    {
      if (is_tree_inited(& info->bulk_insert[i]))
      {
        delete_tree(& info->bulk_insert[i]);
      }
    }
    free((void *)info->bulk_insert);
    info->bulk_insert=0;
  }
}
