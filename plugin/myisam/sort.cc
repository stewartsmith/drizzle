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

/*
  Creates a index for a database by reading keys, sorting them and outputing
  them in sorted order through SORT_INFO functions.
*/

#include "myisam_priv.h"
#include <stddef.h>
#include <queue>
#include <algorithm>

/* static variables */

#undef MIN_SORT_MEMORY
#undef MYF_RW
#undef DISK_BUFFER_SIZE

#define MERGEBUFF 15
#define MERGEBUFF2 31
#define MIN_SORT_MEMORY (4096-MALLOC_OVERHEAD)
#define MYF_RW  MYF(MY_NABP | MY_WME | MY_WAIT_IF_FULL)
#define DISK_BUFFER_SIZE (IO_SIZE*16)

using namespace std;
using namespace drizzled;


/*
 Pointers of functions for store and read keys from temp file
*/

extern void print_error(const char *fmt,...);

/* Functions defined in this file */

static ha_rows  find_all_keys(MI_SORT_PARAM *info,uint32_t keys,
			      unsigned char **sort_keys,
			      DYNAMIC_ARRAY *buffpek,
			      size_t *maxbuffer,
			      internal::io_cache_st *tempfile,
			      internal::io_cache_st *tempfile_for_exceptions);
static int  write_keys(MI_SORT_PARAM *info,unsigned char **sort_keys,
                             uint32_t count, BUFFPEK *buffpek,internal::io_cache_st *tempfile);
static int  write_key(MI_SORT_PARAM *info, unsigned char *key,
			    internal::io_cache_st *tempfile);
static int  write_index(MI_SORT_PARAM *info,unsigned char * *sort_keys,
                              uint32_t count);
static int  merge_many_buff(MI_SORT_PARAM *info,uint32_t keys,
			    unsigned char * *sort_keys,
			    BUFFPEK *buffpek,size_t *maxbuffer,
			    internal::io_cache_st *t_file);
static uint32_t  read_to_buffer(internal::io_cache_st *fromfile,BUFFPEK *buffpek,
                                  uint32_t sort_length);
static int  merge_buffers(MI_SORT_PARAM *info,uint32_t keys,
                                internal::io_cache_st *from_file, internal::io_cache_st *to_file,
                                unsigned char * *sort_keys, BUFFPEK *lastbuff,
                                BUFFPEK *Fb, BUFFPEK *Tb);
static int  merge_index(MI_SORT_PARAM *,uint,unsigned char **,BUFFPEK *, int,
                              internal::io_cache_st *);
static int  write_keys_varlen(MI_SORT_PARAM *info,unsigned char **sort_keys,
                       uint32_t count, BUFFPEK *buffpek,
                       internal::io_cache_st *tempfile);
static uint32_t  read_to_buffer_varlen(internal::io_cache_st *fromfile,BUFFPEK *buffpek,
                                uint32_t sort_length);
static int  write_merge_key(MI_SORT_PARAM *info, internal::io_cache_st *to_file,
                     unsigned char *key, uint32_t sort_length, uint32_t count);
static int  write_merge_key_varlen(MI_SORT_PARAM *info,
                            internal::io_cache_st *to_file,
                            unsigned char* key, uint32_t sort_length,
                            uint32_t count);

inline int
my_var_write(MI_SORT_PARAM *info, internal::io_cache_st *to_file, unsigned char *bufs);

/*
  Creates a index of sorted keys

  SYNOPSIS
    _create_index_by_sort()
    info		Sort parameters
    no_messages		Set to 1 if no output
    sortbuff_size	Size if sortbuffer to allocate

  RESULT
    0	ok
   <> 0 Error
*/

