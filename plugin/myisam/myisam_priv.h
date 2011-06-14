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

/* This file is included by all internal myisam files */

#pragma once

#include <config.h>

#include "myisam.h"			/* Structs & some defines */
#include "myisampack.h"			/* packing of keys */
#include <drizzled/tree.h>
#include <drizzled/internal/my_pthread.h>
#include <drizzled/thr_lock.h>
#include <drizzled/common.h>
#include <drizzled/enum.h>
#include <drizzled/dynamic_array.h>
#include <drizzled/error_t.h>

#include <assert.h>
#include <fcntl.h>
#include <string.h>
#include <list>

#include <boost/thread/mutex.hpp>

#if defined(my_write)
#undef my_write				/* undef map from my_nosys; We need test-if-disk full */
#endif

/* Typical key cash */
static const uint32_t KEY_CACHE_SIZE= 8*1024*1024;

/* Default size of a key cache block  */
static const uint32_t KEY_CACHE_BLOCK_SIZE= 1024;

typedef struct st_mi_status_info
{
  drizzled::ha_rows records;			/* Rows in table */
  drizzled::ha_rows del;				/* Removed rows */
  drizzled::internal::my_off_t empty;			/* lost space in datafile */
  drizzled::internal::my_off_t key_empty;			/* lost space in indexfile */
  drizzled::internal::my_off_t key_file_length;
  drizzled::internal::my_off_t data_file_length;
  ha_checksum checksum;
} MI_STATUS_INFO;

typedef struct st_mi_state_info
{
  struct {				/* Fileheader */
    unsigned char file_version[4];
    unsigned char options[2];
    unsigned char header_length[2];
    unsigned char state_info_length[2];
    unsigned char base_info_length[2];
    unsigned char base_pos[2];
    unsigned char key_parts[2];			/* Key parts */
    unsigned char unique_key_parts[2];		/* Key parts + unique parts */
    unsigned char keys;				/* number of keys in file */
    unsigned char uniques;			/* number of UNIQUE definitions */
    unsigned char language;			/* Language for indexes */
    unsigned char max_block_size_index;		/* max keyblock size */
    unsigned char fulltext_keys;
    unsigned char not_used;                     /* To align to 8 */
  } header;

  MI_STATUS_INFO state;
  drizzled::ha_rows split;			/* number of split blocks */
  drizzled::internal::my_off_t dellink;			/* Link to next removed block */
  uint64_t auto_increment;
  ulong process;			/* process that updated table last */
  ulong unique;				/* Unique number for this process */
  ulong update_count;			/* Updated for each write lock */
  ulong status;
  ulong *rec_per_key_part;
  drizzled::internal::my_off_t *key_root;			/* Start of key trees */
  drizzled::internal::my_off_t *key_del;			/* delete links for trees */
  drizzled::internal::my_off_t rec_per_key_rows;		/* Rows when calculating rec_per_key */

  ulong sec_index_changed;		/* Updated when new sec_index */
  ulong sec_index_used;			/* which extra index are in use */
  uint64_t key_map;			/* Which keys are in use */
  ha_checksum checksum;                 /* Table checksum */
  ulong version;			/* timestamp of create */
  time_t create_time;			/* Time when created database */
  time_t recover_time;			/* Time for last recover */
  time_t check_time;			/* Time for last check */
  uint	sortkey;			/* sorted by this key  (not used) */
  uint32_t open_count;
  uint8_t changed;			/* Changed since myisamchk */

  /* the following isn't saved on disk */
  uint32_t state_diff_length;		/* Should be 0 */
  uint	state_length;			/* Length of state header in file */
  ulong *key_info;
} MI_STATE_INFO;

#define MI_STATE_INFO_SIZE	(24+14*8+7*4+2*2+8)
#define MI_STATE_KEY_SIZE	8
#define MI_STATE_KEYBLOCK_SIZE  8
#define MI_STATE_KEYSEG_SIZE	4
#define MI_STATE_EXTRA_SIZE ((MI_MAX_KEY+MI_MAX_KEY_BLOCK_SIZE)*MI_STATE_KEY_SIZE + MI_MAX_KEY*MI_MAX_KEY_SEG*MI_STATE_KEYSEG_SIZE)
#define MI_KEYDEF_SIZE		(2+ 5*2)
#define MI_UNIQUEDEF_SIZE	(2+1+1)
#define HA_KEYSEG_SIZE		(6+ 2*2 + 4*2)
#define MI_COLUMNDEF_SIZE	(2*3+1)
#define MI_BASE_INFO_SIZE	(5*8 + 8*4 + 4 + 4*2 + 16)
#define MI_INDEX_BLOCK_MARGIN	16	/* Safety margin for .MYI tables */

