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

/* open a isam-database */

#include "myisam_priv.h"

#include <string.h>
#include <algorithm>
#include <memory>
#include <boost/scoped_ptr.hpp>
#include <boost/scoped_array.hpp>

#include <drizzled/internal/m_string.h>
#include <drizzled/util/test.h>
#include <drizzled/charset.h>
#include <drizzled/memory/multi_malloc.h>
#include <drizzled/identifier.h>


using namespace std;
using namespace drizzled;

static void setup_key_functions(MI_KEYDEF *keyinfo);
static unsigned char *mi_keydef_read(unsigned char *ptr, MI_KEYDEF *keydef);
static unsigned char *mi_keyseg_read(unsigned char *ptr, HA_KEYSEG *keyseg);
static unsigned char *mi_recinfo_read(unsigned char *ptr, MI_COLUMNDEF *recinfo);
static uint64_t mi_safe_mul(uint64_t a, uint64_t b);
static unsigned char *mi_state_info_read(unsigned char *ptr, MI_STATE_INFO *state);
static unsigned char *mi_uniquedef_read(unsigned char *ptr, MI_UNIQUEDEF *def);
static unsigned char *my_n_base_info_read(unsigned char *ptr, MI_BASE_INFO *base);

#define disk_pos_assert(pos, end_pos) \
if (pos > end_pos)             \
{                              \
  errno=HA_ERR_CRASHED;     \
  goto err;                    \
}


/******************************************************************************
** Return the shared struct if the table is already open.
** In MySQL the server will handle version issues.
******************************************************************************/

MI_INFO *test_if_reopen(char *filename)
{
  list<MI_INFO *>::iterator it= myisam_open_list.begin();
  while (it != myisam_open_list.end())
  {
    MI_INFO *info= *it;
    MYISAM_SHARE *share=info->s;
    if (!strcmp(share->unique_file_name,filename) && share->last_version)
      return info;
    ++it;
  }
  return 0;
}


/******************************************************************************
  open a MyISAM database.
  See my_base.h for the handle_locking argument
  if handle_locking and HA_OPEN_ABORT_IF_CRASHED then abort if the table
  is marked crashed or if we are not using locking and the table doesn't
  have an open count of 0.
******************************************************************************/

