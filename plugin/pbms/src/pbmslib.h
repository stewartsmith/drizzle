/* Copyright (C) 2008 PrimeBase Technologies GmbH, Germany
 *
 *  PrimeBase Media Stream (PBMS)
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA
 *
 * 2008-09-10	Barry Leslie
 *
 * H&G2JCtL
 */
#pragma once
#ifndef __PBMSLIB_H__
#define __PBMSLIB_H__

#ifdef __cplusplus
extern "C" {
#endif

#define MS_STANDARD_STORAGE		0	// BLOB data is stored in the repository.
#define MS_CLOUD_STORAGE		1	// BLOB data is in S3 cloud storage.

/*<DRIZZLE>*/
#define MS_CHECKSUM_TAG       "PBMS_CHECKSUM"
#define MS_ALIAS_TAG          "PBMS_BLOB_ALIAS"
#define MS_BLOB_INFO_REQUEST	"PBMS_RETURN_HEADER_ONLY"
#define MS_PING_REQUEST       "PBMS_PING_CONNECTION"
#define MS_BLOB_SIZE          "PBMS_BLOB_SIZE"
#define MS_LAST_ACCESS        "PBMS_LAST_ACCESS"
#define MS_ACCESS_COUNT       "PBMS_ACCESS_COUNT"
#define MS_CREATION_TIME      "PBMS_CREATION_TIME"
#define MS_BLOB_TYPE          "PBMS_BLOB_TYPE"

#define MS_REQUEST_SIZE       "PBMS_REQUEST_SIZE"     // The number of bytes of BLOB data the client requests.
#define MS_REQUEST_OFFSET     "PBMS_REQUEST_OFFSET"   // The offset into the BLOB that the client requests the data from.

#define MS_CLOUD_SERVER       "PBMS_CLOUD_SERVER"
#define MS_CLOUD_BUCKET       "PBMS_CLOUD_BUCKET"
#define MS_CLOUD_OBJECT_KEY		"PBMS_CLOUD_OBJECT_KEY"
#define MS_CLOUD_KEY          "PBMS_CLOUD_KEY"
#define MS_BLOB_SIGNATURE     "PBMS_BLOB_SIGNATURE"
#define MS_BLOB_DATE          "PBMS_BLOB_DATE"	// The date used when creating the signature.

#define MS_META_NAME_SIZE		32
#define MS_META_VALUE_SIZE		1024
#define MS_BLOB_ALIAS_SIZE		MS_META_VALUE_SIZE
#define MS_BLOB_URL_SIZE		200

#ifndef PBMS_PORT
#define DEFAULT_PBMS_PORT 8080
#else
#define DEFAULT_PBMS_PORT PBMS_PORT
#endif

/* PBMS handle types. */
typedef void *PBMS;				// A connection handle.

typedef unsigned char pbms_bool;

/* options for pbms_set_option() and pbms_get_option() */
enum pbms_option						/*: Parameter type	|	Information																										*/
{
  PBMS_OPTION_KEEP_ALIVE,				/*: unsigned int*	|	A boolean value indicating if the keep_alive flag should be set on HTTP requests.(Defalt = false)	*/
  PBMS_OPTION_TRANSMITION_TIMEOUT,		/*: unsigned int*	|	If greater than zero this sets a limit to how long a blob transfer can take.  (Defalt = 0)						*/
  PBMS_OPTION_HOST,						/*: (const char *)*	|	The connection's Media Stream server host IP address. (Read Only)												*/
  PBMS_OPTION_PORT,						/*: unsigned int*	|	The connection's Media Stream server host port number. (Read Only)												*/
  PBMS_OPTION_DATABASE					/*: (const char *)*	|	The connection's associated database.																			*/
};

typedef size_t (* PBMS_READ_CALLBACK_FUNC) (void *caller_data, char *buffer, size_t size, pbms_bool reset);
typedef size_t (* PBMS_WRITE_CALLBACK_FUNC) (void *caller_data, const char *buffer, size_t size, pbms_bool reset);

pbms_bool pbms_library_init();
void	pbms_library_end();
PBMS pbms_connect(const char* host, unsigned int port, const char *database);


void pbms_close(PBMS pbms);
pbms_bool pbms_set_option(PBMS pbms, enum pbms_option option, const void *in_value);
pbms_bool pbms_get_option(PBMS pbms, enum pbms_option option, void *out_value);
int pbms_errno(PBMS pbms);
const char *pbms_error(PBMS pbms);
pbms_bool pbms_is_blob_reference(PBMS pbms, const char *ref);
pbms_bool pbms_get_blob_size(PBMS pbms, const char *ref, size_t *size);

/* Metadata related functions. */
pbms_bool pbms_add_metadata(PBMS pbms, const char *name, const char *value);
void pbms_clear_metadata(PBMS pbms, const char *name);	//If name is NULL all metadata associaed with the connection is removed.
unsigned int pbms_reset_metadata(PBMS pbms);			//Resets the metadata cursor for downloaded metadata.
pbms_bool pbms_next_metadata(PBMS pbms, char *name, char *value, size_t *v_size);
pbms_bool pbms_get_metadata_value(PBMS pbms, const char *name, char *buffer, size_t *size);

pbms_bool pbms_get_md5_digest(PBMS pbms, char *md5_digest); 

pbms_bool pbms_put_data(PBMS pbms, const char *table, const char *checksum, char *ref, size_t size, const unsigned char *buffer);
pbms_bool pbms_put_data_cb(PBMS pbms, const char *table, const char *checksum, char *ref, size_t size, PBMS_READ_CALLBACK_FUNC cb, void *caller_data);

pbms_bool pbms_get_data(PBMS pbms, const char *ref, unsigned char *buffer, size_t buffer_size);
pbms_bool pbms_get_data_cb(PBMS pbms, const char *ref, PBMS_WRITE_CALLBACK_FUNC cb, void *caller_data);

pbms_bool pbms_get_data_range(PBMS pbms, const char *ref, size_t start_offset, size_t end_offset, unsigned char *buffer, size_t buffer_size, size_t *data_size);
pbms_bool pbms_get_data_range_cb(PBMS pbms, const char *ref, size_t start_offset, size_t end_offset, PBMS_WRITE_CALLBACK_FUNC cb, void *caller_data);

pbms_bool pbms_get_info(PBMS pbms, const char *ref);

#ifdef __cplusplus
}
#endif
#endif // __PBMSLIB_H__