int _create_index_by_sort(MI_SORT_PARAM *info,bool no_messages,
			  size_t sortbuff_size)
{
  size_t skr;
  uint32_t memavl,keys;
  DYNAMIC_ARRAY buffpek;
  internal::io_cache_st tempfile, tempfile_for_exceptions;

  if (info->keyinfo->flag & HA_VAR_LENGTH_KEY)
  {
    info->write_keys=write_keys_varlen;
    info->read_to_buffer=read_to_buffer_varlen;
    info->write_key= write_merge_key_varlen;
  }
  else
  {
    info->write_keys=write_keys;
    info->read_to_buffer=read_to_buffer;
    info->write_key=write_merge_key;
  }

  my_b_clear(&tempfile);
  my_b_clear(&tempfile_for_exceptions);
  memset(&buffpek, 0, sizeof(buffpek));
  unsigned char** sort_keys= (unsigned char **) NULL; 
  int error= 1;
  size_t maxbuffer=1;

  memavl=max(sortbuff_size,(size_t)MIN_SORT_MEMORY);
  ha_rows records= info->sort_info->max_records;
  uint32_t sort_length= info->key_length;

  while (memavl >= MIN_SORT_MEMORY)
  {
    if ((records < UINT32_MAX) &&
       ((internal::my_off_t) (records + 1) *
        (sort_length + sizeof(char*)) <= (internal::my_off_t) memavl))
      keys= (uint)records+1;
    else
      do
      {
        skr=maxbuffer;
        if (memavl < sizeof(BUFFPEK)* maxbuffer ||
          (keys=(memavl-sizeof(BUFFPEK)* maxbuffer) / (sort_length+sizeof(char*))) <= 1 ||
          keys < maxbuffer)
        {
          mi_check_print_error(info->sort_info->param,
            "myisam_sort_buffer_size is too small");
          goto err;
        }
      }
      while ((maxbuffer= (size_t)(records/(keys-1)+1)) != skr);

    sort_keys=(unsigned char **)malloc(keys*(sort_length+sizeof(char*)));
    my_init_dynamic_array(&buffpek, sizeof(BUFFPEK), maxbuffer, maxbuffer/2);
    break;
  }
  if (memavl < MIN_SORT_MEMORY)
  {
    mi_check_print_error(info->sort_info->param,"MyISAM sort buffer too small");
    goto err;
  }
  (*info->lock_in_memory)(info->sort_info->param);/* Everything is allocated */

  if (!no_messages)
    printf("  - Searching for keys, allocating buffer for %d keys\n",keys);

  if ((records=find_all_keys(info,keys,sort_keys,&buffpek,&maxbuffer,
			     &tempfile,&tempfile_for_exceptions))
      == HA_POS_ERROR)
    goto err;
  if (maxbuffer == 0)
  {
    if (!no_messages)
      printf("  - Dumping %u keys\n", (uint32_t) records);
    if (write_index(info,sort_keys, (uint) records))
      goto err;
  }
  else
  {
    keys=(keys*(sort_length+sizeof(char*)))/sort_length;
    if (maxbuffer >= MERGEBUFF2)
    {
      if (!no_messages)
	printf("  - Merging %u keys\n", (uint32_t) records);
      if (merge_many_buff(info,keys,sort_keys, (BUFFPEK*)buffpek.buffer, &maxbuffer, &tempfile))
	goto err;
    }
    if (internal::flush_io_cache(&tempfile) ||
	tempfile.reinit_io_cache(internal::READ_CACHE,0L,0,0))
      goto err;
    if (!no_messages)
      printf("  - Last merge and dumping keys\n");
    if (merge_index(info,keys,sort_keys, (BUFFPEK*)buffpek.buffer, maxbuffer, &tempfile))
      goto err;
  }

  if (flush_pending_blocks(info))
    goto err;

  if (my_b_inited(&tempfile_for_exceptions))
  {
    MI_INFO *idx=info->sort_info->info;
    uint32_t     keyno=info->key;
    uint32_t     key_length, ref_length=idx->s->rec_reflength;

    if (not no_messages)
      printf("  - Adding exceptions\n");

    if (flush_io_cache(&tempfile_for_exceptions) || tempfile_for_exceptions.reinit_io_cache(internal::READ_CACHE,0L,0,0))
    {
      goto err;
    }

    while (!my_b_read(&tempfile_for_exceptions,(unsigned char*)&key_length,
		      sizeof(key_length))
        && !my_b_read(&tempfile_for_exceptions,(unsigned char*)sort_keys,
		      (uint) key_length))
    {
	if (_mi_ck_write(idx,keyno,(unsigned char*) sort_keys,key_length-ref_length))
	  goto err;
    }
  }

  error =0;

err:
  free(sort_keys);
  delete_dynamic(&buffpek);
  tempfile.close_cached_file();
  tempfile_for_exceptions.close_cached_file();

  return(error ? -1 : 0);
} /* _create_index_by_sort */


