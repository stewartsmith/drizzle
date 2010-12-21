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

  CHANGES
    2002 by Peter Zaitsev to
     - fit to new prototypes according to MySQL standard
     - Some optimizations
     - All checking is now done in debug only mode
     - More comments
*/

#include "CSConfig.h"
#include <string.h>
#include <stdint.h>

#include "CSDefs.h"
#include "CSObject.h"

#include "CSSha1.h"

#define SHA1CircularShift(bits,word) (((word) << (bits)) | ((word) >> (32-(bits))))

void CSSha1::sha1_reset()
{
	Length = 0;
	Message_Block_Index = 0;

	Intermediate_Hash[0] = 0x67452301;
	Intermediate_Hash[1] = 0xEFCDAB89;
	Intermediate_Hash[2] = 0x98BADCFE;
	Intermediate_Hash[3] = 0x10325476;
	Intermediate_Hash[4] = 0xC3D2E1F0;

	Computed = false;

}

//------------------------------
void CSSha1::sha1_digest(Sha1Digest *digest)
{
	if (!Computed) {
		sha1_pad();		
		memset(Message_Block, 0, 64);
		Length = 0;
		Computed = true;
	}

	for (int i = 0; i < SHA1_HASH_SIZE; i++)
		digest->val[i] = (int8_t)((Intermediate_Hash[i>>2] >> 8 * ( 3 - ( i & 0x03 ) )));
}

//------------------------------
void CSSha1::sha1_input(const void *data, size_t len)
{
	uint8_t *message_array = (u_char *) data;
	
	if (!len)
		return;

	if (Computed)
		sha1_reset();

	while (len--) {
		Message_Block[Message_Block_Index++]= (*message_array & 0xFF);
		Length += 8;  /* Length is in bits */

		if (Message_Block_Index == 64)
			sha1_process();

		message_array++;
	}
}

//------------------------------
void CSSha1::sha1_pad()
{
	/*
	Check to see if the current message block is too small to hold
	the initial padding bits and length.  If so, we will pad the
	block, process it, and then continue padding into a second
	block.
	*/

	int i = Message_Block_Index;

	if (i > 55) {
		Message_Block[i++] = 0x80;
		memset(&Message_Block[i], 0, sizeof(Message_Block[0]) * 64-i);
		Message_Block_Index=64;

		/* This function sets Message_Block_Index to zero	*/
		sha1_process();

		memset(Message_Block, 0, sizeof(Message_Block[0]) * 56);
		Message_Block_Index=56;
	} else {
		Message_Block[i++] = 0x80;
		memset(&Message_Block[i], 0, sizeof(Message_Block[0])*(56-i));
		Message_Block_Index=56;
	}

	/*
	Store the message length as the last 8 octets
	*/

	Message_Block[56] = (int8_t) (Length >> 56);
	Message_Block[57] = (int8_t) (Length >> 48);
	Message_Block[58] = (int8_t) (Length >> 40);
	Message_Block[59] = (int8_t) (Length >> 32);
	Message_Block[60] = (int8_t) (Length >> 24);
	Message_Block[61] = (int8_t) (Length >> 16);
	Message_Block[62] = (int8_t) (Length >> 8);
	Message_Block[63] = (int8_t) (Length);

	sha1_process();
}

//------------------------------
void CSSha1::sha1_process()
{
    const uint32_t K[] = { 0x5A827999, 0x6ED9EBA1, 0x8F1BBCDC, 0xCA62C1D6 };
	int		t;		   /* Loop counter		  */
	uint32_t	temp;		   /* Temporary word value	  */
	uint32_t	W[80];		   /* Word sequence		  */
	uint32_t	A, B, C, D, E;	   /* Word buffers		  */
	int idx;

	/*
	Initialize the first 16 words in the array W
	*/

	for (t = 0; t < 16; t++) {
		idx=t*4;
		W[t] = Message_Block[idx] << 24;
		W[t] |= Message_Block[idx + 1] << 16;
		W[t] |= Message_Block[idx + 2] << 8;
		W[t] |= Message_Block[idx + 3];
	}


	for (t = 16; t < 80; t++) {
		W[t] = SHA1CircularShift(1,W[t-3] ^ W[t-8] ^ W[t-14] ^ W[t-16]);
	}

	A = Intermediate_Hash[0];
	B = Intermediate_Hash[1];
	C = Intermediate_Hash[2];
	D = Intermediate_Hash[3];
	E = Intermediate_Hash[4];

	for (t = 0; t < 20; t++) {
		temp= SHA1CircularShift(5,A) + ((B & C) | ((~B) & D)) + E + W[t] + K[0];
		E = D;
		D = C;
		C = SHA1CircularShift(30,B);
		B = A;
		A = temp;
	}

	for (t = 20; t < 40; t++) {
		temp = SHA1CircularShift(5,A) + (B ^ C ^ D) + E + W[t] + K[1];
		E = D;
		D = C;
		C = SHA1CircularShift(30,B);
		B = A;
		A = temp;
	}

	for (t = 40; t < 60; t++) {
		temp= (SHA1CircularShift(5,A) + ((B & C) | (B & D) | (C & D)) + E + W[t] + K[2]);
		E = D;
		D = C;
		C = SHA1CircularShift(30,B);
		B = A;
		A = temp;
	}

	for (t = 60; t < 80; t++) {
		temp = SHA1CircularShift(5,A) + (B ^ C ^ D) + E + W[t] + K[3];
		E = D;
		D = C;
		C = SHA1CircularShift(30,B);
		B = A;
		A = temp;
	}

	Intermediate_Hash[0] += A;
	Intermediate_Hash[1] += B;
	Intermediate_Hash[2] += C;
	Intermediate_Hash[3] += D;
	Intermediate_Hash[4] += E;

	Message_Block_Index = 0;
}