MI_INFO *mi_open(const drizzled::identifier::Table &identifier, int mode, uint32_t open_flags)
{
  int lock_error,kfile,open_mode,save_errno,have_rtree=0;
  uint32_t i,j,len,errpos,head_length,base_pos,offset,info_length,keys,
    key_parts,unique_key_parts,uniques;
  char name_buff[FN_REFLEN], org_name[FN_REFLEN], index_name[FN_REFLEN],
       data_name[FN_REFLEN], rp_buff[PATH_MAX];
  unsigned char *disk_cache= NULL;
  unsigned char *disk_pos, *end_pos;
  MI_INFO info,*m_info,*old_info;
  boost::scoped_ptr<MYISAM_SHARE> share_buff_ap(new MYISAM_SHARE);
  MYISAM_SHARE &share_buff= *share_buff_ap.get();
  MYISAM_SHARE *share;
  boost::scoped_array<ulong> rec_per_key_part_ap(new ulong[HA_MAX_POSSIBLE_KEY*MI_MAX_KEY_SEG]);
  ulong *rec_per_key_part= rec_per_key_part_ap.get();
  internal::my_off_t key_root[HA_MAX_POSSIBLE_KEY],key_del[MI_MAX_KEY_BLOCK_SIZE];
  uint64_t max_key_file_length, max_data_file_length;

  kfile= -1;
  lock_error=1;
  errpos=0;
  head_length=sizeof(share_buff.state.header);
  memset(&info, 0, sizeof(info));

  (void)internal::fn_format(org_name,
                            identifier.getPath().c_str(), 
                            "",
                            MI_NAME_IEXT,
                            MY_UNPACK_FILENAME);
  if (!realpath(org_name,rp_buff))
    internal::my_load_path(rp_buff,org_name, NULL);
  rp_buff[FN_REFLEN-1]= '\0';
  strcpy(name_buff,rp_buff);
  THR_LOCK_myisam.lock();
  if (!(old_info=test_if_reopen(name_buff)))
  {
    share= &share_buff;
    memset(&share_buff, 0, sizeof(share_buff));
    share_buff.state.rec_per_key_part=rec_per_key_part;
    share_buff.state.key_root=key_root;
    share_buff.state.key_del=key_del;
    share_buff.setKeyCache();

    if ((kfile=internal::my_open(name_buff,(open_mode=O_RDWR),MYF(0))) < 0)
    {
      if ((errno != EROFS && errno != EACCES) ||
	  mode != O_RDONLY ||
	  (kfile=internal::my_open(name_buff,(open_mode=O_RDONLY),MYF(0))) < 0)
	goto err;
    }
    share->mode=open_mode;
    errpos=1;
    if (internal::my_read(kfile, share->state.header.file_version, head_length,
		MYF(MY_NABP)))
    {
      errno= HA_ERR_NOT_A_TABLE;
      goto err;
    }
    if (memcmp(share->state.header.file_version, myisam_file_magic, 4))
    {
      errno=HA_ERR_NOT_A_TABLE;
      goto err;
    }
    share->options= mi_uint2korr(share->state.header.options);
    static const uint64_t OLD_FILE_OPTIONS= HA_OPTION_PACK_RECORD |
	    HA_OPTION_PACK_KEYS |
	    HA_OPTION_COMPRESS_RECORD | HA_OPTION_READ_ONLY_DATA |
	    HA_OPTION_TEMP_COMPRESS_RECORD |
	    HA_OPTION_TMP_TABLE;
    if (share->options & ~OLD_FILE_OPTIONS)
    {
      errno=HA_ERR_OLD_FILE;
      goto err;
    }

    /* Don't call realpath() if the name can't be a link */
    ssize_t sym_link_size= readlink(org_name,index_name,FN_REFLEN-1);
    if (sym_link_size >= 0 )
      index_name[sym_link_size]= '\0';
    if (!strcmp(name_buff, org_name) || sym_link_size == -1)
      (void) strcpy(index_name, org_name);
    *strrchr(org_name, '.')= '\0';
    (void) internal::fn_format(data_name,org_name,"",MI_NAME_DEXT,
                     MY_APPEND_EXT|MY_UNPACK_FILENAME|MY_RESOLVE_SYMLINKS);

    info_length=mi_uint2korr(share->state.header.header_length);
    base_pos=mi_uint2korr(share->state.header.base_pos);
    if (!(disk_cache= (unsigned char*) malloc(info_length+128)))
    {
      errno=ENOMEM;
      goto err;
    }
    end_pos=disk_cache+info_length;
    errpos=2;

    lseek(kfile,0,SEEK_SET);
    errpos=3;
    if (internal::my_read(kfile,disk_cache,info_length,MYF(MY_NABP)))
    {
      errno=HA_ERR_CRASHED;
      goto err;
    }
    len=mi_uint2korr(share->state.header.state_info_length);
    keys=    (uint) share->state.header.keys;
    uniques= (uint) share->state.header.uniques;
    key_parts= mi_uint2korr(share->state.header.key_parts);
    unique_key_parts= mi_uint2korr(share->state.header.unique_key_parts);
    share->state_diff_length=len-MI_STATE_INFO_SIZE;

    mi_state_info_read(disk_cache, &share->state);
    len= mi_uint2korr(share->state.header.base_info_length);
    disk_pos= my_n_base_info_read(disk_cache + base_pos, &share->base);
    share->state.state_length=base_pos;

    if (share->state.changed & STATE_CRASHED)
    {
      errno=((share->state.changed & STATE_CRASHED_ON_REPAIR) ?
		HA_ERR_CRASHED_ON_REPAIR : HA_ERR_CRASHED_ON_USAGE);
      goto err;
    }

    /* sanity check */
    if (share->base.keystart > 65535 || share->base.rec_reflength > 8)
    {
      errno=HA_ERR_CRASHED;
      goto err;
    }

    if (share->base.max_key_length > MI_MAX_KEY_BUFF || keys > MI_MAX_KEY ||
	key_parts > MI_MAX_KEY * MI_MAX_KEY_SEG)
    {
      errno=HA_ERR_UNSUPPORTED;
      goto err;
    }

    /* Correct max_file_length based on length of sizeof(off_t) */
    max_data_file_length=
      (share->options & (HA_OPTION_PACK_RECORD | HA_OPTION_COMPRESS_RECORD)) ?
      (((uint64_t) 1 << (share->base.rec_reflength*8))-1) :
      (mi_safe_mul(share->base.pack_reclength,
		   (uint64_t) 1 << (share->base.rec_reflength*8))-1);
    max_key_file_length=
      mi_safe_mul(MI_MIN_KEY_BLOCK_LENGTH,
		  ((uint64_t) 1 << (share->base.key_reflength*8))-1);
#if SIZEOF_OFF_T == 4
    set_if_smaller(max_data_file_length, INT32_MAX);
    set_if_smaller(max_key_file_length, INT32_MAX);
#endif
    if (share->base.raid_type)
    {
      errno=HA_ERR_UNSUPPORTED;
      goto err;
    }
    share->base.max_data_file_length=(internal::my_off_t) max_data_file_length;
    share->base.max_key_file_length=(internal::my_off_t) max_key_file_length;

    if (share->options & HA_OPTION_COMPRESS_RECORD)
      share->base.max_key_length+=2;	/* For safety */

    /* Add space for node pointer */
    share->base.max_key_length+= share->base.key_reflength;

    if (!drizzled::memory::multi_malloc(false,
           &share,sizeof(*share),
           &share->state.rec_per_key_part,sizeof(long)*key_parts,
           &share->keyinfo,keys*sizeof(MI_KEYDEF),
           &share->uniqueinfo,uniques*sizeof(MI_UNIQUEDEF),
           &share->keyparts,
           (key_parts+unique_key_parts+keys+uniques) * sizeof(HA_KEYSEG),
           &share->rec, (share->base.fields+1)*sizeof(MI_COLUMNDEF),
           &share->blobs,sizeof(MI_BLOB)*share->base.blobs,
           &share->unique_file_name,strlen(name_buff)+1,
           &share->index_file_name,strlen(index_name)+1,
           &share->data_file_name,strlen(data_name)+1,
           &share->state.key_root,keys*sizeof(uint64_t),
           &share->state.key_del,
           (share->state.header.max_block_size_index*sizeof(uint64_t)),
           NULL))
      goto err;
    errpos=4;
    *share=share_buff;
    memcpy(share->state.rec_per_key_part, rec_per_key_part,
           sizeof(long)*key_parts);
    memcpy(share->state.key_root, key_root,
           sizeof(internal::my_off_t)*keys);
    memcpy(share->state.key_del, key_del,
           sizeof(internal::my_off_t) * share->state.header.max_block_size_index);
    strcpy(share->unique_file_name, name_buff);
    share->unique_name_length= strlen(name_buff);
    strcpy(share->index_file_name,  index_name);
    strcpy(share->data_file_name,   data_name);

    share->blocksize=min((uint32_t)IO_SIZE,myisam_block_size);
    {
      HA_KEYSEG *pos=share->keyparts;
      for (i=0 ; i < keys ; i++)
      {
        share->keyinfo[i].share= share;
	disk_pos=mi_keydef_read(disk_pos, &share->keyinfo[i]);
        disk_pos_assert(disk_pos + share->keyinfo[i].keysegs * HA_KEYSEG_SIZE,
 			end_pos);
	set_if_smaller(share->blocksize,(uint)share->keyinfo[i].block_length);
	share->keyinfo[i].seg=pos;
	for (j=0 ; j < share->keyinfo[i].keysegs; j++,pos++)
	{
	  disk_pos=mi_keyseg_read(disk_pos, pos);
          if (pos->flag & HA_BLOB_PART &&
              ! (share->options & (HA_OPTION_COMPRESS_RECORD |
                                   HA_OPTION_PACK_RECORD)))
          {
            errno= HA_ERR_CRASHED;
            goto err;
          }
	  if (pos->type == HA_KEYTYPE_TEXT ||
              pos->type == HA_KEYTYPE_VARTEXT1 ||
              pos->type == HA_KEYTYPE_VARTEXT2)
	  {
	    if (!pos->language)
	      pos->charset=default_charset_info;
	    else if (!(pos->charset= get_charset(pos->language)))
	    {
	      errno=HA_ERR_UNKNOWN_CHARSET;
	      goto err;
	    }
	  }
	  else if (pos->type == HA_KEYTYPE_BINARY)
	    pos->charset= &my_charset_bin;
	}
        setup_key_functions(share->keyinfo+i);
	share->keyinfo[i].end=pos;
	pos->type=HA_KEYTYPE_END;			/* End */
	pos->length=share->base.rec_reflength;
	pos->null_bit=0;
	pos->flag=0;					/* For purify */
	pos++;
      }
      for (i=0 ; i < uniques ; i++)
      {
	disk_pos=mi_uniquedef_read(disk_pos, &share->uniqueinfo[i]);
        disk_pos_assert(disk_pos + share->uniqueinfo[i].keysegs *
			HA_KEYSEG_SIZE, end_pos);
	share->uniqueinfo[i].seg=pos;
	for (j=0 ; j < share->uniqueinfo[i].keysegs; j++,pos++)
	{
	  disk_pos=mi_keyseg_read(disk_pos, pos);
	  if (pos->type == HA_KEYTYPE_TEXT ||
              pos->type == HA_KEYTYPE_VARTEXT1 ||
              pos->type == HA_KEYTYPE_VARTEXT2)
	  {
	    if (!pos->language)
	      pos->charset=default_charset_info;
	    else if (!(pos->charset= get_charset(pos->language)))
	    {
	      errno=HA_ERR_UNKNOWN_CHARSET;
	      goto err;
	    }
	  }
	}
	share->uniqueinfo[i].end=pos;
	pos->type=HA_KEYTYPE_END;			/* End */
	pos->null_bit=0;
	pos->flag=0;
	pos++;
      }
    }

    disk_pos_assert(disk_pos + share->base.fields *MI_COLUMNDEF_SIZE, end_pos);
    for (i=j=offset=0 ; i < share->base.fields ; i++)
    {
      disk_pos=mi_recinfo_read(disk_pos,&share->rec[i]);
      share->rec[i].pack_type=0;
      share->rec[i].huff_tree=0;
      share->rec[i].offset=offset;
      if (share->rec[i].type == (int) FIELD_BLOB)
      {
	share->blobs[j].pack_length=
	  share->rec[i].length-portable_sizeof_char_ptr;
	share->blobs[j].offset=offset;
	j++;
      }
      offset+=share->rec[i].length;
    }
    share->rec[i].type=(int) FIELD_LAST;	/* End marker */
    if (offset > share->base.reclength)
    {
      errno= HA_ERR_CRASHED;
      goto err;
    }

    if (! lock_error)
    {
      lock_error=1;			/* Database unlocked */
    }

    if (mi_open_datafile(&info, share, -1))
      goto err;
    errpos=5;

    share->kfile=kfile;
    share->this_process=(ulong) getpid();
    share->last_process= share->state.process;
    share->base.key_parts=key_parts;
    share->base.all_key_parts=key_parts+unique_key_parts;
    if (!(share->last_version=share->state.version))
      share->last_version=1;			/* Safety */
    share->rec_reflength=share->base.rec_reflength; /* May be changed */
    share->base.margin_key_file_length=(share->base.max_key_file_length -
					(keys ? MI_INDEX_BLOCK_MARGIN *
					 share->blocksize * keys : 0));
    share->blocksize=min((uint32_t)IO_SIZE,myisam_block_size);
    share->data_file_type=STATIC_RECORD;
    if (share->options & HA_OPTION_PACK_RECORD)
      share->data_file_type = DYNAMIC_RECORD;
    free(disk_cache);
    disk_cache= NULL;
    mi_setup_functions(share);
    share->is_log_table= false;
    if (myisam_concurrent_insert)
    {
      share->concurrent_insert=
	((share->options & (HA_OPTION_READ_ONLY_DATA | HA_OPTION_TMP_TABLE |
			   HA_OPTION_COMPRESS_RECORD |
			   HA_OPTION_TEMP_COMPRESS_RECORD)) ||
	 (open_flags & HA_OPEN_TMP_TABLE) || have_rtree) ? 0 : 1;
      if (share->concurrent_insert)
      {
        assert(0);
      }
    }
  }
  else
  {
    share= old_info->s;
    if (mode == O_RDWR && share->mode == O_RDONLY)
    {
      errno=EACCES;				/* Can't open in write mode */
      goto err;
    }
    if (mi_open_datafile(&info, share, old_info->dfile))
      goto err;
    errpos=5;
    have_rtree= old_info->rtree_recursion_state != NULL;
  }

  /* alloc and set up private structure parts */
  if (!drizzled::memory::multi_malloc(MY_WME,
         &m_info,sizeof(MI_INFO),
         &info.blobs,sizeof(MI_BLOB)*share->base.blobs,
         &info.buff,(share->base.max_key_block_length*2+
                     share->base.max_key_length),
         &info.lastkey,share->base.max_key_length*3+1,
         &info.first_mbr_key, share->base.max_key_length,
         &info.filename, identifier.getPath().length()+1,
         &info.rtree_recursion_state,have_rtree ? 1024 : 0,
         NULL))
    goto err;
  errpos=6;

  if (!have_rtree)
    info.rtree_recursion_state= NULL;

  strcpy(info.filename, identifier.getPath().c_str());
  memcpy(info.blobs,share->blobs,sizeof(MI_BLOB)*share->base.blobs);
  info.lastkey2=info.lastkey+share->base.max_key_length;

  info.s=share;
  info.lastpos= HA_OFFSET_ERROR;
  info.update= (short) (HA_STATE_NEXT_FOUND+HA_STATE_PREV_FOUND);
  info.opt_flag=READ_CHECK_USED;
  info.this_unique= (ulong) info.dfile; /* Uniq number in process */
  if (share->data_file_type == COMPRESSED_RECORD)
    info.this_unique= share->state.unique;
  info.this_loop=0;				/* Update counter */
  info.last_unique= share->state.unique;
  info.last_loop=   share->state.update_count;
  if (mode == O_RDONLY)
    share->options|=HA_OPTION_READ_ONLY_DATA;
  info.lock_type=F_UNLCK;
  info.quick_mode=0;
  info.bulk_insert=0;
  info.errkey= -1;
  info.page_changed=1;
  info.read_record=share->read_record;
  share->reopen++;
  share->write_flag=MYF(MY_NABP | MY_WAIT_IF_FULL);
  if (share->options & HA_OPTION_READ_ONLY_DATA)
  {
    info.lock_type=F_RDLCK;
    share->r_locks++;
    share->tot_locks++;
  }
  if ((open_flags & HA_OPEN_TMP_TABLE) ||
      (share->options & HA_OPTION_TMP_TABLE))
  {
    share->temporary=share->delay_key_write=1;
    share->write_flag=MYF(MY_NABP);
    /*
     * The following two statements are commented out as a fix of
     * bug https://bugs.launchpad.net/drizzle/+bug/387627
     *
     * UPDATE can be TRUNCATE on TEMPORARY TABLE (MyISAM).
     * The root cause of why this makes a difference hasn't
     * been found, but this fixes things for now.
     */
//    share->w_locks++;			// We don't have to update status
//    share->tot_locks++;
    info.lock_type=F_WRLCK;
  }

  share->delay_key_write= 1;
  info.state= &share->state.state;	/* Change global values by default */

  /* Allocate buffer for one record */

  /* prerequisites: memset(info, 0) && info->s=share; are met. */
  if (!mi_alloc_rec_buff(&info, SIZE_MAX, &info.rec_buff))
    goto err;
  memset(info.rec_buff, 0, mi_get_rec_buff_len(&info, info.rec_buff));

  *m_info=info;
  myisam_open_list.push_front(m_info);

  THR_LOCK_myisam.unlock();
  return(m_info);

err:
  free(disk_cache);
  save_errno=errno ? errno : HA_ERR_END_OF_FILE;
  if ((save_errno == HA_ERR_CRASHED) ||
      (save_errno == HA_ERR_CRASHED_ON_USAGE) ||
      (save_errno == HA_ERR_CRASHED_ON_REPAIR))
    mi_report_error(save_errno, identifier.getPath().c_str());
  switch (errpos) {
  case 6:
    free((unsigned char*) m_info);
    /* fall through */
  case 5:
    internal::my_close(info.dfile,MYF(0));
    if (old_info)
      break;					/* Don't remove open table */
    /* fall through */
  case 4:
    free((unsigned char*) share);
    /* fall through */
  case 3:
    /* fall through */
  case 1:
    internal::my_close(kfile,MYF(0));
    /* fall through */
  case 0:
  default:
    break;
  }
  THR_LOCK_myisam.unlock();
  errno=save_errno;
  return (NULL);
} /* mi_open */