typedef struct st_mi_base_info
{
  drizzled::internal::my_off_t keystart;			/* Start of keys */
  drizzled::internal::my_off_t max_data_file_length;
  drizzled::internal::my_off_t max_key_file_length;
  drizzled::internal::my_off_t margin_key_file_length;
  drizzled::ha_rows records,reloc;		/* Create information */
  ulong mean_row_length;		/* Create information */
  ulong reclength;			/* length of unpacked record */
  ulong pack_reclength;			/* Length of full packed rec. */
  ulong min_pack_length;
  ulong max_pack_length;		/* Max possibly length of packed rec.*/
  ulong min_block_length;
  ulong fields,				/* fields in table */
       pack_fields;			/* packed fields in table */
  uint32_t rec_reflength;			/* = 2-8 */
  uint32_t key_reflength;			/* = 2-8 */
  uint32_t keys;				/* same as in state.header */
  uint32_t auto_key;			/* Which key-1 is a auto key */
  uint32_t blobs;				/* Number of blobs */
  uint32_t pack_bits;			/* Length of packed bits */
  uint32_t max_key_block_length;		/* Max block length */
  uint32_t max_key_length;			/* Max key length */
  /* Extra allocation when using dynamic record format */
  uint32_t extra_alloc_bytes;
  uint32_t extra_alloc_procent;
  /* Info about raid */
  uint32_t raid_type,raid_chunks;
  uint32_t raid_chunksize;
  /* The following are from the header */
  uint32_t key_parts,all_key_parts;
} MI_BASE_INFO;


	/* Structs used intern in database */

typedef struct st_mi_blob		/* Info of record */
{
  ulong offset;				/* Offset to blob in record */
  uint32_t pack_length;			/* Type of packed length */
  ulong length;				/* Calc:ed for each record */
} MI_BLOB;


typedef struct st_mi_isam_pack {
  ulong header_length;
  uint32_t ref_length;
  unsigned char version;
} MI_PACK;

#define MAX_NONMAPPED_INSERTS 1000

typedef struct st_mi_isam_share {	/* Shared between opens */
  MI_STATE_INFO state;
  MI_BASE_INFO base;
  MI_KEYDEF  ft2_keyinfo;		/* Second-level ft-key definition */
  MI_KEYDEF  *keyinfo;			/* Key definitions */
  MI_UNIQUEDEF *uniqueinfo;		/* unique definitions */
  HA_KEYSEG *keyparts;			/* key part info */
  drizzled::MI_COLUMNDEF *rec;			/* Pointer to field information */
  MI_PACK    pack;			/* Data about packed records */
  MI_BLOB    *blobs;			/* Pointer to blobs */
  std::list<drizzled::Session *> *in_use;         /* List of threads using this table */
  char  *unique_file_name;		/* realpath() of index file */
  char  *data_file_name,		/* Resolved path names from symlinks */
        *index_file_name;
  unsigned char *file_map;			/* mem-map of file if possible */
private:
  drizzled::KEY_CACHE key_cache;			/* ref to the current key cache */
public:
  drizzled::KEY_CACHE *getKeyCache()
  {
    return &key_cache;
  }

  void setKeyCache();

  MI_DECODE_TREE *decode_trees;
  uint16_t *decode_tables;
  int (*read_record)(struct st_myisam_info*, drizzled::internal::my_off_t, unsigned char*);
  int (*write_record)(struct st_myisam_info*, const unsigned char*);
  int (*update_record)(struct st_myisam_info*, drizzled::internal::my_off_t, const unsigned char*);
  int (*delete_record)(struct st_myisam_info*);
  int (*read_rnd)(struct st_myisam_info*, unsigned char*, drizzled::internal::my_off_t, bool);
  int (*compare_record)(struct st_myisam_info*, const unsigned char *);
  /* Function to use for a row checksum. */
  ha_checksum (*calc_checksum)(struct st_myisam_info*, const unsigned char *);
  int (*compare_unique)(struct st_myisam_info*, MI_UNIQUEDEF *,
			const unsigned char *record, drizzled::internal::my_off_t pos);
  size_t (*file_read)(MI_INFO *, unsigned char *, size_t, drizzled::internal::my_off_t, drizzled::myf);
  size_t (*file_write)(MI_INFO *, const unsigned char *, size_t, drizzled::internal::my_off_t, drizzled::myf);
  ulong this_process;			/* processid */
  ulong last_process;			/* For table-change-check */
  ulong last_version;			/* Version on start */
  uint64_t options;			/* Options used */
  ulong min_pack_length;		/* Theese are used by packed data */
  ulong max_pack_length;
  ulong state_diff_length;
  uint	rec_reflength;			/* rec_reflength in use now */
  uint32_t  unique_name_length;
  int	kfile;				/* Shared keyfile */
  int	data_file;			/* Shared data file */
  int	mode;				/* mode of file on open */
  uint	reopen;				/* How many times reopened */
  uint	w_locks,r_locks,tot_locks;	/* Number of read/write locks */
  uint	blocksize;			/* blocksize of keyfile */
  drizzled::myf write_flag;
  enum drizzled::data_file_type data_file_type;
  /* Below flag is needed to make log tables work with concurrent insert */
  bool is_log_table;

  bool  changed,			/* If changed since lock */
    global_changed,			/* If changed since open */
    not_flushed,
    temporary,delay_key_write,
    concurrent_insert;
  drizzled::internal::my_off_t mmaped_length;
  uint32_t     nonmmaped_inserts;           /* counter of writing in non-mmaped
                                           area */
} MYISAM_SHARE;


typedef uint32_t mi_bit_type;

typedef struct st_mi_bit_buff {		/* Used for packing of record */
  mi_bit_type current_byte;
  uint32_t bits;
  unsigned char *pos,*end,*blob_pos,*blob_end;
  uint32_t error;
} MI_BIT_BUFF;


typedef bool (*index_cond_func_t)(void *param);

