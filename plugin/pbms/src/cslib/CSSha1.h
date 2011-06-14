/* Copyright (C) 2008 PrimeBase Technologies GmbH, Germany
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
 *  Modified by Barry Leslie on 10/21/08.
 *	I have put a C++ wrapper around the data and functions to create an sha1 class.
 *
 */
/*
  Original Source from: http://www.faqs.org/rfcs/rfc3174.html
  and MySQL mysys/sha1.c build 5.1.24.

 DESCRIPTION
   This file implements the Secure Hashing Algorithm 1 as
   defined in FIPS PUB 180-1 published April 17, 1995.

   The SHA-1, produces a 160-bit message digest for a given data
   stream.  It should take about 2**n steps to find a message with the
   same digest as a given message and 2**(n/2) to find any two
   messages with the same digest, when n is the digest size in bits.
   Therefore, this algorithm can serve as a means of providing a
   "fingerprint" for a message.

 PORTABILITY ISSUES
   SHA-1 is defined in terms of 32-bit "words".  This code uses
   <stdint.h> (included via "sha1.h" to define 32 and 8 bit unsigned
   integer types.  If your C compiler does not support 32 bit unsigned
   integers, this code is not appropriate.

 CAVEATS
   SHA-1 is designed to work with messages less than 2^64 bits long.
   Although SHA-1 allows a message digest to be generated for messages
   of any number of bits less than 2^64, this implementation only
   works with messages with a length that is a multiple of the size of
   an 8-bit character.

*/

#pragma once
#ifndef __CSSHA1_H__
#define __CSSHA1_H__
#include <string.h>

#define SHA1_HASH_SIZE 20

typedef struct {
	uint8_t val[SHA1_HASH_SIZE];
} Sha1Digest;

class CSSha1 : public CSObject {
	private:
	
	uint64_t  Length;								// Message length in bits   
	uint32_t Intermediate_Hash[SHA1_HASH_SIZE/4];	// Message Digest  
	bool Computed;									// Is the digest computed?
	int16_t Message_Block_Index;					// Index into message block array  
	uint8_t Message_Block[64];						// 512-bit message blocks      

	void sha1_pad();
	void sha1_process();

	public:
	CSSha1() {sha1_reset();}
	
	void sha1_reset();
	void sha1_input(const void *data, size_t len);
	void sha1_digest(Sha1Digest *digest);
	
};

#endif // __CSSHA1_H__