/* Search after all keys and place them in a temp. file */

static ha_rows  find_all_keys(MI_SORT_PARAM *info, uint32_t keys,
			      unsigned char **sort_keys,
			      DYNAMIC_ARRAY *buffpek,
			      size_t *maxbuffer, internal::io_cache_st *tempfile,
			      internal::io_cache_st *tempfile_for_exceptions)
{
  int error;
  uint32_t idx;

  idx=error=0;
  sort_keys[0]=(unsigned char*) (sort_keys+keys);

  while (!(error=(*info->key_read)(info,sort_keys[idx])))
  {
    if (info->real_key_length > info->key_length)
    {
      if (write_key(info,sort_keys[idx],tempfile_for_exceptions))
        return(HA_POS_ERROR);
      continue;
    }

    if (++idx == keys)
    {
      if (info->write_keys(info,sort_keys,idx-1,(BUFFPEK *)alloc_dynamic(buffpek),
		     tempfile))
      return(HA_POS_ERROR);

      sort_keys[0]=(unsigned char*) (sort_keys+keys);
      memcpy(sort_keys[0],sort_keys[idx-1],(size_t) info->key_length);
      idx=1;
    }
    sort_keys[idx]=sort_keys[idx-1]+info->key_length;
  }
  if (error > 0)
    return(HA_POS_ERROR);
  if (buffpek->size())
  {
    if (info->write_keys(info,sort_keys,idx,(BUFFPEK *)alloc_dynamic(buffpek),
		   tempfile))
      return(HA_POS_ERROR);
    *maxbuffer=buffpek->size() - 1;
  }
  else
    *maxbuffer=0;

  return((*maxbuffer)*(keys-1)+idx);
} /* find_all_keys */


