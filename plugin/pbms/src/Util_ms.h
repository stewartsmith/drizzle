/* Copyright (c) 2008 PrimeBase Technologies GmbH, Germany
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
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 *
 * Original author: Paul McCullagh
 * Continued development: Barry Leslie
 *
 * 2007-06-26
 *
 * H&G2JCtL
 *
 * Various utilities.
 *
 */

#ifndef __UTIL_MS_H__
#define __UTIL_MS_H__

#include "Defs_ms.h"
#include "pbms.h"

#define MS_URL_TYPE_BLOB	'~'
#define MS_URL_TYPE_REPO	'_'


typedef struct MSBlobURL {
	int					bu_type;
	uint32_t				bu_db_id;
	uint32_t				bu_tab_id;				// or repo ID if type = REPO
	uint64_t				bu_blob_id;				// or repo file offset if type = REPO
	uint32_t				bu_auth_code;
	uint32_t				bu_server_id;
	uint64_t				bu_blob_size;			
	uint64_t				bu_blob_ref_id;			// Unique identifier of the blob reference
} MSBlobURLRec, *MSBlobURLPtr;

uint32_t		ms_file_to_table_id(const char *file_name, const char *name_part);
uint32_t		ms_file_to_table_id(const char *file_name);
const char	*ms_file_to_table_name(size_t size, char *tab_name, const char *file_name);
bool		ms_parse_blob_url(MSBlobURLPtr blob, const char *url);
void		ms_build_blob_url(MSBlobURLPtr blob, char *url);
const char	*ms_version();

/*
 * The pbms_ functions are utility functions supplied by ha_pbms.cc
 */
void	pbms_take_part_in_transaction(void *thread);
int		pbms_enter_conn_no_thd(CSThread **r_self, PBMSResultPtr result);
void	pbms_exit_conn();
int		pbms_exception_to_result(CSException *e, PBMSResultPtr result);
int		pbms_os_error_result(const char *func, const char *file, int line, int err, PBMSResultPtr result);
int		pbms_error_result(const char *func, const char *file, int line, int err, const char *message, PBMSResultPtr result);

#endif