struct st_myisam_info {
  MYISAM_SHARE *s;			/* Shared between open:s */
  MI_STATUS_INFO *state,save_state;
  MI_BLOB     *blobs;			/* Pointer to blobs */
  MI_BIT_BUFF  bit_buff;
  /* accumulate indexfile changes between write's */
  drizzled::TREE	        *bulk_insert;
  drizzled::Session *in_use;                      /* Thread using this table          */
  char *filename;			/* parameter to open filename       */
  unsigned char *buff,				/* Temp area for key                */
	*lastkey,*lastkey2;		/* Last used search key             */
  unsigned char *first_mbr_key;			/* Searhed spatial key              */
  unsigned char	*rec_buff;			/* Tempbuff for recordpack          */
  unsigned char *int_keypos,			/* Save position for next/previous  */
        *int_maxpos;			/*  -""-  */
  uint32_t  int_nod_flag;			/*  -""-  */
  uint32_t int_keytree_version;		/*  -""-  */
  int (*read_record)(struct st_myisam_info*, drizzled::internal::my_off_t, unsigned char*);
  ulong this_unique;			/* uniq filenumber or thread */
  ulong last_unique;			/* last unique number */
  ulong this_loop;			/* counter for this open */
  ulong last_loop;			/* last used counter */
  drizzled::internal::my_off_t lastpos,			/* Last record position */
	nextpos;			/* Position to next record */
  drizzled::internal::my_off_t save_lastpos;
  drizzled::internal::my_off_t pos;				/* Intern variable */
  drizzled::internal::my_off_t last_keypage;		/* Last key page read */
  drizzled::internal::my_off_t last_search_keypage;		/* Last keypage when searching */
  drizzled::internal::my_off_t dupp_key_pos;
  ha_checksum checksum;                 /* Temp storage for row checksum */
  /* QQ: the folloing two xxx_length fields should be removed,
     as they are not compatible with parallel repair */
  ulong packed_length,blob_length;	/* Length of found, packed record */
  int  dfile;				/* The datafile */
  uint32_t opt_flag;			/* Optim. for space/speed */
  uint32_t update;				/* If file changed since open */
  int	lastinx;			/* Last used index */
  uint	lastkey_length;			/* Length of key in lastkey */
  uint	last_rkey_length;		/* Last length in mi_rkey() */
  enum drizzled::ha_rkey_function last_key_func;  /* CONTAIN, OVERLAP, etc */
  uint32_t  save_lastkey_length;
  uint32_t  pack_key_length;                /* For MYISAMMRG */
  uint16_t last_used_keyseg;              /* For MyISAMMRG */
  int	errkey;				/* Got last error on this key */
  int   lock_type;			/* How database was locked */
  int   tmp_lock_type;			/* When locked by readinfo */
  uint	data_changed;			/* Somebody has changed data */
  uint	save_update;			/* When using KEY_READ */
  int	save_lastinx;
  drizzled::internal::io_cache_st rec_cache;			/* When cacheing records */
  uint32_t  preload_buff_size;              /* When preloading indexes */
  drizzled::myf lock_wait;			/* is 0 or MY_DONT_WAIT */
  bool was_locked;			/* Was locked in panic */
  bool append_insert_at_end;		/* Set if concurrent insert */
  bool quick_mode;
  bool page_changed;		/* If info->buff can't be used for rnext */
  bool buff_used;		/* If info->buff has to be reread for rnext */
  bool once_flags;           /* For MYISAMMRG */

  index_cond_func_t index_cond_func;   /* Index condition function */
  void *index_cond_func_arg;           /* parameter for the func */
  drizzled::THR_LOCK_DATA lock;
  unsigned char  *rtree_recursion_state;	/* For RTREE */
  int     rtree_recursion_depth;
};

typedef struct st_buffpek {
  off_t file_pos;                    /* Where we are in the sort file */
  unsigned char *base,*key;                     /* Key pointers */
  drizzled::ha_rows count;                        /* Number of rows in table */
  ulong mem_count;                      /* numbers of keys in memory */
  ulong max_keys;                       /* Max keys in buffert */
} BUFFPEK;

typedef struct st_mi_sort_param
{
  pthread_t  thr;
  drizzled::internal::io_cache_st read_cache, tempfile, tempfile_for_exceptions;
  drizzled::DYNAMIC_ARRAY buffpek;
  MI_BIT_BUFF   bit_buff;               /* For parallel repair of packrec. */

  /*
    The next two are used to collect statistics, see update_key_parts for
    description.
  */
  uint64_t unique[MI_MAX_KEY_SEG+1];
  uint64_t notnull[MI_MAX_KEY_SEG+1];

  drizzled::internal::my_off_t pos,max_pos,filepos,start_recpos;
  uint32_t key, key_length,real_key_length,sortbuff_size;
  uint32_t maxbuffers, keys, find_length, sort_keys_length;
  bool fix_datafile, master;
  bool calc_checksum;                /* calculate table checksum */
  MI_KEYDEF *keyinfo;
  HA_KEYSEG *seg;
  SORT_INFO *sort_info;
  unsigned char **sort_keys;
  unsigned char *rec_buff;
  void *wordlist, *wordptr;
  drizzled::memory::Root wordroot;
  unsigned char *record;
  int (*key_cmp)(struct st_mi_sort_param *, const void *, const void *);
  int (*key_read)(struct st_mi_sort_param *,void *);
  int (*key_write)(struct st_mi_sort_param *, const void *);
  void (*lock_in_memory)(MI_CHECK *);
  int (*write_keys)(struct st_mi_sort_param *, register unsigned char **,
                     uint32_t , struct st_buffpek *, drizzled::internal::io_cache_st *);
  unsigned int (*read_to_buffer)(drizzled::internal::io_cache_st *,struct st_buffpek *, uint);
  int (*write_key)(struct st_mi_sort_param *, drizzled::internal::io_cache_st *,unsigned char *,
                       uint, uint);
} MI_SORT_PARAM;

	/* Some defines used by isam-funktions */

