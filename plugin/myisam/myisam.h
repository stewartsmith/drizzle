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

/* This file should be included when using myisam_funktions */

#pragma once

#include <drizzled/common_fwd.h>

#include <drizzled/key_map.h>

#include <drizzled/base.h>
#ifndef _keycache_h
#include "keycache.h"
#endif
#include <plugin/myisam/my_handler.h>
#include <drizzled/internal/iocache.h>

/*
  Limit max keys according to HA_MAX_POSSIBLE_KEY
*/

#if MAX_INDEXES > HA_MAX_POSSIBLE_KEY
#define MI_MAX_KEY                  HA_MAX_POSSIBLE_KEY /* Max allowed keys */
#else
#define MI_MAX_KEY                  MAX_INDEXES         /* Max allowed keys */
#endif

/*
  The following defines can be increased if necessary.
  But beware the dependency of MI_MAX_POSSIBLE_KEY_BUFF and MI_MAX_KEY_LENGTH.
*/
#define MI_MAX_KEY_LENGTH           1332            /* Max length in bytes */
#define MI_MAX_KEY_SEG              16              /* Max segments for key */

#define MI_MAX_POSSIBLE_KEY_BUFF (MI_MAX_KEY_LENGTH + 6 + 6) /* For mi_check */

#define MI_MAX_KEY_BUFF  (MI_MAX_KEY_LENGTH+MI_MAX_KEY_SEG*6+8+8)
#define MI_MAX_MSG_BUF      1024 /* used in CHECK TABLE, REPAIR TABLE */
#define MI_NAME_IEXT	".MYI"
#define MI_NAME_DEXT	".MYD"
/* Max extra space to use when sorting keys */
#define MI_MAX_TEMP_LENGTH	2*1024L*1024L*1024L

/* Possible values for myisam_block_size (must be power of 2) */
#define MI_KEY_BLOCK_LENGTH	1024	/* default key block length */
#define MI_MIN_KEY_BLOCK_LENGTH	1024	/* Min key block length */
#define MI_MAX_KEY_BLOCK_LENGTH	16384

/*
  In the following macros '_keyno_' is 0 .. keys-1.
  If there can be more keys than bits in the key_map, the highest bit
  is for all upper keys. They cannot be switched individually.
  This means that clearing of high keys is ignored, setting one high key
  sets all high keys.
*/
#define MI_KEYMAP_BITS      (64)
#define MI_KEYMAP_HIGH_MASK (1UL << (MI_KEYMAP_BITS - 1))
#define mi_get_mask_all_keys_active(_keys_) \
                            (((_keys_) < MI_KEYMAP_BITS) ? \
                             ((1UL << (_keys_)) - 1UL) : \
                             (~ 0UL))

#if MI_MAX_KEY > MI_KEYMAP_BITS

#define mi_is_key_active(_keymap_,_keyno_) \
                            (((_keyno_) < MI_KEYMAP_BITS) ? \
                             test((_keymap_) & (1UL << (_keyno_))) : \
                             test((_keymap_) & MI_KEYMAP_HIGH_MASK))
#define mi_set_key_active(_keymap_,_keyno_) \
                            (_keymap_)|= (((_keyno_) < MI_KEYMAP_BITS) ? \
                                          (1UL << (_keyno_)) : \
                                          MI_KEYMAP_HIGH_MASK)
#define mi_clear_key_active(_keymap_,_keyno_) \
                            (_keymap_)&= (((_keyno_) < MI_KEYMAP_BITS) ? \
                                          (~ (1UL << (_keyno_))) : \
                                          (~ (0UL)) /*ignore*/ )

#else

#define mi_is_key_active(_keymap_,_keyno_) \
                            test((_keymap_) & (1UL << (_keyno_)))
#define mi_set_key_active(_keymap_,_keyno_) \
                            (_keymap_)|= (1UL << (_keyno_))
#define mi_clear_key_active(_keymap_,_keyno_) \
                            (_keymap_)&= (~ (1UL << (_keyno_)))

#endif

#define mi_is_any_key_active(_keymap_) \
                            test((_keymap_))
#define mi_is_all_keys_active(_keymap_,_keys_) \
                            ((_keymap_) == mi_get_mask_all_keys_active(_keys_))
#define mi_set_all_keys_active(_keymap_,_keys_) \
                            (_keymap_)= mi_get_mask_all_keys_active(_keys_)