int thr_write_keys(MI_SORT_PARAM *sort_param)
{
  SORT_INFO *sort_info= sort_param->sort_info;
  MI_CHECK *param= sort_info->param;
  uint32_t length= 0, keys;
  ulong *rec_per_key_part= param->rec_per_key_part;
  int got_error=sort_info->got_error;
  uint32_t i;
  MI_INFO *info=sort_info->info;
  MYISAM_SHARE *share=info->s;
  MI_SORT_PARAM *sinfo;
  unsigned char *mergebuf=0;

  for (i= 0, sinfo= sort_param ;
       i < sort_info->total_keys ;
       i++, rec_per_key_part+=sinfo->keyinfo->keysegs, sinfo++)
  {
    if (!sinfo->sort_keys)
    {
      got_error=1;
      void * rec_buff_ptr= mi_get_rec_buff_ptr(info, sinfo->rec_buff);
      free(rec_buff_ptr);
      continue;
    }
    if (!got_error)
    {
      mi_set_key_active(share->state.key_map, sinfo->key);
      if (!sinfo->buffpek.size())
      {
        if (param->testflag & T_VERBOSE)
        {
          printf("Key %d  - Dumping %u keys\n",sinfo->key+1, sinfo->keys);
          fflush(stdout);
        }
        if (write_index(sinfo, sinfo->sort_keys, sinfo->keys) || flush_pending_blocks(sinfo))
          got_error=1;
      }
      if (!got_error && param->testflag & T_STATISTICS)
        update_key_parts(sinfo->keyinfo, rec_per_key_part, sinfo->unique,
                         param->stats_method == MI_STATS_METHOD_IGNORE_NULLS?
                         sinfo->notnull: NULL,
                         (uint64_t) info->state->records);
    }
    free((unsigned char*) sinfo->sort_keys);
    void * rec_buff_ptr= mi_get_rec_buff_ptr(info, sinfo->rec_buff);
    free(rec_buff_ptr);
    sinfo->sort_keys=0;
  }

  for (i= 0, sinfo= sort_param ;
       i < sort_info->total_keys ;
       i++,
	 delete_dynamic(&sinfo->buffpek),
	 sinfo->tempfile.close_cached_file(),
	 sinfo->tempfile_for_exceptions.close_cached_file(),
	 sinfo++)
  {
    if (got_error)
      continue;
    if (sinfo->keyinfo->flag & HA_VAR_LENGTH_KEY)
    {
      sinfo->write_keys=write_keys_varlen;
      sinfo->read_to_buffer=read_to_buffer_varlen;
      sinfo->write_key=write_merge_key_varlen;
    }
    else
    {
      sinfo->write_keys=write_keys;
      sinfo->read_to_buffer=read_to_buffer;
      sinfo->write_key=write_merge_key;
    }
    if (sinfo->buffpek.size())
    {
      size_t maxbuffer=sinfo->buffpek.size() - 1;
      if (!mergebuf)
      {
        length=param->sort_buffer_length;
        while (length >= MIN_SORT_MEMORY)
        {
          if ((mergebuf= (unsigned char *)malloc(length)))
              break;
          length=length*3/4;
        }
        if (!mergebuf)
        {
          got_error=1;
          continue;
        }
      }
      keys=length/sinfo->key_length;
      if (maxbuffer >= MERGEBUFF2)
      {
        if (param->testflag & T_VERBOSE)
          printf("Key %d  - Merging %u keys\n",sinfo->key+1, sinfo->keys);
        if (merge_many_buff(sinfo, keys, (unsigned char **)mergebuf, (BUFFPEK*)sinfo->buffpek.buffer,
			    &maxbuffer, &sinfo->tempfile))
        {
          got_error=1;
          continue;
        }
      }
      if (flush_io_cache(&sinfo->tempfile) || sinfo->tempfile.reinit_io_cache(internal::READ_CACHE,0L,0,0))
      {
        got_error=1;
        continue;
      }
      if (param->testflag & T_VERBOSE)
        printf("Key %d  - Last merge and dumping keys\n", sinfo->key+1);
      if (merge_index(sinfo, keys, (unsigned char **)mergebuf, (BUFFPEK*)sinfo->buffpek.buffer,
                      maxbuffer,&sinfo->tempfile) ||
	  flush_pending_blocks(sinfo))
      {
        got_error=1;
        continue;
      }
    }
    if (my_b_inited(&sinfo->tempfile_for_exceptions))
    {
      uint32_t key_length;

      if (param->testflag & T_VERBOSE)
        printf("Key %d  - Dumping 'long' keys\n", sinfo->key+1);

      if (flush_io_cache(&sinfo->tempfile_for_exceptions) || sinfo->tempfile_for_exceptions.reinit_io_cache(internal::READ_CACHE,0L,0,0))
      {
        got_error=1;
        continue;
      }

      while (!got_error &&
	     !my_b_read(&sinfo->tempfile_for_exceptions,(unsigned char*)&key_length,
			sizeof(key_length)))
      {
        unsigned char ft_buf[10];
        if (key_length > sizeof(ft_buf) ||
            my_b_read(&sinfo->tempfile_for_exceptions, (unsigned char*)ft_buf,
                      (uint)key_length) ||
            _mi_ck_write(info, sinfo->key, (unsigned char*)ft_buf,
                         key_length - info->s->rec_reflength))
          got_error=1;
      }
    }
  }
  free((unsigned char*) mergebuf);
  return(got_error);
}

        /* Write all keys in memory to file for later merge */

