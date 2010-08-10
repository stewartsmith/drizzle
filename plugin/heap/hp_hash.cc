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
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA */

/* The hash functions used for saveing keys */

#include "heap_priv.h"

#include "drizzled/charset_info.h"
#include <drizzled/util/test.h>

#include <math.h>
#include <string.h>

#include <cassert>

using namespace drizzled;

static uint32_t hp_hashnr(register HP_KEYDEF *keydef, register const unsigned char *key);
static int hp_key_cmp(HP_KEYDEF *keydef, const unsigned char *rec, const unsigned char *key);

/*
  Find out how many rows there is in the given range

  SYNOPSIS
    hp_rb_records_in_range()
    info		HEAP handler
    inx			Index to use
    min_key		Min key. Is = 0 if no min range
    max_key		Max key. Is = 0 if no max range

  NOTES
    min_key.flag can have one of the following values:
      HA_READ_KEY_EXACT		Include the key in the range
      HA_READ_AFTER_KEY		Don't include key in range

    max_key.flag can have one of the following values:
      HA_READ_BEFORE_KEY	Don't include key in range
      HA_READ_AFTER_KEY		Include all 'end_key' values in the range

  RETURN
   HA_POS_ERROR		Something is wrong with the index tree.
   0			There is no matching keys in the given range
   number > 0		There is approximately 'number' matching rows in
			the range.
*/

ha_rows hp_rb_records_in_range(HP_INFO *info, int inx,  key_range *min_key,
                               key_range *max_key)
{
  ha_rows start_pos, end_pos;
  HP_KEYDEF *keyinfo= info->getShare()->keydef + inx;
  TREE *rb_tree = &keyinfo->rb_tree;
  heap_rb_param custom_arg;

  info->lastinx= inx;
  custom_arg.keyseg= keyinfo->seg;
  custom_arg.search_flag= SEARCH_FIND | SEARCH_SAME;
  if (min_key)
  {
    custom_arg.key_length= hp_rb_pack_key(keyinfo, (unsigned char*) info->recbuf,
					  (unsigned char*) min_key->key,
					  min_key->keypart_map);
    start_pos= tree_record_pos(rb_tree, info->recbuf, min_key->flag,
			       &custom_arg);
  }
  else
  {
    start_pos= 0;
  }

  if (max_key)
  {
    custom_arg.key_length= hp_rb_pack_key(keyinfo, (unsigned char*) info->recbuf,
					  (unsigned char*) max_key->key,
                                          max_key->keypart_map);
    end_pos= tree_record_pos(rb_tree, info->recbuf, max_key->flag,
			     &custom_arg);
  }
  else
  {
    end_pos= rb_tree->elements_in_tree + (ha_rows)1;
  }

  if (start_pos == HA_POS_ERROR || end_pos == HA_POS_ERROR)
    return(HA_POS_ERROR);
  return(end_pos < start_pos ? (ha_rows) 0 :
	      (end_pos == start_pos ? (ha_rows) 1 : end_pos - start_pos));
}


	/* Search after a record based on a key */
	/* Sets info->current_ptr to found record */
	/* next_flag:  Search=0, next=1, prev =2, same =3 */