unsigned char *mi_alloc_rec_buff(MI_INFO *info, size_t length, unsigned char **buf)
{
  uint32_t extra;
  uint32_t old_length= 0;

  if (! *buf || length > (old_length=mi_get_rec_buff_len(info, *buf)))
  {
    unsigned char *newptr = *buf;

    /* to simplify initial init of info->rec_buf in mi_open and mi_extra */
    if (length == SIZE_MAX)
    {
      if (info->s->options & HA_OPTION_COMPRESS_RECORD)
        length= max(info->s->base.pack_reclength, info->s->max_pack_length);
      else
        length= info->s->base.pack_reclength;
      length= max((uint32_t)length, info->s->base.max_key_length);
      /* Avoid unnecessary realloc */
      if (newptr && length == old_length)
	return newptr;
    }

    extra= ((info->s->options & HA_OPTION_PACK_RECORD) ?
	    ALIGN_SIZE(MI_MAX_DYN_BLOCK_HEADER)+MI_SPLIT_LENGTH+
	    MI_REC_BUFF_OFFSET : 0);
    if (extra && newptr)
      newptr-= MI_REC_BUFF_OFFSET;
    void *tmpnewptr= NULL;
    if (!(tmpnewptr= realloc(newptr, length+extra+8))) 
      return newptr;
    newptr= (unsigned char *)tmpnewptr;
    *((uint32_t *) newptr)= (uint32_t) length;
    *buf= newptr+(extra ?  MI_REC_BUFF_OFFSET : 0);
  }
  return *buf;
}


