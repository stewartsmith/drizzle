 #ifndef __CSENCODE_H__
#define __CSENCODE_H__
/* Copyright (c) 2008 PrimeBase Technologies GmbH, Germany
 *
 * PrimeBase S3Daemon
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
 *  Created by Barry Leslie on 10/21/08.
 *
 */

#include "CSSha1.h"
#include "CSMd5.h"

char *base64Encode(const void *data, size_t len);
void *base64Decode(const char *data, size_t len);

CSString *signature(const char *text, const char *key);
CSString *urlEncode(CSString *src);
	
//void hmac_md5(u_char *text, size_t text_len, u_char *key, size_t key_len, Md5Digest *digest);
void hmac_sha1(const char *text, const char *key, Sha1Digest *digest);

#endif // __CSENCODE_H__