unsigned char *hp_search(HP_INFO *info, HP_KEYDEF *keyinfo, const unsigned char *key,
                uint32_t nextflag)
{
  register HASH_INFO *pos,*prev_ptr;
  int flag;
  uint32_t old_nextflag;
  HP_SHARE *share=info->getShare();
  old_nextflag=nextflag;
  flag=1;
  prev_ptr=0;

  if (share->records)
  {
    pos=hp_find_hash(&keyinfo->block, hp_mask(hp_hashnr(keyinfo, key),
					      share->blength, share->records));
    do
    {
      if (!hp_key_cmp(keyinfo, pos->ptr_to_rec, key))
      {
	switch (nextflag) {
	case 0:					/* Search after key */
	  info->current_hash_ptr=pos;
	  return(info->current_ptr= pos->ptr_to_rec);
	case 1:					/* Search next */
	  if (pos->ptr_to_rec == info->current_ptr)
	    nextflag=0;
	  break;
	case 2:					/* Search previous */
	  if (pos->ptr_to_rec == info->current_ptr)
	  {
	    errno=HA_ERR_KEY_NOT_FOUND;	/* If gpos == 0 */
	    info->current_hash_ptr=prev_ptr;
	    return(info->current_ptr=prev_ptr ? prev_ptr->ptr_to_rec : 0);
	  }
	  prev_ptr=pos;				/* Prev. record found */
	  break;
	case 3:					/* Search same */
	  if (pos->ptr_to_rec == info->current_ptr)
	  {
	    info->current_hash_ptr=pos;
	    return(info->current_ptr);
	  }
	}
      }
      if (flag)
      {
	flag=0;					/* Reset flag */
	if (hp_find_hash(&keyinfo->block,
			 hp_mask(hp_rec_hashnr(keyinfo, pos->ptr_to_rec),
				  share->blength, share->records)) != pos)
	  break;				/* Wrong link */
      }
    }
    while ((pos=pos->next_key));
  }
  errno=HA_ERR_KEY_NOT_FOUND;
  if (nextflag == 2 && ! info->current_ptr)
  {
    /* Do a previous from end */
    info->current_hash_ptr=prev_ptr;
    return(info->current_ptr=prev_ptr ? prev_ptr->ptr_to_rec : 0);
  }

  if (old_nextflag && nextflag)
    errno=HA_ERR_RECORD_CHANGED;		/* Didn't find old record */
  info->current_hash_ptr=0;
  return((info->current_ptr= 0));
}


/*
  Search next after last read;  Assumes that the table hasn't changed
  since last read !
*/

unsigned char *hp_search_next(HP_INFO *info, HP_KEYDEF *keyinfo, const unsigned char *key,
		      HASH_INFO *pos)
{
  while ((pos= pos->next_key))
  {
    if (! hp_key_cmp(keyinfo, pos->ptr_to_rec, key))
    {
      info->current_hash_ptr=pos;
      return (info->current_ptr= pos->ptr_to_rec);
    }
  }
  errno=HA_ERR_KEY_NOT_FOUND;
  info->current_hash_ptr=0;
  return ((info->current_ptr= 0));
}


/*
  Calculate position number for hash value.
  SYNOPSIS
    hp_mask()
      hashnr     Hash value
      buffmax    Value such that
                 2^(n-1) < maxlength <= 2^n = buffmax
      maxlength

  RETURN
    Array index, in [0..maxlength)
*/

uint32_t hp_mask(uint32_t hashnr, uint32_t buffmax, uint32_t maxlength)
{
  if ((hashnr & (buffmax-1)) < maxlength) return (hashnr & (buffmax-1));
  return (hashnr & ((buffmax >> 1) -1));
}


/*
  Change
    next_link -> ... -> X -> pos
  to
    next_link -> ... -> X -> newlink
*/

void hp_movelink(HASH_INFO *pos, HASH_INFO *next_link, HASH_INFO *newlink)
{
  HASH_INFO *old_link;
  do
  {
    old_link=next_link;
  }
  while ((next_link=next_link->next_key) != pos);
  old_link->next_key=newlink;
  return;
}

	/* Calc hashvalue for a key */

