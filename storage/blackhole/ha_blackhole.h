/* Copyright (C) 2005 MySQL AB

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


#ifndef STORAGE_BLACKHOLE_HA_BLACKHOLE_H
#define STORAGE_BLACKHOLE_HA_BLACKHOLE_H

#include <drizzled/handler.h>
#include <mysys/thr_lock.h>

#define BLACKHOLE_MAX_KEY	64		/* Max allowed keys */
#define BLACKHOLE_MAX_KEY_SEG	16		/* Max segments for key */
#define BLACKHOLE_MAX_KEY_LENGTH 1000

/*
  Shared structure for correct LOCK operation
*/
struct st_blackhole_share {
  THR_LOCK lock;
  uint32_t use_count;
  uint32_t table_name_length;
  char table_name[1];
};


/*
  Class definition for the blackhole storage engine
  "Dumbest named feature ever"
*/
class ha_blackhole: public handler
{
  THR_LOCK_DATA lock;      /* MySQL lock */
  st_blackhole_share *share;

public:
  ha_blackhole(handlerton *hton, TABLE_SHARE *table_arg);
  ~ha_blackhole()
  {
  }
  /* The name that will be used for display purposes */
  const char *table_type() const { return "BLACKHOLE"; }
  /*
    The name of the index type that will be used for display
    don't implement this method unless you really have indexes
  */
  const char *index_type(uint32_t key_number);
  const char **bas_ext() const;
  uint64_t table_flags() const
  {
    return(HA_NULL_IN_KEY |
           HA_BINLOG_STMT_CAPABLE |
           HA_CAN_INDEX_BLOBS | 
           HA_AUTO_PART_KEY |
           HA_FILE_BASED);
  }
  uint32_t index_flags(uint32_t inx, uint32_t part, bool all_parts) const;
  /* The following defines can be increased if necessary */
  uint32_t max_supported_keys()          const { return BLACKHOLE_MAX_KEY; }
  uint32_t max_supported_key_length()    const { return BLACKHOLE_MAX_KEY_LENGTH; }
  uint32_t max_supported_key_part_length() const { return BLACKHOLE_MAX_KEY_LENGTH; }
  int open(const char *name, int mode, uint32_t test_if_locked);
  int close(void);
  int write_row(unsigned char * buf);
  int rnd_init(bool scan);
  int rnd_next(unsigned char *buf);
  int rnd_pos(unsigned char * buf, unsigned char *pos);
  int index_read_map(unsigned char * buf, const unsigned char * key, key_part_map keypart_map,
                     enum ha_rkey_function find_flag);
  int index_read_idx_map(unsigned char * buf, uint32_t idx, const unsigned char * key,
                         key_part_map keypart_map,
                         enum ha_rkey_function find_flag);
  int index_read_last_map(unsigned char * buf, const unsigned char * key, key_part_map keypart_map);
  int index_next(unsigned char * buf);
  int index_prev(unsigned char * buf);
  int index_first(unsigned char * buf);
  int index_last(unsigned char * buf);
  void position(const unsigned char *record);
  int info(uint32_t flag);
  int external_lock(Session *session, int lock_type);
  int create(const char *name, Table *table_arg,
             HA_CREATE_INFO *create_info);
  THR_LOCK_DATA **store_lock(Session *session,
                             THR_LOCK_DATA **to,
                             enum thr_lock_type lock_type);
};

#endif /* STORAGE_BLACKHOLE_HA_BLACKHOLE_H */