static uint64_t mi_safe_mul(uint64_t a, uint64_t b)
{
  uint64_t max_val= ~ (uint64_t) 0;		/* internal::my_off_t is unsigned */

  if (!a || max_val / a < b)
    return max_val;
  return a*b;
}

	/* Set up functions in structs */

void mi_setup_functions(register MYISAM_SHARE *share)
{
  if (share->options & HA_OPTION_PACK_RECORD)
  {
    share->read_record=_mi_read_dynamic_record;
    share->read_rnd=_mi_read_rnd_dynamic_record;
    share->delete_record=_mi_delete_dynamic_record;
    share->compare_record=_mi_cmp_dynamic_record;
    share->compare_unique=_mi_cmp_dynamic_unique;
    share->calc_checksum= mi_checksum;

    /* add bits used to pack data to pack_reclength for faster allocation */
    share->base.pack_reclength+= share->base.pack_bits;
    if (share->base.blobs)
    {
      share->update_record=_mi_update_blob_record;
      share->write_record=_mi_write_blob_record;
    }
    else
    {
      share->write_record=_mi_write_dynamic_record;
      share->update_record=_mi_update_dynamic_record;
    }
  }
  else
  {
    share->read_record=_mi_read_static_record;
    share->read_rnd=_mi_read_rnd_static_record;
    share->delete_record=_mi_delete_static_record;
    share->compare_record=_mi_cmp_static_record;
    share->update_record=_mi_update_static_record;
    share->write_record=_mi_write_static_record;
    share->compare_unique=_mi_cmp_static_unique;
    share->calc_checksum= mi_static_checksum;
  }
  share->file_read= mi_nommap_pread;
  share->file_write= mi_nommap_pwrite;
  share->calc_checksum=0;
}