#define USE_WHOLE_KEY	MI_MAX_KEY_BUFF*2 /* Use whole key in _mi_search() */
#define F_EXTRA_LCK	-1

	/* bits in opt_flag */
#define MEMMAP_USED	32
#define REMEMBER_OLD_POS 64

#define WRITEINFO_UPDATE_KEYFILE	1
#define WRITEINFO_NO_UNLOCK		2

        /* once_flags */
#define USE_PACKED_KEYS         1
#define RRND_PRESERVE_LASTINX   2

	/* bits in state.changed */

#define STATE_CHANGED		1
#define STATE_CRASHED		2
#define STATE_CRASHED_ON_REPAIR 4
#define STATE_NOT_ANALYZED	8
#define STATE_NOT_OPTIMIZED_KEYS 16
#define STATE_NOT_SORTED_PAGES	32

	/* options to mi_read_cache */

#define READING_NEXT	1
#define READING_HEADER	2


#define mi_getint(x)	((uint) mi_uint2korr(x) & 32767)
#define mi_putint(x,y,nod) { uint16_t boh=(nod ? (uint16_t) 32768 : 0) + (uint16_t) (y);\
			  mi_int2store(x,boh); }
#define mi_test_if_nod(x) (x[0] & 128 ? info->s->base.key_reflength : 0)
#define mi_report_crashed(A, B) _mi_report_crashed((A), (B), __FILE__, __LINE__)
#define mi_mark_crashed(x) do{(x)->s->state.changed|= STATE_CRASHED; \
                              mi_report_crashed((x), 0); \
                           }while(0)
#define mi_mark_crashed_on_repair(x) do{(x)->s->state.changed|= \
                                        STATE_CRASHED|STATE_CRASHED_ON_REPAIR; \
                                        (x)->update|= HA_STATE_CHANGED; \
                                     }while(0)
#define mi_is_crashed(x) ((x)->s->state.changed & STATE_CRASHED)
#define mi_is_crashed_on_repair(x) ((x)->s->state.changed & STATE_CRASHED_ON_REPAIR)
#define mi_print_error(SHARE, ERRNO)                     \
        mi_report_error((ERRNO), (SHARE)->index_file_name)

/* Functions to store length of space packed keys, VARCHAR or BLOB keys */

#define store_key_length(key,length) \
{ if ((length) < 255) \
  { *(key)=(length); } \
  else \
  { *(key)=255; mi_int2store((key)+1,(length)); } \
}

#define get_key_full_length(length,key) \
{ if ((unsigned char) *(key) != 255) \
    length= ((uint) (unsigned char) *((key)++))+1; \
  else \
  { length=mi_uint2korr((key)+1)+3; (key)+=3; } \
}

#define get_key_full_length_rdonly(length,key) \
{ if ((unsigned char) *(key) != 255) \
    length= ((uint) (unsigned char) *((key)))+1; \
  else \
  { length=mi_uint2korr((key)+1)+3; } \
}

#define get_pack_length(length) ((length) >= 255 ? 3 : 1)

#define MI_MIN_BLOCK_LENGTH	20	/* Because of delete-link */
#define MI_EXTEND_BLOCK_LENGTH	20	/* Don't use to small record-blocks */
#define MI_SPLIT_LENGTH	((MI_EXTEND_BLOCK_LENGTH+4)*2)
#define MI_MAX_DYN_BLOCK_HEADER	20	/* Max prefix of record-block */
#define MI_BLOCK_INFO_HEADER_LENGTH 20
#define MI_DYN_DELETE_BLOCK_HEADER 20	/* length of delete-block-header */
#define MI_DYN_MAX_BLOCK_LENGTH	((1L << 24)-4L)
#define MI_DYN_MAX_ROW_LENGTH	(MI_DYN_MAX_BLOCK_LENGTH - MI_SPLIT_LENGTH)
#define MI_DYN_ALIGN_SIZE	4	/* Align blocks on this */
#define MI_MAX_DYN_HEADER_BYTE	13	/* max header byte for dynamic rows */
#define MI_MAX_BLOCK_LENGTH	((((ulong) 1 << 24)-1) & (~ (ulong) (MI_DYN_ALIGN_SIZE-1)))
#define MI_REC_BUFF_OFFSET      ALIGN_SIZE(MI_DYN_DELETE_BLOCK_HEADER+sizeof(uint32_t))

#define MEMMAP_EXTRA_MARGIN	7	/* Write this as a suffix for file */