static int  write_keys(MI_SORT_PARAM *info, register unsigned char **sort_keys,
                             uint32_t count, BUFFPEK *buffpek, internal::io_cache_st *tempfile)
{
  unsigned char **end;
  uint32_t sort_length=info->key_length;

  internal::my_qsort2((unsigned char*) sort_keys,count,sizeof(unsigned char*),(qsort2_cmp) info->key_cmp,
            info);
  if (!my_b_inited(tempfile) && tempfile->open_cached_file(P_tmpdir, "ST", DISK_BUFFER_SIZE, info->sort_info->param->myf_rw))
    return(1);

  buffpek->file_pos=my_b_tell(tempfile);
  buffpek->count=count;

  for (end=sort_keys+count ; sort_keys != end ; sort_keys++)
  {
    if (my_b_write(tempfile,(unsigned char*) *sort_keys,(uint) sort_length))
      return(1);
  }
  return(0);
} /* write_keys */


inline int
my_var_write(MI_SORT_PARAM *info, internal::io_cache_st *to_file, unsigned char *bufs)
{
  int err;
  uint16_t len = _mi_keylength(info->keyinfo, (unsigned char*) bufs);

  /* The following is safe as this is a local file */
  if ((err= my_b_write(to_file, (unsigned char*)&len, sizeof(len))))
    return (err);
  if ((err= my_b_write(to_file,bufs, (uint) len)))
    return (err);
  return (0);
}


static int  write_keys_varlen(MI_SORT_PARAM *info,
				    register unsigned char **sort_keys,
                                    uint32_t count, BUFFPEK *buffpek,
				    internal::io_cache_st *tempfile)
{
  unsigned char **end;
  int err;

  internal::my_qsort2((unsigned char*) sort_keys,count,sizeof(unsigned char*),(qsort2_cmp) info->key_cmp,
            info);
  if (!my_b_inited(tempfile) && tempfile->open_cached_file(P_tmpdir, "ST", DISK_BUFFER_SIZE, info->sort_info->param->myf_rw))
    return(1);

  buffpek->file_pos=my_b_tell(tempfile);
  buffpek->count=count;
  for (end=sort_keys+count ; sort_keys != end ; sort_keys++)
  {
    if ((err= my_var_write(info,tempfile, (unsigned char*) *sort_keys)))
      return(err);
  }
  return(0);
} /* write_keys_varlen */


static int  write_key(MI_SORT_PARAM *info, unsigned char *key,
			    internal::io_cache_st *tempfile)
{
  uint32_t key_length=info->real_key_length;

  if (!my_b_inited(tempfile) && tempfile->open_cached_file(P_tmpdir, "ST", DISK_BUFFER_SIZE, info->sort_info->param->myf_rw))
    return(1);

  if (my_b_write(tempfile,(unsigned char*)&key_length,sizeof(key_length)) ||
      my_b_write(tempfile,(unsigned char*)key,(uint) key_length))
    return(1);
  return(0);
} /* write_key */


/* Write index */

static int  write_index(MI_SORT_PARAM *info, register unsigned char **sort_keys,
                              register uint32_t count)
{
  internal::my_qsort2((unsigned char*) sort_keys,(size_t) count,sizeof(unsigned char*),
           (qsort2_cmp) info->key_cmp,info);
  while (count--)
  {
    if ((*info->key_write)(info,*sort_keys++))
      return(-1);
  }
  return(0);
} /* write_index */


        /* Merge buffers to make < MERGEBUFF2 buffers */

static int  merge_many_buff(MI_SORT_PARAM *info, uint32_t keys,
			    unsigned char **sort_keys, BUFFPEK *buffpek,
			    size_t *maxbuffer, internal::io_cache_st *t_file)
{
  uint32_t i;
  internal::io_cache_st t_file2, *from_file, *to_file, *temp;
  BUFFPEK *lastbuff;

  if (*maxbuffer < MERGEBUFF2)
    return(0);
  if (flush_io_cache(t_file) || t_file2.open_cached_file(P_tmpdir, "ST",
                       DISK_BUFFER_SIZE, info->sort_info->param->myf_rw))
    return(1);

  from_file= t_file ; to_file= &t_file2;
  while (*maxbuffer >= MERGEBUFF2)
  {
    from_file->reinit_io_cache(internal::READ_CACHE,0L,0,0);
    to_file->reinit_io_cache(internal::WRITE_CACHE,0L,0,0);
    lastbuff=buffpek;
    for (i=0 ; i <= *maxbuffer-MERGEBUFF*3/2 ; i+=MERGEBUFF)
    {
      if (merge_buffers(info,keys,from_file,to_file,sort_keys,lastbuff++,
                        buffpek+i,buffpek+i+MERGEBUFF-1))
        goto cleanup;
    }
    if (merge_buffers(info,keys,from_file,to_file,sort_keys,lastbuff++,
                      buffpek+i,buffpek+ *maxbuffer))
      break;
    if (flush_io_cache(to_file))
      break;
    temp=from_file; from_file=to_file; to_file=temp;
    *maxbuffer= (int) (lastbuff-buffpek)-1;
  }
cleanup:
  to_file->close_cached_file();                   /* This holds old result */
  if (to_file == t_file)
    *t_file=t_file2;                            /* Copy result file */

  return(*maxbuffer >= MERGEBUFF2);        /* Return 1 if interrupted */
} /* merge_many_buff */