static uint32_t hp_hashnr(register HP_KEYDEF *keydef, register const unsigned char *key)
{
  /*register*/
  uint32_t nr=1, nr2=4;
  HA_KEYSEG *seg,*endseg;

  for (seg=keydef->seg,endseg=seg+keydef->keysegs ; seg < endseg ; seg++)
  {
    unsigned char *pos=(unsigned char*) key;
    key+=seg->length;
    if (seg->null_bit)
    {
      key++;					/* Skip null byte */
      if (*pos)					/* Found null */
      {
	nr^= (nr << 1) | 1;
	/* Add key pack length (2) to key for VARCHAR segments */
        if (seg->type == HA_KEYTYPE_VARTEXT1)
          key+= 2;
	continue;
      }
      pos++;
    }
    if (seg->type == HA_KEYTYPE_TEXT)
    {
       const CHARSET_INFO * const cs= seg->charset;
       uint32_t length= seg->length;
       if (cs->mbmaxlen > 1)
       {
         uint32_t char_length;
         char_length= my_charpos(cs, pos, pos + length, length/cs->mbmaxlen);
         set_if_smaller(length, char_length);
       }
       cs->coll->hash_sort(cs, pos, length, &nr, &nr2);
    }
    else if (seg->type == HA_KEYTYPE_VARTEXT1)  /* Any VARCHAR segments */
    {
       const CHARSET_INFO * const cs= seg->charset;
       uint32_t pack_length= 2;                     /* Key packing is constant */
       uint32_t length= uint2korr(pos);
       if (cs->mbmaxlen > 1)
       {
         uint32_t char_length;
         char_length= my_charpos(cs, pos +pack_length,
                                 pos +pack_length + length,
                                 seg->length/cs->mbmaxlen);
         set_if_smaller(length, char_length);
       }
       cs->coll->hash_sort(cs, pos+pack_length, length, &nr, &nr2);
       key+= pack_length;
    }
    else
    {
      for (; pos < (unsigned char*) key ; pos++)
      {
	nr^=(uint32_t) ((((uint) nr & 63)+nr2)*((uint) *pos)) + (nr << 8);
	nr2+=3;
      }
    }
  }
  return((uint32_t) nr);
}

	/* Calc hashvalue for a key in a record */

uint32_t hp_rec_hashnr(register HP_KEYDEF *keydef, register const unsigned char *rec)
{
  uint32_t nr=1, nr2=4;
  HA_KEYSEG *seg,*endseg;

  for (seg=keydef->seg,endseg=seg+keydef->keysegs ; seg < endseg ; seg++)
  {
    unsigned char *pos=(unsigned char*) rec+seg->start,*end=pos+seg->length;
    if (seg->null_bit)
    {
      if (rec[seg->null_pos] & seg->null_bit)
      {
	nr^= (nr << 1) | 1;
	continue;
      }
    }
    if (seg->type == HA_KEYTYPE_TEXT)
    {
      const CHARSET_INFO * const cs= seg->charset;
      uint32_t char_length= seg->length;
      if (cs->mbmaxlen > 1)
      {
        char_length= my_charpos(cs, pos, pos + char_length,
                                char_length / cs->mbmaxlen);
        set_if_smaller(char_length, (uint32_t)seg->length); /* QQ: ok to remove? */
      }
      cs->coll->hash_sort(cs, pos, char_length, &nr, &nr2);
    }
    else if (seg->type == HA_KEYTYPE_VARTEXT1)  /* Any VARCHAR segments */
    {
      const CHARSET_INFO * const cs= seg->charset;
      uint32_t pack_length= seg->bit_start;
      uint32_t length= (pack_length == 1 ? (uint) *(unsigned char*) pos : uint2korr(pos));
      if (cs->mbmaxlen > 1)
      {
        uint32_t char_length;
        char_length= my_charpos(cs, pos + pack_length,
                                pos + pack_length + length,
                                seg->length/cs->mbmaxlen);
        set_if_smaller(length, char_length);
      }
      cs->coll->hash_sort(cs, pos+pack_length, length, &nr, &nr2);
    }
    else
    {
      for (; pos < end ; pos++)
      {
	nr^=(uint32_t) ((((uint) nr & 63)+nr2)*((uint) *pos))+ (nr << 8);
	nr2+=3;
      }
    }
  }
  return(nr);
}

/*
  Compare keys for two records. Returns 0 if they are identical

  SYNOPSIS
    hp_rec_key_cmp()
    keydef		Key definition
    rec1		Record to compare
    rec2		Other record to compare
    diff_if_only_endspace_difference
			Different number of end space is significant

  NOTES
    diff_if_only_endspace_difference is used to allow us to insert
    'a' and 'a ' when there is an an unique key.

  RETURN
    0		Key is identical
    <> 0 	Key differes
*/