#define mi_clear_all_keys_active(_keymap_) \
                            (_keymap_)= 0
#define mi_intersect_keys_active(_to_,_from_) \
                            (_to_)&= (_from_)
#define mi_is_any_intersect_keys_active(_keymap1_,_keys_,_keymap2_) \
                            ((_keymap1_) & (_keymap2_) & \
                             mi_get_mask_all_keys_active(_keys_))
#define mi_copy_keys_active(_to_,_maxkeys_,_from_) \
                            (_to_)= (mi_get_mask_all_keys_active(_maxkeys_) & \
                                     (_from_))

typedef uint32_t ha_checksum;

	/* Param to/from mi_status */
typedef struct st_mi_isaminfo		/* Struct from h_info */
{
  drizzled::ha_rows records;			/* Records in database */
  drizzled::ha_rows deleted;			/* Deleted records in database */
  drizzled::internal::my_off_t recpos;			/* Pos for last used record */
  drizzled::internal::my_off_t newrecpos;			/* Pos if we write new record */
  drizzled::internal::my_off_t dupp_key_pos;		/* Position to record with dupp key */
  drizzled::internal::my_off_t data_file_length,		/* Length of data file */
           max_data_file_length,
           index_file_length,
           max_index_file_length,
           delete_length;
  ulong reclength;			/* Recordlength */
  ulong mean_reclength;			/* Mean recordlength (if packed) */
  uint64_t auto_increment;
  uint64_t key_map;			/* Which keys are used */
  char  *data_file_name, *index_file_name;
  uint32_t  keys;				/* Number of keys in use */
  uint	options;			/* HA_OPTION_... used */
  int	errkey,				/* With key was dupplicated on err */
	sortkey;			/* clustered by this key */
  int	filenr;				/* (uniq) filenr for datafile */
  time_t create_time;			/* When table was created */
  time_t check_time;
  time_t update_time;
  uint32_t  reflength;
  ulong record_offset;
  ulong *rec_per_key;			/* for sql optimizing */
} MI_ISAMINFO;


typedef struct st_mi_create_info
{
  const char *index_file_name, *data_file_name;	/* If using symlinks */
  drizzled::ha_rows max_rows;
  drizzled::ha_rows reloc_rows;
  uint64_t auto_increment;
  uint64_t data_file_length;
  uint64_t key_file_length;
  uint32_t old_options;
  uint8_t language;
  bool with_auto_increment;

  st_mi_create_info():
    index_file_name(0),
    data_file_name(0),
    max_rows(0),
    reloc_rows(0),
    auto_increment(0),
    data_file_length(0),
    key_file_length(0),
    old_options(0),
    language(0),
    with_auto_increment(0)
  { }

} MI_CREATE_INFO;

struct st_myisam_info;			/* For referense */
struct st_mi_isam_share;
typedef struct st_myisam_info MI_INFO;
struct st_mi_s_param;

typedef struct st_mi_keydef		/* Key definition with open & info */
{
  struct st_mi_isam_share *share;       /* Pointer to base (set in mi_open) */
  uint16_t keysegs;			/* Number of key-segment */
  uint16_t flag;				/* NOSAME, PACK_USED */

  uint8_t  key_alg;			/* BTREE, RTREE */
  uint16_t block_length;			/* Length of keyblock (auto) */
  uint16_t underflow_block_length;	/* When to execute underflow */
  uint16_t keylength;			/* Tot length of keyparts (auto) */
  uint16_t minlength;			/* min length of (packed) key (auto) */
  uint16_t maxlength;			/* max length of (packed) key (auto) */
  uint16_t block_size_index;		/* block_size (auto) */
  uint32_t version;			/* For concurrent read/write */

  HA_KEYSEG *seg,*end;

  int (*bin_search)(struct st_myisam_info *info,struct st_mi_keydef *keyinfo,
		    unsigned char *page,unsigned char *key,
		    uint32_t key_len,uint32_t comp_flag,unsigned char * *ret_pos,
		    unsigned char *buff, bool *was_last_key);
  uint32_t (*get_key)(struct st_mi_keydef *keyinfo,uint32_t nod_flag,unsigned char * *page,
		  unsigned char *key);
  int (*pack_key)(struct st_mi_keydef *keyinfo,uint32_t nod_flag,unsigned char *next_key,
		  unsigned char *org_key, unsigned char *prev_key, unsigned char *key,
		  struct st_mi_s_param *s_temp);
  void (*store_key)(struct st_mi_keydef *keyinfo, unsigned char *key_pos,
		    struct st_mi_s_param *s_temp);
  int (*ck_insert)(struct st_myisam_info *inf, uint32_t k_nr, unsigned char *k, uint32_t klen);
  int (*ck_delete)(struct st_myisam_info *inf, uint32_t k_nr, unsigned char *k, uint32_t klen);
} MI_KEYDEF;