/*
   Read data to buffer

  SYNOPSIS
    read_to_buffer()
    fromfile		File to read from
    buffpek		Where to read from
    sort_length		max length to read
  RESULT
    > 0	Ammount of bytes read
    -1	Error
*/

static uint32_t  read_to_buffer(internal::io_cache_st *fromfile, BUFFPEK *buffpek,
                                  uint32_t sort_length)
{
  register uint32_t count;
  uint32_t length;

  if ((count=(uint) min((ha_rows) buffpek->max_keys,buffpek->count)))
  {
    if (my_pread(fromfile->file,(unsigned char*) buffpek->base,
                 (length= sort_length*count),buffpek->file_pos,MYF_RW))
      return((uint) -1);
    buffpek->key=buffpek->base;
    buffpek->file_pos+= length;                 /* New filepos */
    buffpek->count-=    count;
    buffpek->mem_count= count;
  }
  return (count*sort_length);
} /* read_to_buffer */

static uint32_t  read_to_buffer_varlen(internal::io_cache_st *fromfile, BUFFPEK *buffpek,
                                         uint32_t sort_length)
{
  register uint32_t count;
  uint16_t length_of_key = 0;
  uint32_t idx;
  unsigned char *buffp;

  if ((count=(uint) min((ha_rows) buffpek->max_keys,buffpek->count)))
  {
    buffp = buffpek->base;

    for (idx=1;idx<=count;idx++)
    {
      if (my_pread(fromfile->file,(unsigned char*)&length_of_key,sizeof(length_of_key),
                   buffpek->file_pos,MYF_RW))
        return((uint) -1);
      buffpek->file_pos+=sizeof(length_of_key);
      if (my_pread(fromfile->file,(unsigned char*) buffp,length_of_key,
                   buffpek->file_pos,MYF_RW))
        return((uint) -1);
      buffpek->file_pos+=length_of_key;
      buffp = buffp + sort_length;
    }
    buffpek->key=buffpek->base;
    buffpek->count-=    count;
    buffpek->mem_count= count;
  }
  return (count*sort_length);
} /* read_to_buffer_varlen */


static int  write_merge_key_varlen(MI_SORT_PARAM *info,
					 internal::io_cache_st *to_file, unsigned char* key,
                                         uint32_t sort_length, uint32_t count)
{
  uint32_t idx;
  unsigned char *bufs = key;

  for (idx=1;idx<=count;idx++)
  {
    int err;
    if ((err= my_var_write(info, to_file, bufs)))
      return (err);
    bufs=bufs+sort_length;
  }
  return(0);
}


static int  write_merge_key(MI_SORT_PARAM *info,
				  internal::io_cache_st *to_file, unsigned char *key,
				  uint32_t sort_length, uint32_t count)
{
  (void)info;
  return my_b_write(to_file, key, (size_t) sort_length*count);
}

/*
 * Function object to be used as the comparison function
 * for the priority queue in the merge_buffers method.
 */