int hp_rec_key_cmp(HP_KEYDEF *keydef, const unsigned char *rec1, const unsigned char *rec2,
                   bool diff_if_only_endspace_difference)
{
  HA_KEYSEG *seg,*endseg;

  for (seg=keydef->seg,endseg=seg+keydef->keysegs ; seg < endseg ; seg++)
  {
    if (seg->null_bit)
    {
      if ((rec1[seg->null_pos] & seg->null_bit) !=
	  (rec2[seg->null_pos] & seg->null_bit))
	return 1;
      if (rec1[seg->null_pos] & seg->null_bit)
	continue;
    }
    if (seg->type == HA_KEYTYPE_TEXT)
    {
      const CHARSET_INFO * const cs= seg->charset;
      uint32_t char_length1;
      uint32_t char_length2;
      unsigned char *pos1= (unsigned char*)rec1 + seg->start;
      unsigned char *pos2= (unsigned char*)rec2 + seg->start;
      if (cs->mbmaxlen > 1)
      {
        uint32_t char_length= seg->length / cs->mbmaxlen;
        char_length1= my_charpos(cs, pos1, pos1 + seg->length, char_length);
        set_if_smaller(char_length1, (uint32_t)seg->length);
        char_length2= my_charpos(cs, pos2, pos2 + seg->length, char_length);
        set_if_smaller(char_length2, (uint32_t)seg->length);
      }
      else
      {
        char_length1= char_length2= seg->length;
      }
      if (seg->charset->coll->strnncollsp(seg->charset,
      					  pos1,char_length1,
					  pos2,char_length2, 0))
	return 1;
    }
    else if (seg->type == HA_KEYTYPE_VARTEXT1)  /* Any VARCHAR segments */
    {
      unsigned char *pos1= (unsigned char*) rec1 + seg->start;
      unsigned char *pos2= (unsigned char*) rec2 + seg->start;
      uint32_t char_length1, char_length2;
      uint32_t pack_length= seg->bit_start;
      const CHARSET_INFO * const cs= seg->charset;
      if (pack_length == 1)
      {
        char_length1= (uint) *(unsigned char*) pos1++;
        char_length2= (uint) *(unsigned char*) pos2++;
      }
      else
      {
        char_length1= uint2korr(pos1);
        char_length2= uint2korr(pos2);
        pos1+= 2;
        pos2+= 2;
      }
      if (cs->mbmaxlen > 1)
      {
        uint32_t safe_length1= char_length1;
        uint32_t safe_length2= char_length2;
        uint32_t char_length= seg->length / cs->mbmaxlen;
        char_length1= my_charpos(cs, pos1, pos1 + char_length1, char_length);
        set_if_smaller(char_length1, safe_length1);
        char_length2= my_charpos(cs, pos2, pos2 + char_length2, char_length);
        set_if_smaller(char_length2, safe_length2);
      }

      if (cs->coll->strnncollsp(seg->charset,
                                pos1, char_length1,
                                pos2, char_length2,
                                seg->flag & HA_END_SPACE_ARE_EQUAL ?
                                0 : diff_if_only_endspace_difference))
	return 1;
    }
    else
    {
      if (memcmp(rec1+seg->start,rec2+seg->start,seg->length))
	return 1;
    }
  }
  return 0;
}

	/* Compare a key in a record to a whole key */

