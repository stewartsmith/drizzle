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
 
#include "CSConfig.h"
#include <ctype.h>

#include "CSGlobal.h"
#include "CSString.h"

#include "CSEncode.h"

static const u_char base64Map[64] = {
	'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H', 
	'I', 'J', 'K', 'L', 'M', 'N', 'O', 'P', 
	'Q', 'R', 'S', 'T', 'U', 'V', 'W', 'X', 
	'Y', 'Z', 'a', 'b', 'c', 'd', 'e', 'f', 
	'g', 'h', 'i', 'j', 'k', 'l', 'm', 'n', 
	'o', 'p', 'q', 'r', 's', 't', 'u', 'v', 
	'w', 'x', 'y', 'z', '0', '1', '2', '3', 
	'4', '5', '6', '7', '8', '9', '+', '/'
};
 
static const u_char decodeBase64Map[128] = {
	0XBF, 0XFF, 0XF2, 0X2C, 0XBF, 0XFF, 0XF2, 0X24, 
	0X0, 0X0, 0X2E, 0X4C, 0X0, 0X0, 0X0, 0X3, 
	0X0, 0X0, 0X0, 0X3, 0XBF, 0XFF, 0XF1, 0X40, 
	0X0, 0X0, 0X0, 0X50, 0X8F, 0XE1, 0X56, 0XD8, 
	0X0, 0X30, 0X6, 0XEC, 0X0, 0X30, 0X6, 0XF0, 
	0X0, 0X0, 0X2E, 0X3E, 0X0, 0X0, 0X0, 0X3F, 
	0X34, 0X35, 0X36, 0X37, 0X38, 0X39, 0X3A, 0X3B, 
	0X3C, 0X3D, 0X2C, 0X68, 0X90, 0X0, 0X17, 0XD8, 
	0XBF, 0X0, 0X1, 0X2, 0X3, 0X4, 0X5, 0X6, 
	0X7, 0X8, 0X9, 0XA, 0XB, 0XC, 0XD, 0XE, 
	0XF, 0X10, 0X11, 0X12, 0X13, 0X14, 0X15, 0X16, 
	0X17, 0X18, 0X19, 0X80, 0X0, 0X0, 0X0, 0X0, 
	0X0, 0X1A, 0X1B, 0X1C, 0X1D, 0X1E, 0X1F, 0X20, 
	0X21, 0X22, 0X23, 0X24, 0X25, 0X26, 0X27, 0X28, 
	0X29, 0X2A, 0X2B, 0X2C, 0X2D, 0X2E, 0X2F, 0X30, 
	0X31, 0X32, 0X33, 0X0, 0X90, 0X1B, 0XBA, 0X48, 
};


static bool base64Encoded(const u_char *data, size_t len)
{
	if (len % 4)
		return false;
		
	size_t i = len;
	
	while ( i && data[i-1] == '=') i--;
	
	if ( (len - i) > 2)
		return false;
		
	for (; i ; i--, data++) {
		if (*data > 63)
			return false;
	}
	
	return true; // Actually this isn't so much 'true' as 'maybe'
}

char *base64Encode(const void *data, size_t len)
{
	u_char *wptr, *rptr, *encoding;
	size_t size;
	enter_();

	size = ((len + 2) / 3) * 4;
	encoding = (u_char *) cs_malloc(size +1);
	encoding[size] = 0;
	
	wptr= encoding;
	rptr = (u_char *) data;
	
	while (len/3) {		
		wptr[0] = base64Map[rptr[0] >>2];
		wptr[1] = base64Map[((rptr[0] & 0X03) <<4) + (rptr[1] >> 4)];
		wptr[2] = base64Map[((rptr[1] & 0X0F) <<2) + (rptr[2] >> 6)];
		wptr[3] = base64Map[(rptr[2] & 0X3F)];
		rptr += 3;
		wptr += 4;
		len -=3;
	}
	
	if (len) {
		wptr[0] = base64Map[rptr[0] >>2];

		if (len == 1) {
			wptr[1] = base64Map[(rptr[0] & 0x03) << 4]; 
			wptr[2] = wptr[3] = '=';
		} else {
			wptr[1] = base64Map[((rptr[0] & 0X03) <<4) + (rptr[1] >> 4)];
			wptr[2] = base64Map[(rptr[1] & 0X0F) <<2];
			wptr[3] = '=';
		}
		
	}
	
	return_((char*)encoding);
}

void *base64Decode(const char *data, size_t len)
{
	uint32_t tail;
	u_char *wptr, *rptr, *decoding;
	enter_();

	rptr = (u_char *) data;

	if (!base64Encoded(rptr, len)) 
		CSException::throwException(CS_CONTEXT, CS_ERR_GENERIC_ERROR, "String was not Base64 encoded.");
	
	decoding = (u_char *) cs_malloc((len/ 4) * 3);
	
	wptr= decoding;
	
	tail = len; 
	while (rptr[tail-1] == '=') 
		tail--; 

	len -= tail;
	while (len/4) {		
		wptr[0] = ( ((decodeBase64Map[rptr[0]] << 2) & 0xFF) | ((decodeBase64Map[rptr[1]] >> 4) & 0x03) ); 
		wptr[1] = ( ((decodeBase64Map[rptr[1]] << 4) & 0xFF) | ((decodeBase64Map[rptr[2]] >> 2) & 0x0F) ); 
		wptr[2] = ( ((decodeBase64Map[rptr[2]] << 6) & 0xFF) | (decodeBase64Map[rptr[3]] & 0x3F) ); 

		rptr += 4;
		wptr += 3;
		len -=4;
	}
	
	if (len) {
		wptr[0] = ( ((decodeBase64Map[rptr[0]] << 2) & 0xFF) |  ((decodeBase64Map[rptr[1]] >> 4) & 0x03) ); 
		if (len == 2) 
			wptr[1] = ( ((decodeBase64Map[rptr[1]] << 4) & 0xFF) | ((decodeBase64Map[rptr[2]] >> 2) & 0x0F) ); 
	}
	
	return_(decoding);
}

