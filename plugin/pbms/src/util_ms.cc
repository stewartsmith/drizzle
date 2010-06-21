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
 * 2007-05-25
 *
 * H&G2JCtL
 *
 * Network interface.
 *
 */

/* I must do this in order to get the MS config.h, which
 * will give us the correct value for VERSION!
 */
#ifdef MYSQL_SERVER
#undef MYSQL_SERVER
#endif

#include "cslib/CSConfig.h"
#include <inttypes.h>

#include <string.h>
#include <ctype.h>

#include "cslib/CSGlobal.h"
#include "cslib/CSStrUtil.h"

#include "engine_ms.h"
#include "util_ms.h"
#include "version_ms.h"

/*
 * A file name has the form:
 * <text>-<number>[.<ext>]
 * This function return the number part as a
 * uint32_t.
 */
uint32_t ms_file_to_table_id(const char *file_name, const char *name_part)
{
	uint32_t value = 0;

	if (file_name) {
		const char *num = file_name +  strlen(file_name) - 1;
		
		while (num >= file_name && *num != '-')
			num--;
		if (name_part) {
			/* Check the name part of the file: */
			int len = strlen(name_part);
			
			if (len != num - file_name)
				return 0;
			if (strncmp(file_name, name_part, len) != 0)
				return 0;
		}
		num++;
		if (isdigit(*num))
			sscanf(num, "%"PRIu32"", &value);
	}
	return value;
}

uint32_t ms_file_to_table_id(const char *file_name)
{
	return ms_file_to_table_id(file_name, NULL);
}

const char *ms_file_to_table_name(size_t size, char *tab_name, const char *file_name)
{
	const char	*cptr;
	size_t		len;

	file_name = cs_last_name_of_path(file_name);
	cptr = file_name + strlen(file_name) - 1;
	while (cptr > file_name && *cptr != '.')
		cptr--;
	if (cptr > file_name && *cptr == '.') {
		if (strncmp(cptr, ".bs", 2) == 0) {
			cptr--;
			while (cptr > file_name && isdigit(*cptr))
				cptr--;
		}
	}

	len = cptr - file_name;
	if (len > size-1)
		len = size-1;

	memcpy(tab_name, file_name, len);
	tab_name[len] = 0;

	/* Return a pointer to what was removed! */
	return file_name + len;
}

bool ms_parse_blob_url(MSBlobURLPtr blob, const char *url)
{
	uint32_t		db_id = 0;
	uint32_t		tab_id = 0;
	uint64_t		blob_id = 0;
	uint64_t		blob_size = 0;
	uint64_t		blob_ref_id = 0;
	uint32_t		auth_code = 0;
	uint32_t		server_id = 0;
	char		type, junk[5];
	int			scanned;

	junk[0] = 0;
	
	//If this changes do not forget to update couldBeURL in pbms.h.
	scanned = sscanf(url, URL_FMT"%4s", &db_id, &type, &tab_id, &blob_id, &auth_code, &server_id, &blob_ref_id, &blob_size, junk);
	if (scanned != 8) // If junk is found at the end this will also result in an invalid URL. 
		return false;
	
	if (junk[0] || (type != MS_URL_TYPE_BLOB && type != MS_URL_TYPE_REPO))
		return false;
		
	blob->bu_type = type;
	blob->bu_db_id = db_id;
	blob->bu_tab_id = tab_id;
	blob->bu_blob_id = blob_id;
	blob->bu_auth_code = auth_code;
	blob->bu_server_id = server_id;
	blob->bu_blob_ref_id = blob_ref_id;
	blob->bu_blob_size = blob_size;
	return blob->bu_db_id && tab_id && blob_id;
}

void ms_build_blob_url(MSBlobURLPtr blob, char *url)
{
	snprintf(url, PBMS_BLOB_URL_SIZE, URL_FMT,	blob->bu_db_id, 
							blob->bu_type, 
							blob->bu_tab_id, 
							blob->bu_blob_id, 
							blob->bu_auth_code, 
							blob->bu_server_id, 
							blob->bu_blob_ref_id, 
							blob->bu_blob_size);
}

#define _STR(x) #x
#define STR(x) _STR(x)
const char *ms_version()
{
	return STR(PBMS_VERSION);
}