#define PACK_TYPE_SELECTED	1	/* Bits in field->pack_type */
#define PACK_TYPE_SPACE_FIELDS	2
#define PACK_TYPE_ZERO_FILL	4
#define MI_FOUND_WRONG_KEY 32738	/* Impossible value from ha_key_cmp */

#define MI_MAX_KEY_BLOCK_SIZE	(MI_MAX_KEY_BLOCK_LENGTH/MI_MIN_KEY_BLOCK_LENGTH)
#define MI_BLOCK_SIZE(key_length,data_pointer,key_pointer,block_size) (((((key_length)+(data_pointer)+(key_pointer))*4+(key_pointer)+2)/(block_size)+1)*(block_size))
#define MI_MAX_KEYPTR_SIZE	5        /* For calculating block lengths */
#define MI_MIN_KEYBLOCK_LENGTH	50         /* When to split delete blocks */

#define MI_MIN_SIZE_BULK_INSERT_TREE 16384             /* this is per key */
#define MI_MIN_ROWS_TO_USE_BULK_INSERT 100
#define MI_MIN_ROWS_TO_DISABLE_INDEXES 100
#define MI_MIN_ROWS_TO_USE_WRITE_CACHE 10

/* The UNIQUE check is done with a hashed long key */

#define MI_UNIQUE_HASH_TYPE	HA_KEYTYPE_ULONG_INT
#define mi_unique_store(A,B)    mi_int4store((A),(B))

extern boost::mutex THR_LOCK_myisam;

	/* Some extern variables */

extern std::list<MI_INFO *> myisam_open_list;
extern unsigned char  myisam_file_magic[], myisam_pack_file_magic[];
extern uint32_t  myisam_read_vec[], myisam_readnext_vec[];
extern uint32_t myisam_quick_table_bits;
extern ulong myisam_pid;

	/* This is used by _mi_calc_xxx_key_length och _mi_store_key */

typedef struct st_mi_s_param
{
  uint	ref_length,key_length,
	n_ref_length,
	n_length,
	totlength,
	part_of_prev_key,prev_length,pack_marker;
  unsigned char *key, *prev_key,*next_key_pos;
  bool store_not_null;
} MI_KEY_PARAM;

	/* Prototypes for intern functions */

extern int _mi_read_dynamic_record(MI_INFO *info,drizzled::internal::my_off_t filepos,unsigned char *buf);
extern int _mi_write_dynamic_record(MI_INFO*, const unsigned char*);
extern int _mi_update_dynamic_record(MI_INFO*, drizzled::internal::my_off_t, const unsigned char*);
extern int _mi_delete_dynamic_record(MI_INFO *info);
extern int _mi_cmp_dynamic_record(MI_INFO *info,const unsigned char *record);
extern int _mi_read_rnd_dynamic_record(MI_INFO *, unsigned char *,drizzled::internal::my_off_t, bool);
extern int _mi_write_blob_record(MI_INFO*, const unsigned char*);
extern int _mi_update_blob_record(MI_INFO*, drizzled::internal::my_off_t, const unsigned char*);
extern int _mi_read_static_record(MI_INFO *info, drizzled::internal::my_off_t filepos,unsigned char *buf);
extern int _mi_write_static_record(MI_INFO*, const unsigned char*);
extern int _mi_update_static_record(MI_INFO*, drizzled::internal::my_off_t, const unsigned char*);
extern int _mi_delete_static_record(MI_INFO *info);
extern int _mi_cmp_static_record(MI_INFO *info,const unsigned char *record);
extern int _mi_read_rnd_static_record(MI_INFO*, unsigned char *,drizzled::internal::my_off_t, bool);
extern int _mi_ck_write(MI_INFO *info,uint32_t keynr,unsigned char *key,uint32_t length);
extern int _mi_ck_real_write_btree(MI_INFO *info, MI_KEYDEF *keyinfo,
                                   unsigned char *key, uint32_t key_length,
                                   drizzled::internal::my_off_t *root, uint32_t comp_flag);
extern int _mi_enlarge_root(MI_INFO *info,MI_KEYDEF *keyinfo,unsigned char *key, drizzled::internal::my_off_t *root);
extern int _mi_insert(MI_INFO *info,MI_KEYDEF *keyinfo,unsigned char *key,
		      unsigned char *anc_buff,unsigned char *key_pos,unsigned char *key_buff,
		      unsigned char *father_buff, unsigned char *father_keypos,
		      drizzled::internal::my_off_t father_page, bool insert_last);
extern int _mi_split_page(MI_INFO *info,MI_KEYDEF *keyinfo,unsigned char *key,
			  unsigned char *buff,unsigned char *key_buff, bool insert_last);
extern unsigned char *_mi_find_half_pos(uint32_t nod_flag,MI_KEYDEF *keyinfo,unsigned char *page,
				unsigned char *key,uint32_t *return_key_length,
				unsigned char **after_key);
extern int _mi_calc_static_key_length(MI_KEYDEF *keyinfo,uint32_t nod_flag,
				      unsigned char *key_pos, unsigned char *org_key,
				      unsigned char *key_buff,
				      unsigned char *key, MI_KEY_PARAM *s_temp);
extern int _mi_calc_var_key_length(MI_KEYDEF *keyinfo,uint32_t nod_flag,
				   unsigned char *key_pos, unsigned char *org_key,
				   unsigned char *key_buff,
				   unsigned char *key, MI_KEY_PARAM *s_temp);