#ifdef NOT_USED
//-----------------------------------------------------
void  hmac_md5(u_char *text, size_t text_len, u_char *key, size_t key_len, Md5Digest *digest)
{
	CSMd5 md5;
	unsigned char k_ipad[65];    /* inner padding -
								  * key XORd with ipad
								  */
	unsigned char k_opad[65];    /* outer padding -
								  * key XORd with opad
								  */
	unsigned char tk[16];
	int i;
	
	/* if key is longer than 64 bytes reset it to key=MD5(key) */
	if (key_len > 64) {
		CSMd5 tmd5;

		md5.md5_init();
		md5.md5_append(key, key_len);
		md5.md5_digest(digest);

		key = digest->val;
		key_len = 16;
	}

	/* start out by storing key in pads */
	memset(k_ipad, 0, sizeof(k_ipad));
	memset(k_opad, 0, sizeof(k_opad));

	memcpy(k_ipad, key, key_len);
	memcpy(k_opad, key, key_len);

	/* XOR key with ipad and opad values */
	for (i=0; i<64; i++) {
		k_ipad[i] ^= 0x36;
		k_opad[i] ^= 0x5c;
	}
	/*
	 * perform inner encoding
	 */
	md5.md5_init();
	md5.md5_append(k_ipad, 64);
	md5.md5_append(text, strlen(text));
	md5.md5_digest(digest);

	/*
	 * perform outer encoding
	 */
	md5.md5_init();
	md5.md5_append(k_opad, 64);
	md5.md5_append((u_char*)digest, 16);
	md5.md5_digest(digest);
	
}

#endif // NOT_USED

//-----------------------------------------------------
void  hmac_sha1(const char *text, const char *key, Sha1Digest *digest)
{
	CSSha1 sha1;
	size_t key_len = strlen(key);
	unsigned char k_ipad[65];    /* inner padding -
								  * key XORd with ipad
								  */
	unsigned char k_opad[65];    /* outer padding -
								  * key XORd with opad
								  */
	int i;
	
	/* if key is longer than 64 bytes reset it to key=MD5(key) */
	if (key_len > 64) {
		CSMd5 tmd5;

		sha1.sha1_reset();
		sha1.sha1_input(key, key_len);
		sha1.sha1_digest(digest);

		key = (char *) digest->val;
		key_len = 16;
	}

	/* start out by storing key in pads */
	memset(k_ipad, 0, sizeof(k_ipad));
	memset(k_opad, 0, sizeof(k_opad));

	memcpy(k_ipad, key, key_len);
	memcpy(k_opad, key, key_len);

	/* XOR key with ipad and opad values */
	for (i=0; i<64; i++) {
		k_ipad[i] ^= 0x36;
		k_opad[i] ^= 0x5c;
	}

	/*
	 * perform inner encoding
	 */
	sha1.sha1_reset();
	sha1.sha1_input(k_ipad, 64);
	sha1.sha1_input(text, strlen(text));
	sha1.sha1_digest(digest);


	/*
	 * perform outer encoding
	 */
	sha1.sha1_reset();
	sha1.sha1_input(k_opad, 64);
	sha1.sha1_input(digest, 20);
	sha1.sha1_digest(digest);
}


//-----------------------------------------------------
CSString *signature(const char *text, const char *key)
{
	Sha1Digest digest;
	char *encoding;
	CSString *signed_str;
	
	memset(&digest, 0, sizeof(digest));
	hmac_sha1(text, key, &digest);

	encoding = base64Encode(digest.val, SHA1_HASH_SIZE);
	signed_str = CSString::newString(encoding);
	cs_free(encoding);
	return signed_str;
}


//-----------------------------------------------------
// Encode url unsafe characters
CSString *urlEncode(CSString *src)
{
	const char *hex = "0123456789ABCDEF", *start, *ptr;
	char encoded[3];
	CSStringBuffer *url;
	
	enter_();
	push_(src);
	
	new_(url, CSStringBuffer(10));
	push_(url);
	
	start = ptr = src->getCString();
	encoded[0] = '%';
	
	while(*ptr) {
		if (!isalnum(*ptr)) {
			switch (*ptr) {
				case '.':
				case '!':
				case '*':
				case '~':
				case '\'':
				case '(':
				case ')':
				case '_':
				case '-':
				case '/':
					break;
				default:
					url->append(start, ptr-start);
					start = ptr +1;
					encoded[1] = hex[*ptr/16];
					encoded[2] = hex[*ptr%16];				
					url->append(encoded, 3);
			}
		}
		ptr++;		
	}
	url->append(start, ptr-start);	
	
	CSString *safe_str = CSString::newString(url->getCString());
	
	release_(url);
	release_(src);
	return_(safe_str);
}