static void setup_key_functions(register MI_KEYDEF *keyinfo)
{
  {
    keyinfo->ck_insert = _mi_ck_write;
    keyinfo->ck_delete = _mi_ck_delete;
  }
  if (keyinfo->flag & HA_BINARY_PACK_KEY)
  {						/* Simple prefix compression */
    keyinfo->bin_search=_mi_seq_search;
    keyinfo->get_key=_mi_get_binary_pack_key;
    keyinfo->pack_key=_mi_calc_bin_pack_key_length;
    keyinfo->store_key=_mi_store_bin_pack_key;
  }
  else if (keyinfo->flag & HA_VAR_LENGTH_KEY)
  {
    keyinfo->get_key= _mi_get_pack_key;
    if (keyinfo->seg[0].flag & HA_PACK_KEY)
    {						/* Prefix compression */
      /*
        _mi_prefix_search() compares end-space against ASCII blank (' ').
        It cannot be used for character sets, that do not encode the
        blank character like ASCII does. UCS2 is an example. All
        character sets with a fixed width > 1 or a mimimum width > 1
        cannot represent blank like ASCII does. In these cases we have
        to use _mi_seq_search() for the search.
      */
      if (!keyinfo->seg->charset || use_strnxfrm(keyinfo->seg->charset) ||
          (keyinfo->seg->flag & HA_NULL_PART) ||
          (keyinfo->seg->charset->mbminlen > 1))
        keyinfo->bin_search=_mi_seq_search;
      else
        keyinfo->bin_search=_mi_prefix_search;
      keyinfo->pack_key=_mi_calc_var_pack_key_length;
      keyinfo->store_key=_mi_store_var_pack_key;
    }
    else
    {
      keyinfo->bin_search=_mi_seq_search;
      keyinfo->pack_key=_mi_calc_var_key_length; /* Variable length key */
      keyinfo->store_key=_mi_store_static_key;
    }
  }
  else
  {
    keyinfo->bin_search=_mi_bin_search;
    keyinfo->get_key=_mi_get_static_key;
    keyinfo->pack_key=_mi_calc_static_key_length;
    keyinfo->store_key=_mi_store_static_key;
  }
  return;
}


/*
   Function to save and store the header in the index file (.MYI)
*/

uint32_t mi_state_info_write(int file, MI_STATE_INFO *state, uint32_t pWrite)
{
  unsigned char  buff[MI_STATE_INFO_SIZE + MI_STATE_EXTRA_SIZE];
  unsigned char *ptr=buff;
  uint	i, keys= (uint) state->header.keys,
	key_blocks=state->header.max_block_size_index;

  memcpy(ptr,&state->header,sizeof(state->header));
  ptr+=sizeof(state->header);

  /* open_count must be first because of _mi_mark_file_changed ! */
  mi_int2store(ptr,state->open_count);		ptr +=2;
  *ptr++= (unsigned char)state->changed; *ptr++= state->sortkey;
  mi_rowstore(ptr,state->state.records);	ptr +=8;
  mi_rowstore(ptr,state->state.del);		ptr +=8;
  mi_rowstore(ptr,state->split);		ptr +=8;
  mi_sizestore(ptr,state->dellink);		ptr +=8;
  mi_sizestore(ptr,state->state.key_file_length);	ptr +=8;
  mi_sizestore(ptr,state->state.data_file_length);	ptr +=8;
  mi_sizestore(ptr,state->state.empty);		ptr +=8;
  mi_sizestore(ptr,state->state.key_empty);	ptr +=8;
  mi_int8store(ptr,state->auto_increment);	ptr +=8;
  mi_int8store(ptr,(uint64_t) state->state.checksum);ptr +=8;
  mi_int4store(ptr,state->process);		ptr +=4;
  mi_int4store(ptr,state->unique);		ptr +=4;
  mi_int4store(ptr,state->status);		ptr +=4;
  mi_int4store(ptr,state->update_count);	ptr +=4;

  ptr+=state->state_diff_length;

  for (i=0; i < keys; i++)
  {
    mi_sizestore(ptr,state->key_root[i]);	ptr +=8;
  }
  for (i=0; i < key_blocks; i++)
  {
    mi_sizestore(ptr,state->key_del[i]);	ptr +=8;
  }
  if (pWrite & 2)				/* From isamchk */
  {
    uint32_t key_parts= mi_uint2korr(state->header.key_parts);
    mi_int4store(ptr,state->sec_index_changed); ptr +=4;
    mi_int4store(ptr,state->sec_index_used);	ptr +=4;
    mi_int4store(ptr,state->version);		ptr +=4;
    mi_int8store(ptr,state->key_map);		ptr +=8;
    mi_int8store(ptr,(uint64_t) state->create_time);	ptr +=8;
    mi_int8store(ptr,(uint64_t) state->recover_time);	ptr +=8;
    mi_int8store(ptr,(uint64_t) state->check_time);	ptr +=8;
    mi_sizestore(ptr,state->rec_per_key_rows);	ptr+=8;
    for (i=0 ; i < key_parts ; i++)
    {
      mi_int4store(ptr,state->rec_per_key_part[i]);  ptr+=4;
    }
  }

  if (pWrite & 1)
    return(my_pwrite(file, buff, (size_t) (ptr-buff), 0L,
			  MYF(MY_NABP | MY_THREADSAFE)) != 0);
  return(internal::my_write(file, buff, (size_t) (ptr-buff),
		       MYF(MY_NABP)) != 0);
}