#define MI_UNIQUE_HASH_LENGTH	4

typedef struct st_unique_def		/* Segment definition of unique */
{
  uint16_t keysegs;			/* Number of key-segment */
  unsigned char key;				/* Mapped to which key */
  uint8_t null_are_equal;
  HA_KEYSEG *seg,*end;
} MI_UNIQUEDEF;

typedef struct st_mi_decode_tree	/* Decode huff-table */
{
  uint16_t *table;
  uint	 quick_table_bits;
  unsigned char	 *intervalls;
} MI_DECODE_TREE;


struct st_mi_bit_buff;

/*
  Note that null markers should always be first in a row !
  When creating a column, one should only specify:
  type, length, null_bit and null_pos
*/

namespace drizzled
{

typedef struct st_columndef		/* column information */
{
  int16_t  type;				/* en_fieldtype */
  uint16_t length;			/* length of field */
  uint32_t offset;			/* Offset to position in row */
  uint8_t  null_bit;			/* If column may be 0 */
  uint16_t null_pos;			/* position for null marker */

#ifndef NOT_PACKED_DATABASES
  void (*unpack)(struct st_columndef *rec,struct st_mi_bit_buff *buff,
		 unsigned char *start,unsigned char *end);
  enum drizzled::en_fieldtype base_type;
  uint32_t space_length_bits,pack_type;
  MI_DECODE_TREE *huff_tree;
#endif
} MI_COLUMNDEF;

}


extern char * myisam_log_filename;		/* Name of logfile */
extern uint32_t myisam_block_size;
extern uint32_t myisam_concurrent_insert;
extern uint32_t myisam_bulk_insert_tree_size; 
extern uint32_t data_pointer_size;

	/* Prototypes for myisam-functions */

extern int mi_close(struct st_myisam_info *file);
extern int mi_delete(struct st_myisam_info *file,const unsigned char *buff);
extern struct st_myisam_info *mi_open(const drizzled::identifier::Table &identifier,
                                      int mode,
				      uint32_t wait_if_locked);
extern int mi_panic(enum drizzled::ha_panic_function function);
extern int mi_rfirst(struct st_myisam_info *file,unsigned char *buf,int inx);
extern int mi_rkey(MI_INFO *info, unsigned char *buf, int inx, const unsigned char *key,
                   drizzled::key_part_map keypart_map, enum drizzled::ha_rkey_function search_flag);
extern int mi_rlast(struct st_myisam_info *file,unsigned char *buf,int inx);
extern int mi_rnext(struct st_myisam_info *file,unsigned char *buf,int inx);
extern int mi_rnext_same(struct st_myisam_info *info, unsigned char *buf);
extern int mi_rprev(struct st_myisam_info *file,unsigned char *buf,int inx);
extern int mi_rrnd(struct st_myisam_info *file,unsigned char *buf, drizzled::internal::my_off_t pos);
extern int mi_scan_init(struct st_myisam_info *file);
extern int mi_scan(struct st_myisam_info *file,unsigned char *buf);
extern int mi_rsame(struct st_myisam_info *file,unsigned char *record,int inx);
extern int mi_update(struct st_myisam_info *file,const unsigned char *old,
		     unsigned char *new_record);
extern int mi_write(struct st_myisam_info *file,unsigned char *buff);
extern drizzled::internal::my_off_t mi_position(struct st_myisam_info *file);
extern int mi_status(struct st_myisam_info *info, MI_ISAMINFO *x, uint32_t flag);
extern int mi_lock_database(struct st_myisam_info *file,int lock_type);
extern int mi_create(const char *name,uint32_t keys,MI_KEYDEF *keydef,
		     uint32_t columns, drizzled::MI_COLUMNDEF *columndef,
		     uint32_t uniques, MI_UNIQUEDEF *uniquedef,
		     MI_CREATE_INFO *create_info, uint32_t flags);
extern int mi_delete_table(const char *name);
extern int mi_rename(const char *from, const char *to);
extern int mi_extra(struct st_myisam_info *file,
		    enum drizzled::ha_extra_function function,
		    void *extra_arg);
