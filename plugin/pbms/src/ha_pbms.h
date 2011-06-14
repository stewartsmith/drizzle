/* Copyright (C) 2008 PrimeBase Technologies GmbH, Germany
 *
 * PrimeBase Media Stream for MySQL
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA
 *
 * Original author: Paul McCullagh
 * Continued development: Barry Leslie
 *
 * 2007-05-20
 *
 * H&G2JCtL
 *
 * Table handler.
 *
 */
#pragma once
#ifndef __HA_PBMS_H__
#define __HA_PBMS_H__

#include "defs_ms.h"
#include "engine_ms.h"

#ifdef USE_PRAGMA_INTERFACE
#pragma interface			/* gcc class implementation */
#endif

#if MYSQL_VERSION_ID >= 50120
#define byte uchar
#endif

#ifdef DRIZZLED
#include <drizzled/cursor.h>
#include <drizzled/thr_lock.h>

class PBMSStorageEngine;
#define handlerton PBMSStorageEngine
#define handler	Cursor

using namespace drizzled;

#else
extern handlerton		*pbms_hton;
#endif

class MSOpenSystemTable;

class ha_pbms: public handler
{
	THR_LOCK_DATA		ha_lock;			///< MySQL lock
	MSOpenSystemTable	*ha_open_tab;
	int					ha_error;
	PBMSResultRec		ha_result;
	//MS_SHARE			*ha_share;		///< Shared lock info

public:
#ifdef DRIZZLED
	ha_pbms(handlerton *hton, Table& table_arg);
#else
	ha_pbms(handlerton *hton, TABLE_SHARE *table_arg);
#endif
	~ha_pbms() { }

	const char *table_type() const { return "PBMS"; }

	const char *index_type(uint inx) { UNUSED(inx); return "NONE"; }

#ifndef DRIZZLED
	const char **bas_ext() const;

	MX_TABLE_TYPES_T table_flags() const;
#endif

	MX_ULONG_T index_flags(uint inx, uint part , bool all_parts ) const 
	{ 
		UNUSED(inx); 
		UNUSED(part); 
		UNUSED(all_parts); 
		return (HA_READ_NEXT | HA_READ_PREV | HA_READ_RANGE | HA_KEYREAD_ONLY); 
	}
	uint	max_supported_keys()			const { return 512; }
	uint	max_supported_key_length()    const { return 1024; }
	uint	max_supported_key_part_length() const { return 1024; }

	int open(const char *name, int mode, uint test_if_locked);
	void drop_table(const char *name) {UNUSED(name);}

	int close(void);
#ifdef DRIZZLED
	int		doInsertRecord(byte * buf);
	int		doUpdateRecord(const byte * old_data, byte * new_data);
	int		doDeleteRecord(const byte * buf);

#else
	int write_row(unsigned char * buf);
	int update_row(const unsigned char * old_data, unsigned char * new_data);
	int delete_row(const unsigned char * buf);
#endif

	/* Sequential scan functions: */
#ifdef DRIZZLED
	int	doStartTableScan(bool scan);
#else
	int rnd_init(bool scan);
#endif
	int rnd_next(byte *buf);
	int rnd_pos(byte * buf, byte *pos);
	void position(const byte *record);
	int info(uint);

#ifdef PBMS_HAS_KEYS
	/* Index access functions: */
	int		index_init(uint idx, bool sorted);
	int		index_end();
	int		index_read(byte * buf, const byte * key,
								 uint key_len, enum ha_rkey_function find_flag);
	int		index_read_idx(byte * buf, uint idx, const byte * key,
										 uint key_len, enum ha_rkey_function find_flag);
	int		index_next(byte * buf);
	int		index_prev(byte * buf);
	int		index_first(byte * buf);
	int		index_last(byte * buf);
	int		index_read_last(byte * buf, const byte * key, uint key_len);
#endif
	
	int		external_lock(THD *thd, int lock_type);
#ifndef DRIZZLED
	int create(const char *name, TABLE *form, HA_CREATE_INFO *create_info);
#endif
  void get_auto_increment(uint64_t, uint64_t,
                          uint64_t,
                          uint64_t *,
                          uint64_t *)
  {}

	THR_LOCK_DATA **store_lock(THD *thd, THR_LOCK_DATA **to, enum thr_lock_type lock_type);

	bool get_error_message(int error, String *buf);

};

#endif

