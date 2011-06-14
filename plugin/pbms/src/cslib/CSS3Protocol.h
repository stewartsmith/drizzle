#pragma once
#ifndef __CSS3PROTOCOL_H__
#define __CSS3PROTOCOL_H__
/* Copyright (C) 2009 PrimeBase Technologies GmbH, Germany
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
 *  Created by Barry Leslie on 10/02/09.
 *
 */

#include "CSHTTPStream.h"
#include "CSMd5.h"
class S3ProtocolCon;

#define PUB_KEY_BIT		((uint8_t)1)
#define PRIV_KEY_BIT	((uint8_t)2)
#define SERVER_BIT		((uint8_t)4)


typedef struct S3Range {
	 off64_t startByte;
	 off64_t endByte;
} S3RangeRec, *S3RangePtr;

class CSS3Protocol: public CSRefObject {
	private:
	
	CSStringBuffer *s3_server;
	
	CSString *s3_public_key;
	CSString *s3_private_key;
	
	uint8_t	s3_ready; // A flag to indicate if the S3 protocol has been fully initialised.
	uint32_t s3_maxRetries; 
	uint32_t s3_sleepTime; 
	
	CSString *s3_getSignature(const char *verb, 
								const char *md5, 
								const char *content_type, 
								const char *date, 
								const char *bucket, 
								const char *key,
								CSString *headers = NULL 
							);
							
	public:
	CSS3Protocol();
		
	~CSS3Protocol();
	
	void s3_setServer(const char *server_arg)
	{
		s3_server->setLength(0);
		if (server_arg && *server_arg) {
			s3_ready |= SERVER_BIT;
			
			s3_server->append(server_arg);
			if (server_arg[strlen(server_arg)-1] != '/')
				s3_server->append("/");
		} else {
			s3_ready ^= SERVER_BIT;
		}
	}
	const char *s3_getServer() { return s3_server->getCString();}
	
	void s3_setPublicKey(const char *key_arg) 
	{
		s3_public_key->release();
		s3_public_key = NULL;
		if (key_arg && *key_arg) {
			s3_ready |= PUB_KEY_BIT;					
			s3_public_key = CSString::newString(key_arg);
		} else {
			s3_ready ^= PUB_KEY_BIT;
		}
	}
	const char *s3_getPublicKey() { return s3_public_key->getCString();}
	
	void s3_setPrivateKey(const char *key_arg)
	{
		s3_private_key->release();
		s3_private_key = NULL;
		if (key_arg && *key_arg) {
			s3_ready |= PRIV_KEY_BIT;					
			s3_private_key = CSString::newString(key_arg);
		} else {
			s3_ready ^= PRIV_KEY_BIT;
		}
	}

	void s3_setMaxRetries(uint32_t retries){s3_maxRetries = retries;}
	void s3_setSleepTime(uint32_t nap_time){s3_sleepTime = nap_time;}
	
	const char *s3_getPrivateKey() { return s3_private_key->getCString();}
	
	bool s3_isReady() { return ((s3_ready & SERVER_BIT) && (s3_ready & PUB_KEY_BIT) && (s3_ready & PRIV_KEY_BIT));}
	
	// s3_getAuthorization() returns the S3 Authorization sting and the time on which it was based.
	CSString *s3_getAuthorization(const char *bucket, const char *key, const char *content_type, uint32_t *s3AuthorizationTime);
	
	CSVector *s3_send(CSInputStream *input, const char *bucket, const char *key, off64_t size, const char *content_type = NULL, Md5Digest *digest = NULL, const char *s3Authorization = NULL, time_t s3AuthorizationTime = 0);

	// s3_receive() returns false if the object was not found.
	// If 'output' is NULL then only the headers will be fetched.
	CSVector *s3_receive(CSOutputStream *output, const char *bucket, const char *key, bool *found, S3RangePtr range = NULL, time_t *last_modified = NULL);

	// s3_delete() returns false if the object was not found.
	bool s3_delete(const char *bucket, const char *key);
	
	void s3_copy(const char *dest_server, const char *dest_bucket, const char *dest_key, const char *src_bucket, const char *src_key);
	
	// s3_list() returns a CSString list if the keys in the bucket with the specified prefix.
	// The list size returned can be limited with the 'max' parameter. The value 0 indicates no max.
	CSVector *s3_list(const char *bucket, const char *key_prefix = NULL, uint32_t max = 0);

	CSString *s3_getDataURL(const char *bucket, const char *key, uint32_t keep_alive);
};

#endif //__CSS3PROTOCOL_H__