static int hp_key_cmp(HP_KEYDEF *keydef, const unsigned char *rec, const unsigned char *key)
{
  HA_KEYSEG *seg,*endseg;

  for (seg=keydef->seg,endseg=seg+keydef->keysegs ;
       seg < endseg ;
       key+= (seg++)->length)
  {
    if (seg->null_bit)
    {
      int found_null=test(rec[seg->null_pos] & seg->null_bit);
      if (found_null != (int) *key++)
	return 1;
      if (found_null)
      {
        /* Add key pack length (2) to key for VARCHAR segments */
        if (seg->type == HA_KEYTYPE_VARTEXT1)
          key+= 2;
	continue;
      }
    }
    if (seg->type == HA_KEYTYPE_TEXT)
    {
      const CHARSET_INFO * const cs= seg->charset;
      uint32_t char_length_key;
      uint32_t char_length_rec;
      unsigned char *pos= (unsigned char*) rec + seg->start;
      if (cs->mbmaxlen > 1)
      {
        uint32_t char_length= seg->length / cs->mbmaxlen;
        char_length_key= my_charpos(cs, key, key + seg->length, char_length);
        set_if_smaller(char_length_key, (uint32_t)seg->length);
        char_length_rec= my_charpos(cs, pos, pos + seg->length, char_length);
        set_if_smaller(char_length_rec, (uint32_t)seg->length);
      }
      else
      {
        char_length_key= seg->length;
        char_length_rec= seg->length;
      }

      if (seg->charset->coll->strnncollsp(seg->charset,
					  (unsigned char*) pos, char_length_rec,
					  (unsigned char*) key, char_length_key, 0))
	return 1;
    }
    else if (seg->type == HA_KEYTYPE_VARTEXT1)  /* Any VARCHAR segments */
    {
      unsigned char *pos= (unsigned char*) rec + seg->start;
      const CHARSET_INFO * const cs= seg->charset;
      uint32_t pack_length= seg->bit_start;
      uint32_t char_length_rec= (pack_length == 1 ? (uint) *(unsigned char*) pos :
                             uint2korr(pos));
      /* Key segments are always packed with 2 bytes */
      uint32_t char_length_key= uint2korr(key);
      pos+= pack_length;
      key+= 2;                                  /* skip key pack length */
      if (cs->mbmaxlen > 1)
      {
        uint32_t char_length1, char_length2;
        char_length1= char_length2= seg->length / cs->mbmaxlen;
        char_length1= my_charpos(cs, key, key + char_length_key, char_length1);
        set_if_smaller(char_length_key, char_length1);
        char_length2= my_charpos(cs, pos, pos + char_length_rec, char_length2);
        set_if_smaller(char_length_rec, char_length2);
      }

      if (cs->coll->strnncollsp(seg->charset,
                                (unsigned char*) pos, char_length_rec,
                                (unsigned char*) key, char_length_key, 0))
	return 1;
    }
    else
    {
      if (memcmp(rec+seg->start,key,seg->length))
	return 1;
    }
  }
  return 0;
}


	/* Copy a key from a record to a keybuffer */

void hp_make_key(HP_KEYDEF *keydef, unsigned char *key, const unsigned char *rec)
{
  HA_KEYSEG *seg,*endseg;

  for (seg=keydef->seg,endseg=seg+keydef->keysegs ; seg < endseg ; seg++)
  {
    const CHARSET_INFO * const cs= seg->charset;
    uint32_t char_length= seg->length;
    unsigned char *pos= (unsigned char*) rec + seg->start;
    if (seg->null_bit)
      *key++= test(rec[seg->null_pos] & seg->null_bit);
    if (cs->mbmaxlen > 1)
    {
      char_length= my_charpos(cs, pos, pos + seg->length,
                              char_length / cs->mbmaxlen);
      set_if_smaller(char_length, (uint32_t)seg->length); /* QQ: ok to remove? */
    }
    if (seg->type == HA_KEYTYPE_VARTEXT1)
      char_length+= seg->bit_start;             /* Copy also length */
    memcpy(key,rec+seg->start,(size_t) char_length);
    key+= char_length;
  }
}

#define FIX_LENGTH(cs, pos, length, char_length)                        \
  do {                                                                  \
    if (length > char_length)                                           \
      char_length= my_charpos(cs, pos, pos+length, char_length);        \
    set_if_smaller(char_length,length);                                 \
  } while(0)