static unsigned char *mi_state_info_read(unsigned char *ptr, MI_STATE_INFO *state)
{
  uint32_t i,keys,key_parts,key_blocks;
  memcpy(&state->header,ptr, sizeof(state->header));
  ptr +=sizeof(state->header);
  keys=(uint) state->header.keys;
  key_parts=mi_uint2korr(state->header.key_parts);
  key_blocks=state->header.max_block_size_index;

  state->open_count = mi_uint2korr(ptr);	ptr +=2;
  state->changed= *ptr++;
  state->sortkey = (uint) *ptr++;
  state->state.records= mi_rowkorr(ptr);	ptr +=8;
  state->state.del = mi_rowkorr(ptr);		ptr +=8;
  state->split	= mi_rowkorr(ptr);		ptr +=8;
  state->dellink= mi_sizekorr(ptr);		ptr +=8;
  state->state.key_file_length = mi_sizekorr(ptr);	ptr +=8;
  state->state.data_file_length= mi_sizekorr(ptr);	ptr +=8;
  state->state.empty	= mi_sizekorr(ptr);	ptr +=8;
  state->state.key_empty= mi_sizekorr(ptr);	ptr +=8;
  state->auto_increment=mi_uint8korr(ptr);	ptr +=8;
  state->state.checksum=(ha_checksum) mi_uint8korr(ptr);	ptr +=8;
  state->process= mi_uint4korr(ptr);		ptr +=4;
  state->unique = mi_uint4korr(ptr);		ptr +=4;
  state->status = mi_uint4korr(ptr);		ptr +=4;
  state->update_count=mi_uint4korr(ptr);	ptr +=4;

  ptr+= state->state_diff_length;

  for (i=0; i < keys; i++)
  {
    state->key_root[i]= mi_sizekorr(ptr);	ptr +=8;
  }
  for (i=0; i < key_blocks; i++)
  {
    state->key_del[i] = mi_sizekorr(ptr);	ptr +=8;
  }
  state->sec_index_changed = mi_uint4korr(ptr); ptr +=4;
  state->sec_index_used =    mi_uint4korr(ptr); ptr +=4;
  state->version     = mi_uint4korr(ptr);	ptr +=4;
  state->key_map     = mi_uint8korr(ptr);	ptr +=8;
  state->create_time = (time_t) mi_sizekorr(ptr);	ptr +=8;
  state->recover_time =(time_t) mi_sizekorr(ptr);	ptr +=8;
  state->check_time =  (time_t) mi_sizekorr(ptr);	ptr +=8;
  state->rec_per_key_rows=mi_sizekorr(ptr);	ptr +=8;
  for (i=0 ; i < key_parts ; i++)
  {
    state->rec_per_key_part[i]= mi_uint4korr(ptr); ptr+=4;
  }
  return ptr;
}


uint32_t mi_state_info_read_dsk(int file, MI_STATE_INFO *state, bool pRead)
{
  unsigned char	buff[MI_STATE_INFO_SIZE + MI_STATE_EXTRA_SIZE];

  if (pRead)
  {
    if (my_pread(file, buff, state->state_length,0L, MYF(MY_NABP)))
      return 1;
  }
  else if (internal::my_read(file, buff, state->state_length,MYF(MY_NABP)))
    return 1;
  mi_state_info_read(buff, state);

  return 0;
}


/****************************************************************************
**  store and read of MI_BASE_INFO
****************************************************************************/

uint32_t mi_base_info_write(int file, MI_BASE_INFO *base)
{
  unsigned char buff[MI_BASE_INFO_SIZE], *ptr=buff;

  mi_sizestore(ptr,base->keystart);			ptr +=8;
  mi_sizestore(ptr,base->max_data_file_length);		ptr +=8;
  mi_sizestore(ptr,base->max_key_file_length);		ptr +=8;
  mi_rowstore(ptr,base->records);			ptr +=8;
  mi_rowstore(ptr,base->reloc);				ptr +=8;
  mi_int4store(ptr,base->mean_row_length);		ptr +=4;
  mi_int4store(ptr,base->reclength);			ptr +=4;
  mi_int4store(ptr,base->pack_reclength);		ptr +=4;
  mi_int4store(ptr,base->min_pack_length);		ptr +=4;
  mi_int4store(ptr,base->max_pack_length);		ptr +=4;
  mi_int4store(ptr,base->min_block_length);		ptr +=4;
  mi_int4store(ptr,base->fields);			ptr +=4;
  mi_int4store(ptr,base->pack_fields);			ptr +=4;
  *ptr++=base->rec_reflength;
  *ptr++=base->key_reflength;
  *ptr++=base->keys;
  *ptr++=base->auto_key;
  mi_int2store(ptr,base->pack_bits);			ptr +=2;
  mi_int2store(ptr,base->blobs);			ptr +=2;
  mi_int2store(ptr,base->max_key_block_length);		ptr +=2;
  mi_int2store(ptr,base->max_key_length);		ptr +=2;
  mi_int2store(ptr,base->extra_alloc_bytes);		ptr +=2;
  *ptr++= base->extra_alloc_procent;
  /* old raid info  slots */
  *ptr++= 0;
  mi_int2store(ptr,UINT16_C(0));			ptr +=2;
  mi_int4store(ptr,UINT32_C(0));         		ptr +=4;

  memset(ptr, 0, 6);					ptr +=6; /* extra */
  return internal::my_write(file, buff, (size_t) (ptr-buff), MYF(MY_NABP)) != 0;
}