extern int mi_reset(struct st_myisam_info *file);
extern drizzled::ha_rows mi_records_in_range(MI_INFO *info, int inx,
                                   drizzled::key_range *min_key, drizzled::key_range *max_key);
extern int mi_log(int activate_log);
extern int mi_delete_all_rows(struct st_myisam_info *info);
extern ulong _mi_calc_blob_length(uint32_t length , const unsigned char *pos);
extern uint32_t mi_get_pointer_length(uint64_t file_length, uint32_t def);

/* this is used to pass to mysql_myisamchk_table */

#define   MYISAMCHK_REPAIR 1  /* equivalent to myisamchk -r */
#define   MYISAMCHK_VERIFY 2  /* Verify, run repair if failure */

/*
  Definitions needed for myisamchk.c

  Entries marked as "QQ to be removed" are NOT used to
  pass check/repair options to mi_check.c. They are used
  internally by myisamchk.c or/and ha_myisam.cc and should NOT
  be stored together with other flags. They should be removed
  from the following list to make addition of new flags possible.
*/

#define T_AUTO_INC              1
#define T_AUTO_REPAIR           2              /* QQ to be removed */
#define T_CALC_CHECKSUM         8
#define T_CHECK                 16             /* QQ to be removed */
#define T_CHECK_ONLY_CHANGED    32             /* QQ to be removed */
#define T_CREATE_MISSING_KEYS   64
#define T_DESCRIPT              128
#define T_DONT_CHECK_CHECKSUM   256
#define T_EXTEND                512
#define T_FAST                  (1L << 10)     /* QQ to be removed */
#define T_FORCE_CREATE          (1L << 11)     /* QQ to be removed */
#define T_FORCE_UNIQUENESS      (1L << 12)
#define T_INFO                  (1L << 13)
#define T_MEDIUM                (1L << 14)
#define T_QUICK                 (1L << 15)     /* QQ to be removed */
#define T_READONLY              (1L << 16)     /* QQ to be removed */
#define T_REP                   (1L << 17)
#define T_REP_BY_SORT           (1L << 18)     /* QQ to be removed */
#define T_REP_PARALLEL          (1L << 19)     /* QQ to be removed */
#define T_RETRY_WITHOUT_QUICK   (1L << 20)
#define T_SAFE_REPAIR           (1L << 21)
#define T_SILENT                (1L << 22)
#define T_SORT_INDEX            (1L << 23)     /* QQ to be removed */
#define T_SORT_RECORDS          (1L << 24)     /* QQ to be removed */
#define T_STATISTICS            (1L << 25)
#define T_UNPACK                (1L << 26)
#define T_UPDATE_STATE          (1L << 27)
#define T_VERBOSE               (1L << 28)
#define T_VERY_SILENT           (1L << 29)
#define T_WAIT_FOREVER          (1L << 30)
#define T_WRITE_LOOP            ((ulong) 1L << 31)

#define T_REP_ANY               (T_REP | T_REP_BY_SORT | T_REP_PARALLEL)

#define O_NEW_INDEX	1		/* Bits set in out_flag */
#define O_NEW_DATA	2
#define O_DATA_LOST	4

/* these struct is used by my_check to tell it what to do */

typedef struct st_sort_key_blocks		/* Used when sorting */
{
  unsigned char *buff,*end_pos;
  unsigned char lastkey[MI_MAX_POSSIBLE_KEY_BUFF];
  uint32_t last_length;
  int inited;
} SORT_KEY_BLOCKS;


/*
  MyISAM supports several statistics collection methods. Currently statistics
  collection method is not stored in MyISAM file and has to be specified for
  each table analyze/repair operation in  MI_CHECK::stats_method.
*/

typedef enum
{
  /* Treat NULLs as inequal when collecting statistics (default for 4.1/5.0) */
  MI_STATS_METHOD_NULLS_NOT_EQUAL,
  /* Treat NULLs as equal when collecting statistics (like 4.0 did) */
  MI_STATS_METHOD_NULLS_EQUAL,
  /* Ignore NULLs - count only tuples without NULLs in the index components */
  MI_STATS_METHOD_IGNORE_NULLS
} enum_mi_stats_method;