uint32_t hp_rb_make_key(HP_KEYDEF *keydef, unsigned char *key,
		    const unsigned char *rec, unsigned char *recpos)
{
  unsigned char *start_key= key;
  HA_KEYSEG *seg, *endseg;

  for (seg= keydef->seg, endseg= seg + keydef->keysegs; seg < endseg; seg++)
  {
    uint32_t char_length;
    if (seg->null_bit)
    {
      if (!(*key++= 1 - test(rec[seg->null_pos] & seg->null_bit)))
        continue;
    }
    if (seg->flag & HA_SWAP_KEY)
    {
      uint32_t length= seg->length;
      unsigned char *pos= (unsigned char*) rec + seg->start;

      if (seg->type == HA_KEYTYPE_DOUBLE)
      {
	double nr;
	float8get(nr, pos);
	if (isnan(nr))
	{
 	  memset(key, 0, length);
	  key+= length;
	  continue;
	}
      }
      pos+= length;
      while (length--)
      {
	*key++= *--pos;
      }
      continue;
    }

    if (seg->flag & HA_VAR_LENGTH_PART)
    {
      unsigned char *pos=      (unsigned char*) rec + seg->start;
      uint32_t length=     seg->length;
      uint32_t pack_length= seg->bit_start;
      uint32_t tmp_length= (pack_length == 1 ? (uint) *(unsigned char*) pos :
                        uint2korr(pos));
      const CHARSET_INFO * const cs= seg->charset;
      char_length= length/cs->mbmaxlen;

      pos+= pack_length;			/* Skip VARCHAR length */
      set_if_smaller(length,tmp_length);
      FIX_LENGTH(cs, pos, length, char_length);
      store_key_length_inc(key,char_length);
      memcpy(key,pos,(size_t) char_length);
      key+= char_length;
      continue;
    }

    char_length= seg->length;
    if (seg->charset->mbmaxlen > 1)
    {
      char_length= my_charpos(seg->charset,
                              rec + seg->start, rec + seg->start + char_length,
                              char_length / seg->charset->mbmaxlen);
      set_if_smaller(char_length, (uint32_t)seg->length); /* QQ: ok to remove? */
      if (char_length < seg->length)
        seg->charset->cset->fill(seg->charset, (char*) key + char_length,
                                 seg->length - char_length, ' ');
    }
    memcpy(key, rec + seg->start, (size_t) char_length);
    key+= seg->length;
  }
  memcpy(key, &recpos, sizeof(unsigned char*));
  return (uint32_t) (key - start_key);
}


uint32_t hp_rb_pack_key(HP_KEYDEF *keydef, unsigned char *key, const unsigned char *old,
                    key_part_map keypart_map)
{
  HA_KEYSEG *seg, *endseg;
  unsigned char *start_key= key;

  for (seg= keydef->seg, endseg= seg + keydef->keysegs;
       seg < endseg && keypart_map; old+= seg->length, seg++)
  {
    uint32_t char_length;
    keypart_map>>= 1;
    if (seg->null_bit)
    {
      if (!(*key++= (char) 1 - *old++))
        continue;
      }
    if (seg->flag & HA_SWAP_KEY)
    {
      uint32_t length= seg->length;
      unsigned char *pos= (unsigned char*) old + length;

      while (length--)
      {
	*key++= *--pos;
      }
      continue;
    }
    if (seg->flag & (HA_VAR_LENGTH_PART | HA_BLOB_PART))
    {
      /* Length of key-part used with heap_rkey() always 2 */
      uint32_t tmp_length=uint2korr(old);
      uint32_t length= seg->length;
      const CHARSET_INFO * const cs= seg->charset;
      char_length= length/cs->mbmaxlen;

      old+= 2;
      set_if_smaller(length,tmp_length);	/* Safety */
      FIX_LENGTH(cs, old, length, char_length);
      store_key_length_inc(key,char_length);
      memcpy(key, old,(size_t) char_length);
      key+= char_length;
      continue;
    }
    char_length= seg->length;
    if (seg->charset->mbmaxlen > 1)
    {
      char_length= my_charpos(seg->charset, old, old+char_length,
                              char_length / seg->charset->mbmaxlen);
      set_if_smaller(char_length, (uint32_t)seg->length); /* QQ: ok to remove? */
      if (char_length < seg->length)
        seg->charset->cset->fill(seg->charset, (char*) key + char_length,
                                 seg->length - char_length, ' ');
    }
    memcpy(key, old, (size_t) char_length);
    key+= seg->length;
  }
  return (uint) (key - start_key);
}