extern int _mi_calc_var_pack_key_length(MI_KEYDEF *keyinfo,uint32_t nod_flag,
					unsigned char *key_pos, unsigned char *org_key,
					unsigned char *prev_key,
					unsigned char *key, MI_KEY_PARAM *s_temp);
extern int _mi_calc_bin_pack_key_length(MI_KEYDEF *keyinfo,uint32_t nod_flag,
					unsigned char *key_pos,unsigned char *org_key,
					unsigned char *prev_key,
					unsigned char *key, MI_KEY_PARAM *s_temp);
void _mi_store_static_key(MI_KEYDEF *keyinfo,  unsigned char *key_pos,
			   MI_KEY_PARAM *s_temp);
void _mi_store_var_pack_key(MI_KEYDEF *keyinfo,  unsigned char *key_pos,
			     MI_KEY_PARAM *s_temp);
#ifdef NOT_USED
void _mi_store_pack_key(MI_KEYDEF *keyinfo,  unsigned char *key_pos,
			 MI_KEY_PARAM *s_temp);
#endif
void _mi_store_bin_pack_key(MI_KEYDEF *keyinfo,  unsigned char *key_pos,
			    MI_KEY_PARAM *s_temp);

extern int _mi_ck_delete(MI_INFO *info,uint32_t keynr,unsigned char *key,uint32_t key_length);
int _mi_readinfo(MI_INFO *info,int lock_flag,int check_keybuffer);
extern int _mi_writeinfo(MI_INFO *info,uint32_t options);
extern int _mi_test_if_changed(MI_INFO *info);
extern int _mi_mark_file_changed(MI_INFO *info);
extern int _mi_decrement_open_count(MI_INFO *info);
extern int _mi_check_index(MI_INFO *info,int inx);
extern int _mi_search(MI_INFO *info,MI_KEYDEF *keyinfo,unsigned char *key,uint32_t key_len,
		      uint32_t nextflag,drizzled::internal::my_off_t pos);
extern int _mi_bin_search(struct st_myisam_info *info,MI_KEYDEF *keyinfo,
			  unsigned char *page,unsigned char *key,uint32_t key_len,uint32_t comp_flag,
			  unsigned char * *ret_pos,unsigned char *buff, bool *was_last_key);
extern int _mi_seq_search(MI_INFO *info,MI_KEYDEF *keyinfo,unsigned char *page,
			  unsigned char *key,uint32_t key_len,uint32_t comp_flag,
			  unsigned char **ret_pos,unsigned char *buff, bool *was_last_key);
extern int _mi_prefix_search(MI_INFO *info,MI_KEYDEF *keyinfo,unsigned char *page,
			  unsigned char *key,uint32_t key_len,uint32_t comp_flag,
			  unsigned char **ret_pos,unsigned char *buff, bool *was_last_key);
extern drizzled::internal::my_off_t _mi_kpos(uint32_t nod_flag,unsigned char *after_key);
extern void _mi_kpointer(MI_INFO *info,unsigned char *buff,drizzled::internal::my_off_t pos);
extern drizzled::internal::my_off_t _mi_dpos(MI_INFO *info, uint32_t nod_flag,unsigned char *after_key);
extern drizzled::internal::my_off_t _mi_rec_pos(MYISAM_SHARE *info, unsigned char *ptr);
void _mi_dpointer(MI_INFO *info, unsigned char *buff,drizzled::internal::my_off_t pos);
extern uint32_t _mi_get_static_key(MI_KEYDEF *keyinfo,uint32_t nod_flag,unsigned char * *page,
			       unsigned char *key);
extern uint32_t _mi_get_pack_key(MI_KEYDEF *keyinfo,uint32_t nod_flag,unsigned char * *page,
			     unsigned char *key);
extern uint32_t _mi_get_binary_pack_key(MI_KEYDEF *keyinfo, uint32_t nod_flag,
				    unsigned char **page_pos, unsigned char *key);
extern unsigned char *_mi_get_last_key(MI_INFO *info,MI_KEYDEF *keyinfo,unsigned char *keypos,
			       unsigned char *lastkey,unsigned char *endpos,
			       uint32_t *return_key_length);
extern unsigned char *_mi_get_key(MI_INFO *info, MI_KEYDEF *keyinfo, unsigned char *page,
			  unsigned char *key, unsigned char *keypos, uint32_t *return_key_length);
extern uint32_t _mi_keylength(MI_KEYDEF *keyinfo,unsigned char *key);
extern uint32_t _mi_keylength_part(MI_KEYDEF *keyinfo, register unsigned char *key,
			       HA_KEYSEG *end);
extern unsigned char *_mi_move_key(MI_KEYDEF *keyinfo,unsigned char *to,unsigned char *from);
extern int _mi_search_next(MI_INFO *info,MI_KEYDEF *keyinfo,unsigned char *key,
			   uint32_t key_length,uint32_t nextflag,drizzled::internal::my_off_t pos);
extern int _mi_search_first(MI_INFO *info,MI_KEYDEF *keyinfo,drizzled::internal::my_off_t pos);
extern int _mi_search_last(MI_INFO *info,MI_KEYDEF *keyinfo,drizzled::internal::my_off_t pos);
extern unsigned char *_mi_fetch_keypage(MI_INFO *info,MI_KEYDEF *keyinfo,drizzled::internal::my_off_t page,
				int level,unsigned char *buff,int return_buffer);