static unsigned char *my_n_base_info_read(unsigned char *ptr, MI_BASE_INFO *base)
{
  base->keystart = mi_sizekorr(ptr);			ptr +=8;
  base->max_data_file_length = mi_sizekorr(ptr);	ptr +=8;
  base->max_key_file_length = mi_sizekorr(ptr);		ptr +=8;
  base->records =  (ha_rows) mi_sizekorr(ptr);		ptr +=8;
  base->reloc = (ha_rows) mi_sizekorr(ptr);		ptr +=8;
  base->mean_row_length = mi_uint4korr(ptr);		ptr +=4;
  base->reclength = mi_uint4korr(ptr);			ptr +=4;
  base->pack_reclength = mi_uint4korr(ptr);		ptr +=4;
  base->min_pack_length = mi_uint4korr(ptr);		ptr +=4;
  base->max_pack_length = mi_uint4korr(ptr);		ptr +=4;
  base->min_block_length = mi_uint4korr(ptr);		ptr +=4;
  base->fields = mi_uint4korr(ptr);			ptr +=4;
  base->pack_fields = mi_uint4korr(ptr);		ptr +=4;

  base->rec_reflength = *ptr++;
  base->key_reflength = *ptr++;
  base->keys=		*ptr++;
  base->auto_key=	*ptr++;
  base->pack_bits = mi_uint2korr(ptr);			ptr +=2;
  base->blobs = mi_uint2korr(ptr);			ptr +=2;
  base->max_key_block_length= mi_uint2korr(ptr);	ptr +=2;
  base->max_key_length = mi_uint2korr(ptr);		ptr +=2;
  base->extra_alloc_bytes = mi_uint2korr(ptr);		ptr +=2;
  base->extra_alloc_procent = *ptr++;

  /* advance past raid_type (1) raid_chunks (2) and raid_chunksize (4) */
  ptr+= 7;

  ptr+=6;
  return ptr;
}

/*--------------------------------------------------------------------------
  mi_keydef
---------------------------------------------------------------------------*/

uint32_t mi_keydef_write(int file, MI_KEYDEF *keydef)
{
  unsigned char buff[MI_KEYDEF_SIZE];
  unsigned char *ptr=buff;

  *ptr++ = (unsigned char) keydef->keysegs;
  *ptr++ = keydef->key_alg;			/* Rtree or Btree */
  mi_int2store(ptr,keydef->flag);		ptr +=2;
  mi_int2store(ptr,keydef->block_length);	ptr +=2;
  mi_int2store(ptr,keydef->keylength);		ptr +=2;
  mi_int2store(ptr,keydef->minlength);		ptr +=2;
  mi_int2store(ptr,keydef->maxlength);		ptr +=2;
  return internal::my_write(file, buff, (size_t) (ptr-buff), MYF(MY_NABP)) != 0;
}

static unsigned char *mi_keydef_read(unsigned char *ptr, MI_KEYDEF *keydef)
{
   keydef->keysegs	= (uint) *ptr++;
   keydef->key_alg	= *ptr++;		/* Rtree or Btree */

   keydef->flag		= mi_uint2korr(ptr);	ptr +=2;
   keydef->block_length = mi_uint2korr(ptr);	ptr +=2;
   keydef->keylength	= mi_uint2korr(ptr);	ptr +=2;
   keydef->minlength	= mi_uint2korr(ptr);	ptr +=2;
   keydef->maxlength	= mi_uint2korr(ptr);	ptr +=2;
   keydef->block_size_index= keydef->block_length/MI_MIN_KEY_BLOCK_LENGTH-1;
   keydef->underflow_block_length=keydef->block_length/3;
   keydef->version	= 0;			/* Not saved */
   return ptr;
}

/***************************************************************************
**  mi_keyseg
***************************************************************************/

int mi_keyseg_write(int file, const HA_KEYSEG *keyseg)
{
  unsigned char buff[HA_KEYSEG_SIZE];
  unsigned char *ptr=buff;
  ulong pos;

  *ptr++= keyseg->type;
  *ptr++= keyseg->language;
  *ptr++= keyseg->null_bit;
  *ptr++= keyseg->bit_start;
  *ptr++= keyseg->bit_end;
  *ptr++= keyseg->bit_length;
  mi_int2store(ptr,keyseg->flag);	ptr+=2;
  mi_int2store(ptr,keyseg->length);	ptr+=2;
  mi_int4store(ptr,keyseg->start);	ptr+=4;
  pos= keyseg->null_bit ? keyseg->null_pos : keyseg->bit_pos;
  mi_int4store(ptr, pos);
  ptr+=4;

  return internal::my_write(file, buff, (size_t) (ptr-buff), MYF(MY_NABP)) != 0;
}