typedef struct st_mi_check_param
{
  uint64_t auto_increment_value;
  uint64_t max_data_file_length;
  uint64_t keys_in_use;
  uint64_t max_record_length;
  drizzled::internal::my_off_t search_after_block;
  drizzled::internal::my_off_t new_file_pos,key_file_blocks;
  drizzled::internal::my_off_t keydata,totaldata,key_blocks,start_check_pos;
  drizzled::ha_rows total_records,total_deleted;
  ha_checksum record_checksum,glob_crc;
  uint64_t use_buffers;
  size_t read_buffer_length, write_buffer_length,
         sort_buffer_length, sort_key_blocks;
  uint32_t out_flag,warning_printed,error_printed,verbose;
  uint32_t opt_sort_key,total_files,max_level;
  uint32_t testflag, key_cache_block_size;
  uint8_t language;
  bool using_global_keycache, opt_lock_memory, opt_follow_links;
  bool retry_repair, force_sort;
  char temp_filename[FN_REFLEN],*isam_file_name;
  int tmpfile_createflag;
  drizzled::myf myf_rw;
  drizzled::internal::io_cache_st read_cache;

  /*
    The next two are used to collect statistics, see update_key_parts for
    description.
  */
  uint64_t unique_count[MI_MAX_KEY_SEG+1];
  uint64_t notnull_count[MI_MAX_KEY_SEG+1];

  ha_checksum key_crc[HA_MAX_POSSIBLE_KEY];
  ulong rec_per_key_part[MI_MAX_KEY_SEG*HA_MAX_POSSIBLE_KEY];
  void *session;
  const char *db_name, *table_name;
  const char *op_name;
  enum_mi_stats_method stats_method;
} MI_CHECK;

typedef struct st_sort_info
{
  drizzled::internal::my_off_t filelength,dupp,buff_length;
  drizzled::ha_rows max_records;
  uint32_t current_key, total_keys;
  drizzled::myf myf_rw;
  enum drizzled::data_file_type new_data_file_type;
  MI_INFO *info;
  MI_CHECK *param;
  unsigned char *buff;
  SORT_KEY_BLOCKS *key_block,*key_block_end;
  /* sync things */
  uint32_t got_error, threads_running;
  pthread_mutex_t mutex;
  pthread_cond_t  cond;
} SORT_INFO;

/* functions in mi_check */
void myisamchk_init(MI_CHECK *param);
int chk_status(MI_CHECK *param, MI_INFO *info);
int chk_del(MI_CHECK *param, register MI_INFO *info, uint32_t test_flag);
int chk_size(MI_CHECK *param, MI_INFO *info);
int chk_key(MI_CHECK *param, MI_INFO *info);
int chk_data_link(MI_CHECK *param, MI_INFO *info,int extend);
int mi_repair(MI_CHECK *param, register MI_INFO *info,
	      char * name, int rep_quick);
int mi_sort_index(MI_CHECK *param, register MI_INFO *info, char * name);
int mi_repair_by_sort(MI_CHECK *param, register MI_INFO *info,
		      const char * name, int rep_quick);
int change_to_newfile(const char * filename, const char * old_ext,
		      const char * new_ext, uint32_t raid_chunks,
		      drizzled::myf myflags);
void lock_memory(MI_CHECK *param);
void update_auto_increment_key(MI_CHECK *param, MI_INFO *info,
			       bool repair);
int update_state_info(MI_CHECK *param, MI_INFO *info,uint32_t update);
void update_key_parts(MI_KEYDEF *keyinfo, ulong *rec_per_key_part,
                      uint64_t *unique, uint64_t *notnull,
                      uint64_t records);
int filecopy(MI_CHECK *param, int to,int from,drizzled::internal::my_off_t start,
	     drizzled::internal::my_off_t length, const char *type);
int movepoint(MI_INFO *info,unsigned char *record,drizzled::internal::my_off_t oldpos,
	      drizzled::internal::my_off_t newpos, uint32_t prot_key);
int write_data_suffix(SORT_INFO *sort_info, bool fix_datafile);
int test_if_almost_full(MI_INFO *info);
bool mi_test_if_sort_rep(MI_INFO *info, drizzled::ha_rows rows, uint64_t key_map,
			    bool force);

int mi_init_bulk_insert(MI_INFO *info, uint32_t cache_size, drizzled::ha_rows rows);
void mi_flush_bulk_insert(MI_INFO *info, uint32_t inx);
void mi_end_bulk_insert(MI_INFO *info);
int mi_preload(MI_INFO *info, uint64_t key_map, bool ignore_leaves);