extern int _mi_write_keypage(MI_INFO *info,MI_KEYDEF *keyinfo,drizzled::internal::my_off_t page,
			     int level, unsigned char *buff);
extern int _mi_dispose(MI_INFO *info,MI_KEYDEF *keyinfo,drizzled::internal::my_off_t pos,
                      int level);
extern drizzled::internal::my_off_t _mi_new(MI_INFO *info,MI_KEYDEF *keyinfo,int level);
extern uint32_t _mi_make_key(MI_INFO *info,uint32_t keynr,unsigned char *key,
			 const unsigned char *record,drizzled::internal::my_off_t filepos);
extern uint32_t _mi_pack_key(register MI_INFO *info, uint32_t keynr, unsigned char *key,
                         unsigned char *old, drizzled::key_part_map keypart_map,
                         HA_KEYSEG **last_used_keyseg);
extern int _mi_read_key_record(MI_INFO *info,drizzled::internal::my_off_t filepos,unsigned char *buf);
extern int _mi_read_cache(drizzled::internal::io_cache_st *info,unsigned char *buff,drizzled::internal::my_off_t pos,
			  uint32_t length,int re_read_if_possibly);
extern uint64_t retrieve_auto_increment(MI_INFO *info,const unsigned char *record);

unsigned char *mi_alloc_rec_buff(MI_INFO *info, size_t length, unsigned char **buf);
#define mi_get_rec_buff_ptr(info,buf)                              \
        ((((info)->s->options & HA_OPTION_PACK_RECORD) && (buf)) ? \
        (buf) - MI_REC_BUFF_OFFSET : (buf))
#define mi_get_rec_buff_len(info,buf)                              \
        (*((uint32_t *)(mi_get_rec_buff_ptr(info,buf))))

extern ulong _mi_rec_unpack(MI_INFO *info,unsigned char *to,unsigned char *from,
			    ulong reclength);
extern bool _mi_rec_check(MI_INFO *info,const unsigned char *record, unsigned char *packpos,
                             ulong packed_length, bool with_checkum);
extern int _mi_write_part_record(MI_INFO *info,drizzled::internal::my_off_t filepos,ulong length,
				 drizzled::internal::my_off_t next_filepos,unsigned char **record,
				 ulong *reclength,int *flag);
extern void _mi_print_key(FILE *stream,HA_KEYSEG *keyseg,const unsigned char *key,
			  uint32_t length);
extern bool _mi_read_pack_info(MI_INFO *info,bool fix_keys);
extern int _mi_read_pack_record(MI_INFO *info,drizzled::internal::my_off_t filepos,unsigned char *buf);
extern int _mi_read_rnd_pack_record(MI_INFO*, unsigned char *,drizzled::internal::my_off_t, bool);
extern int _mi_pack_rec_unpack(MI_INFO *info, MI_BIT_BUFF *bit_buff,
                               unsigned char *to, unsigned char *from, ulong reclength);

struct st_sort_info;


typedef struct st_mi_block_info {	/* Parameter to _mi_get_block_info */
  unsigned char header[MI_BLOCK_INFO_HEADER_LENGTH];
  ulong rec_len;
  ulong data_len;
  ulong block_len;
  ulong blob_len;
  drizzled::internal::my_off_t filepos;
  drizzled::internal::my_off_t next_filepos;
  drizzled::internal::my_off_t prev_filepos;
  uint32_t second_read;
  uint32_t offset;
} MI_BLOCK_INFO;

	/* bits in return from _mi_get_block_info */

#define BLOCK_FIRST	1
#define BLOCK_LAST	2
#define BLOCK_DELETED	4
#define BLOCK_ERROR	8	/* Wrong data */
#define BLOCK_SYNC_ERROR 16	/* Right data at wrong place */
#define BLOCK_FATAL_ERROR 32	/* hardware-error */

#define NEED_MEM	((uint) 10*4*(IO_SIZE+32)+32) /* Nead for recursion */
#define MAXERR			20
#define BUFFERS_WHEN_SORTING	16		/* Alloc for sort-key-tree */
#define WRITE_COUNT		1000
#define INDEX_TMP_EXT		".TMM"
#define DATA_TMP_EXT		".TMD"

#define UPDATE_TIME		1
#define UPDATE_STAT		2
#define UPDATE_SORT		4
#define UPDATE_AUTO_INC		8
#define UPDATE_OPEN_COUNT	16

#define USE_BUFFER_INIT		(((1024L*512L-MALLOC_OVERHEAD)/IO_SIZE)*IO_SIZE)
#define READ_BUFFER_INIT	(1024L*256L-MALLOC_OVERHEAD)
#define SORT_BUFFER_INIT	(2048L*1024L-MALLOC_OVERHEAD)
#define MIN_SORT_BUFFER		(4096-MALLOC_OVERHEAD)

#define fast_mi_writeinfo(INFO) if (!(INFO)->s->tot_locks) (void) _mi_writeinfo((INFO),0)
#define fast_mi_readinfo(INFO) ((INFO)->lock_type == F_UNLCK) && _mi_readinfo((INFO),F_RDLCK,1)