uint32_t hp_rb_key_length(HP_KEYDEF *keydef, const unsigned char *not_used)
{
  (void)not_used;
  return keydef->length;
}


uint32_t hp_rb_null_key_length(HP_KEYDEF *keydef, const unsigned char *key)
{
  const unsigned char *start_key= key;
  HA_KEYSEG *seg, *endseg;

  for (seg= keydef->seg, endseg= seg + keydef->keysegs; seg < endseg; seg++)
  {
    if (seg->null_bit && !*key++)
      continue;
    key+= seg->length;
  }
  return (uint) (key - start_key);
}


uint32_t hp_rb_var_key_length(HP_KEYDEF *keydef, const unsigned char *key)
{
  const unsigned char *start_key= key;
  HA_KEYSEG *seg, *endseg;

  for (seg= keydef->seg, endseg= seg + keydef->keysegs; seg < endseg; seg++)
  {
    uint32_t length= seg->length;
    if (seg->null_bit && !*key++)
      continue;
    if (seg->flag & (HA_VAR_LENGTH_PART | HA_BLOB_PART))
    {
      get_key_length(length, key);
    }
    key+= length;
  }
  return (uint) (key - start_key);
}


/*
  Test if any of the key parts are NULL.
  Return:
    1 if any of the key parts was NULL
    0 otherwise
*/

bool hp_if_null_in_key(HP_KEYDEF *keydef, const unsigned char *record)
{
  HA_KEYSEG *seg,*endseg;
  for (seg=keydef->seg,endseg=seg+keydef->keysegs ; seg < endseg ; seg++)
  {
    if (seg->null_bit && (record[seg->null_pos] & seg->null_bit))
      return 1;
  }
  return 0;
}


/*
  Update auto_increment info

  SYNOPSIS
    update_auto_increment()
    info			MyISAM handler
    record			Row to update

  IMPLEMENTATION
    Only replace the auto_increment value if it is higher than the previous
    one. For signed columns we don't update the auto increment value if it's
    less than zero.
*/

void heap_update_auto_increment(HP_INFO *info, const unsigned char *record)
{
  uint64_t value= 0;			/* Store unsigned values here */
  int64_t s_value= 0;			/* Store signed values here */

  HA_KEYSEG *keyseg= info->getShare()->keydef[info->getShare()->auto_key - 1].seg;
  const unsigned char *key=  (unsigned char*) record + keyseg->start;

  switch (info->getShare()->auto_key_type) {
  case HA_KEYTYPE_BINARY:
    value=(uint64_t)  *(unsigned char*) key;
    break;
  case HA_KEYTYPE_LONG_INT:
    s_value= (int64_t) sint4korr(key);
    break;
  case HA_KEYTYPE_ULONG_INT:
    value=(uint64_t) uint4korr(key);
    break;
  case HA_KEYTYPE_UINT24:
    value=(uint64_t) uint3korr(key);
    break;
  case HA_KEYTYPE_DOUBLE:                       /* This shouldn't be used */
  {
    double f_1;
    float8get(f_1,key);
    /* Ignore negative values */
    value = (f_1 < 0.0) ? 0 : (uint64_t) f_1;
    break;
  }
  case HA_KEYTYPE_LONGLONG:
    s_value= sint8korr(key);
    break;
  case HA_KEYTYPE_ULONGLONG:
    value= uint8korr(key);
    break;
  default:
    assert(0);
    value=0;                                    /* Error */
    break;
  }

  /*
    The following code works becasue if s_value < 0 then value is 0
    and if s_value == 0 then value will contain either s_value or the
    correct value.
  */
  set_if_bigger(info->getShare()->auto_increment,
                (s_value > 0) ? (uint64_t) s_value : value);
}