static unsigned char *mi_keyseg_read(unsigned char *ptr, HA_KEYSEG *keyseg)
{
   keyseg->type		= *ptr++;
   keyseg->language	= *ptr++;
   keyseg->null_bit	= *ptr++;
   keyseg->bit_start	= *ptr++;
   keyseg->bit_end	= *ptr++;
   keyseg->bit_length   = *ptr++;
   keyseg->flag		= mi_uint2korr(ptr);  ptr +=2;
   keyseg->length	= mi_uint2korr(ptr);  ptr +=2;
   keyseg->start	= mi_uint4korr(ptr);  ptr +=4;
   keyseg->null_pos	= mi_uint4korr(ptr);  ptr +=4;
   keyseg->charset=0;				/* Will be filled in later */
   if (keyseg->null_bit)
     keyseg->bit_pos= (uint16_t)(keyseg->null_pos + (keyseg->null_bit == 7));
   else
   {
     keyseg->bit_pos= (uint16_t)keyseg->null_pos;
     keyseg->null_pos= 0;
   }
   return ptr;
}

/*--------------------------------------------------------------------------
  mi_uniquedef
---------------------------------------------------------------------------*/

uint32_t mi_uniquedef_write(int file, MI_UNIQUEDEF *def)
{
  unsigned char buff[MI_UNIQUEDEF_SIZE];
  unsigned char *ptr=buff;

  mi_int2store(ptr,def->keysegs);		ptr+=2;
  *ptr++=  (unsigned char) def->key;
  *ptr++ = (unsigned char) def->null_are_equal;

  return internal::my_write(file, buff, (size_t) (ptr-buff), MYF(MY_NABP)) != 0;
}

static unsigned char *mi_uniquedef_read(unsigned char *ptr, MI_UNIQUEDEF *def)
{
   def->keysegs = mi_uint2korr(ptr);
   def->key	= ptr[2];
   def->null_are_equal=ptr[3];
   return ptr+4;				/* 1 extra byte */
}

/***************************************************************************
**  MI_COLUMNDEF
***************************************************************************/

uint32_t mi_recinfo_write(int file, MI_COLUMNDEF *recinfo)
{
  unsigned char buff[MI_COLUMNDEF_SIZE];
  unsigned char *ptr=buff;

  mi_int2store(ptr,recinfo->type);	ptr +=2;
  mi_int2store(ptr,recinfo->length);	ptr +=2;
  *ptr++ = recinfo->null_bit;
  mi_int2store(ptr,recinfo->null_pos);	ptr+= 2;
  return internal::my_write(file, buff, (size_t) (ptr-buff), MYF(MY_NABP)) != 0;
}

static unsigned char *mi_recinfo_read(unsigned char *ptr, MI_COLUMNDEF *recinfo)
{
   recinfo->type=  mi_sint2korr(ptr);	ptr +=2;
   recinfo->length=mi_uint2korr(ptr);	ptr +=2;
   recinfo->null_bit= (uint8_t) *ptr++;
   recinfo->null_pos=mi_uint2korr(ptr); ptr +=2;
   return ptr;
}

/**************************************************************************
Open data file
We can't use dup() here as the data file descriptors need to have different
active seek-positions.

The argument file_to_dup is here for the future if there would on some OS
exist a dup()-like call that would give us two different file descriptors.
*************************************************************************/

int mi_open_datafile(MI_INFO *info, MYISAM_SHARE *share, int file_to_dup)
{
  (void)file_to_dup; 
  info->dfile=internal::my_open(share->data_file_name, share->mode,
                      MYF(MY_WME));
  return info->dfile >= 0 ? 0 : 1;
}


int mi_open_keyfile(MYISAM_SHARE *share)
{
  if ((share->kfile=internal::my_open(share->unique_file_name, share->mode,
                            MYF(MY_WME))) < 0)
    return 1;
  return 0;
}


/*
  Disable all indexes.

  SYNOPSIS
    mi_disable_indexes()
    info        A pointer to the MyISAM storage engine MI_INFO struct.

  DESCRIPTION
    Disable all indexes.

  RETURN
    0  ok
*/

int mi_disable_indexes(MI_INFO *info)
{
  MYISAM_SHARE *share= info->s;

  mi_clear_all_keys_active(share->state.key_map);
  return 0;
}


/*
  Enable all indexes

  SYNOPSIS
    mi_enable_indexes()
    info        A pointer to the MyISAM storage engine MI_INFO struct.

  DESCRIPTION
    Enable all indexes. The indexes might have been disabled
    by mi_disable_index() before.
    The function works only if both data and indexes are empty,
    otherwise a repair is required.
    To be sure, call handler::delete_all_rows() before.

  RETURN
    0  ok
    HA_ERR_CRASHED data or index is non-empty.
*/

int mi_enable_indexes(MI_INFO *info)
{
  int error= 0;
  MYISAM_SHARE *share= info->s;

  if (share->state.state.data_file_length ||
      (share->state.state.key_file_length != share->base.keystart))
  {
    mi_print_error(info->s, HA_ERR_CRASHED);
    error= HA_ERR_CRASHED;
  }
  else
    mi_set_all_keys_active(share->state.key_map, share->base.keys);
  return error;
}


/*
  Test if indexes are disabled.

  SYNOPSIS
    mi_indexes_are_disabled()
    info        A pointer to the MyISAM storage engine MI_INFO struct.

  DESCRIPTION
    Test if indexes are disabled.

  RETURN
    0  indexes are not disabled
    1  all indexes are disabled
    2  non-unique indexes are disabled
*/

int mi_indexes_are_disabled(MI_INFO *info)
{
  MYISAM_SHARE *share= info->s;

  /*
    No keys or all are enabled. keys is the number of keys. Left shifted
    gives us only one bit set. When decreased by one, gives us all all bits
    up to this one set and it gets unset.
  */
  if (!share->base.keys ||
      (mi_is_all_keys_active(share->state.key_map, share->base.keys)))
    return 0;

  /* All are disabled */
  if (mi_is_any_key_active(share->state.key_map))
    return 1;

  /*
    We have keys. Some enabled, some disabled.
    Don't check for any non-unique disabled but return directly 2
  */
  return 2;
}