class compare_functor
{
  qsort2_cmp key_compare;
  void *key_compare_arg;
  public:
  compare_functor(qsort2_cmp in_key_compare, void *in_compare_arg)
    : key_compare(in_key_compare), key_compare_arg(in_compare_arg) { }
  inline bool operator()(const BUFFPEK *i, const BUFFPEK *j) const
  {
    int val= key_compare(key_compare_arg,
                      &i->key, &j->key);
    return (val >= 0);
  }
};

/*
  Merge buffers to one buffer
  If to_file == 0 then use info->key_write
*/

static int
merge_buffers(MI_SORT_PARAM *info, uint32_t keys, internal::io_cache_st *from_file,
              internal::io_cache_st *to_file, unsigned char **sort_keys, BUFFPEK *lastbuff,
              BUFFPEK *Fb, BUFFPEK *Tb)
{
  int error;
  uint32_t sort_length,maxcount;
  ha_rows count;
  internal::my_off_t to_start_filepos= 0;
  unsigned char *strpos;
  BUFFPEK *buffpek;
  priority_queue<BUFFPEK *, vector<BUFFPEK *>, compare_functor > 
    queue(compare_functor((qsort2_cmp) info->key_cmp, static_cast<void *>(info)));
  volatile int *killed= killed_ptr(info->sort_info->param);

  count=error=0;
  maxcount=keys/((uint) (Tb-Fb) +1);
  assert(maxcount > 0);
  if (to_file)
    to_start_filepos=my_b_tell(to_file);
  strpos=(unsigned char*) sort_keys;
  sort_length=info->key_length;

  for (buffpek= Fb ; buffpek <= Tb ; buffpek++)
  {
    count+= buffpek->count;
    buffpek->base= strpos;
    buffpek->max_keys=maxcount;
    strpos+= (uint) (error=(int) info->read_to_buffer(from_file,buffpek,
                                                      sort_length));
    if (error == -1)
      goto err;
    queue.push(buffpek);
  }

  while (queue.size() > 1)
  {
    for (;;)
    {
      if (*killed)
      {
        error=1; goto err;
      }
      buffpek= queue.top();
      if (to_file)
      {
        if (info->write_key(info,to_file,(unsigned char*) buffpek->key,
                            (uint) sort_length,1))
        {
          error=1; goto err;
        }
      }
      else
      {
        if ((*info->key_write)(info,(void*) buffpek->key))
        {
          error=1; goto err;
        }
      }
      buffpek->key+=sort_length;
      if (! --buffpek->mem_count)
      {
        if (!(error=(int) info->read_to_buffer(from_file,buffpek,sort_length)))
        {
          queue.pop();
          break;                /* One buffer have been removed */
        }
      }
      else if (error == -1)
        goto err;
      /* Top element has been replaced */
      queue.pop();
      queue.push(buffpek);
    }
  }
  buffpek= queue.top();
  buffpek->base=(unsigned char *) sort_keys;
  buffpek->max_keys=keys;
  do
  {
    if (to_file)
    {
      if (info->write_key(info,to_file,(unsigned char*) buffpek->key,
                         sort_length,buffpek->mem_count))
      {
        error=1; goto err;
      }
    }
    else
    {
      register unsigned char *end;
      strpos= buffpek->key;
      for (end=strpos+buffpek->mem_count*sort_length;
           strpos != end ;
           strpos+=sort_length)
      {
        if ((*info->key_write)(info,(void*) strpos))
        {
          error=1; goto err;
        }
      }
    }
  }
  while ((error=(int) info->read_to_buffer(from_file,buffpek,sort_length)) != -1 &&
         error != 0);

  lastbuff->count=count;
  if (to_file)
    lastbuff->file_pos=to_start_filepos;
err:
  return(error);
} /* merge_buffers */


        /* Do a merge to output-file (save only positions) */

static int
merge_index(MI_SORT_PARAM *info, uint32_t keys, unsigned char **sort_keys,
            BUFFPEK *buffpek, int maxbuffer, internal::io_cache_st *tempfile)
{
  if (merge_buffers(info,keys,tempfile,(internal::io_cache_st*) 0,sort_keys,buffpek,buffpek,
                    buffpek+maxbuffer))
    return(1);
  return(0);
} /* merge_index */