extern uint32_t _mi_get_block_info(MI_BLOCK_INFO *,int, drizzled::internal::my_off_t);
extern uint32_t _mi_rec_pack(MI_INFO *info,unsigned char *to,const unsigned char *from);
extern uint32_t _mi_pack_get_block_info(MI_INFO *myisam, MI_BIT_BUFF *bit_buff,
                                    MI_BLOCK_INFO *info, unsigned char **rec_buff_p,
                                    int file, drizzled::internal::my_off_t filepos);
extern void _my_store_blob_length(unsigned char *pos,uint32_t pack_length,uint32_t length);
extern void mi_report_error(int errcode, const char *file_name);
extern void mi_report_error(drizzled::error_t errcode, const char *file_name);
extern size_t mi_mmap_pread(MI_INFO *info, unsigned char *Buffer,
                            size_t Count, drizzled::internal::my_off_t offset, drizzled::myf MyFlags);
extern size_t mi_mmap_pwrite(MI_INFO *info, const unsigned char *Buffer,
                             size_t Count, drizzled::internal::my_off_t offset, drizzled::myf MyFlags);
extern size_t mi_nommap_pread(MI_INFO *info, unsigned char *Buffer,
                              size_t Count, drizzled::internal::my_off_t offset, drizzled::myf MyFlags);
extern size_t mi_nommap_pwrite(MI_INFO *info, const unsigned char *Buffer,
                               size_t Count, drizzled::internal::my_off_t offset, drizzled::myf MyFlags);

uint32_t mi_state_info_write(int file, MI_STATE_INFO *state, uint32_t pWrite);
uint32_t mi_state_info_read_dsk(int file, MI_STATE_INFO *state, bool pRead);
uint32_t mi_base_info_write(int file, MI_BASE_INFO *base);
int mi_keyseg_write(int file, const HA_KEYSEG *keyseg);
uint32_t mi_keydef_write(int file, MI_KEYDEF *keydef);
uint32_t mi_uniquedef_write(int file, MI_UNIQUEDEF *keydef);
uint32_t mi_recinfo_write(int file, drizzled::MI_COLUMNDEF *recinfo);
extern int mi_disable_indexes(MI_INFO *info);
extern int mi_enable_indexes(MI_INFO *info);
extern int mi_indexes_are_disabled(MI_INFO *info);
ulong _my_calc_total_blob_length(MI_INFO *info, const unsigned char *record);
ha_checksum mi_checksum(MI_INFO *info, const unsigned char *buf);
ha_checksum mi_static_checksum(MI_INFO *info, const unsigned char *buf);
bool mi_check_unique(MI_INFO *info, MI_UNIQUEDEF *def, unsigned char *record,
		     ha_checksum unique_hash, drizzled::internal::my_off_t pos);
ha_checksum mi_unique_hash(MI_UNIQUEDEF *def, const unsigned char *buf);
int _mi_cmp_static_unique(MI_INFO *info, MI_UNIQUEDEF *def,
			   const unsigned char *record, drizzled::internal::my_off_t pos);
int _mi_cmp_dynamic_unique(MI_INFO *info, MI_UNIQUEDEF *def,
			   const unsigned char *record, drizzled::internal::my_off_t pos);
int mi_unique_comp(MI_UNIQUEDEF *def, const unsigned char *a, const unsigned char *b,
		   bool null_are_equal);

extern MI_INFO *test_if_reopen(char *filename);
bool check_table_is_closed(const char *name, const char *where);
int mi_open_datafile(MI_INFO *info, MYISAM_SHARE *share, int file_to_dup);
int mi_open_keyfile(MYISAM_SHARE *share);
void mi_setup_functions(register MYISAM_SHARE *share);
bool mi_dynmap_file(MI_INFO *info, drizzled::internal::my_off_t size);
void mi_remap_file(MI_INFO *info, drizzled::internal::my_off_t size);

int mi_check_index_cond(register MI_INFO *info, uint32_t keynr, unsigned char *record);

    /* Functions needed by mi_check */
volatile int *killed_ptr(MI_CHECK *param);
void mi_check_print_error(MI_CHECK *param, const char *fmt,...);
void mi_check_print_warning(MI_CHECK *param, const char *fmt,...);
void mi_check_print_info(MI_CHECK *param, const char *fmt,...);
int flush_pending_blocks(MI_SORT_PARAM *param);
int thr_write_keys(MI_SORT_PARAM *sort_param);
int flush_blocks(MI_CHECK *param, drizzled::KEY_CACHE *key_cache, int file);

int sort_write_record(MI_SORT_PARAM *sort_param);
int _create_index_by_sort(MI_SORT_PARAM *info,bool no_messages, size_t);

extern void mi_set_index_cond_func(MI_INFO *info, index_cond_func_t func,
                                   void *func_arg);
/* Just for myisam legacy */
extern size_t my_pwrite(int Filedes,const unsigned char *Buffer,size_t Count,
                        drizzled::internal::my_off_t offset,drizzled::myf MyFlags);
extern size_t my_pread(int Filedes,unsigned char *Buffer,size_t Count,drizzled::internal::my_off_t offset,
                       drizzled::myf MyFlags);

/* Needed for handler */
void mi_disable_non_unique_index(MI_INFO *info, drizzled::ha_rows rows);
void _mi_report_crashed(MI_INFO *file, const char *message, const char *sfile,
                        uint32_t sline);


#define DFLT_INIT_HITS  3
