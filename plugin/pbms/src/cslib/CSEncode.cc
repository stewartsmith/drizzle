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
 *  Created by Barry Leslie on 10/21/08.
 *
 */
 
#include "CSConfig.h"
#include <ctype.h>

#include "CSGlobal.h"
#include "CSString.h"

#include "CSEncode.h"

static const u_char base64URLMap[64] = {
	'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H', 
	'I', 'J', 'K', 'L', 'M', 'N', 'O', 'P', 
	'Q', 'R', 'S', 'T', 'U', 'V', 'W', 'X', 
	'Y', 'Z', 'a', 'b', 'c', 'd', 'e', 'f', 
	'g', 'h', 'i', 'j', 'k', 'l', 'm', 'n', 
	'o', 'p', 'q', 'r', 's', 't', 'u', 'v', 
	'w', 'x', 'y', 'z', '0', '1', '2', '3', 
	'4', '5', '6', '7', '8', '9', '-', '_'
};
static const u_char base64STDMap[64] = {
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
	0XFF, 0XFF, 0XFF, 0XFF, 0XFF, 0XFF, 0XFF, 0XFF, 
	0XFF, 0XFF, 0XFF, 0XFF, 0XFF, 0XFF, 0XFF, 0XFF, 
	0XFF, 0XFF, 0XFF, 0XFF, 0XFF, 0XFF, 0XFF, 0XFF, 
	0XFF, 0XFF, 0XFF, 0XFF, 0XFF, 0XFF, 0XFF, 0XFF, 
	0XFF, 0XFF, 0XFF, 0XFF, 0XFF, 0XFF, 0XFF, 0XFF, 
	0XFF, 0XFF, 0XFF, 0X3E, 0XFF, 0X3E, 0XFF, 0X3F, 
	0X34, 0X35, 0X36, 0X37, 0X38, 0X39, 0X3A, 0X3B, 
	0X3C, 0X3D, 0XFF, 0XFF, 0XFF, 0XFF, 0XFF, 0XFF, 
	0XFF, 0X00, 0X01, 0X02, 0X03, 0X04, 0X05, 0X06, 
	0X07, 0X08, 0X09, 0X0A, 0X0B, 0X0C, 0X0D, 0X0E, 
	0X0F, 0X10, 0X11, 0X12, 0X13, 0X14, 0X15, 0X16, 
	0X17, 0X18, 0X19, 0XFF, 0XFF, 0XFF, 0XFF, 0X3F, 
	0XFF, 0X1A, 0X1B, 0X1C, 0X1D, 0X1E, 0X1F, 0X20, 
	0X21, 0X22, 0X23, 0X24, 0X25, 0X26, 0X27, 0X28, 
	0X29, 0X2A, 0X2B, 0X2C, 0X2D, 0X2E, 0X2F, 0X30, 
	0X31, 0X32, 0X33, 0XFF, 0XFF, 0XFF, 0XFF, 0XFF, 
};

//------------------
static bool base64Encoded(const u_char *data, size_t len, const u_char *vc)
{
	if (len % 4)
		return false;
		
	size_t i = len;
	
	while ( i && data[i-1] == '=') i--;
	
	if ( (len - i) > 2)
		return false;
		
	for (; i ; i--, data++) {
		if (((*data < 'A') || (*data > 'Z')) &&
			((*data < 'a') || (*data > 'z')) &&
			((*data < '0') || (*data > '9')) &&
			((*data != vc[0]) && (*data != vc[1])))
			return false;
	}
	
	return true; // Actually this isn't so much 'true' as 'maybe'
}

//------------------
static char *genericBase64Encode(const void *data, size_t len, char *encode_buffer, size_t encode_buffer_len, const u_char base64Map[])
{
	u_char *wptr, *rptr, *encoding;
	size_t size;
	enter_();

	size = ((len + 2) / 3) * 4 +1;
	if ((encode_buffer != NULL) && (encode_buffer_len < size)) 
		CSException::throwException(CS_CONTEXT, CS_ERR_GENERIC_ERROR, "Base64 encode buffer is too small.");
	
	if (encode_buffer)
		encoding = (u_char *) encode_buffer;
	else
		encoding = (u_char *) cs_malloc(size);
		
	size--;
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

//------------------
static void *genericBase64Decode(const char *data, size_t len, void *decode_buffer, size_t decode_buffer_len, const u_char base64Map[])
{
	u_char *wptr, *rptr, *decoding;
	enter_();

	rptr = (u_char *) data;

	if (!base64Encoded(rptr, len, base64Map +62)) 
		CSException::throwException(CS_CONTEXT, CS_ERR_GENERIC_ERROR, "String was not Base64 encoded.");
	
	if ((decode_buffer != NULL) && (decode_buffer_len < ((len/ 4) * 3))) 
		CSException::throwException(CS_CONTEXT, CS_ERR_GENERIC_ERROR, "Base64 decoded buffer is too small.");
	
	if (decode_buffer)
		decoding = (u_char *) decode_buffer;
	else
		decoding = (u_char *) cs_malloc((len/ 4) * 3);
	
	wptr= decoding;
	
	while (rptr[len-1] == '=') 
		len--; 

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

//------------------
char *base64Encode(const void *data, size_t len, char *encode_buffer, size_t encode_buffer_len)
{
	return genericBase64Encode(data, len, encode_buffer, encode_buffer_len, base64STDMap);
}

//------------------
void *base64Decode(const char *data, size_t len, void *decode_buffer, size_t decode_buffer_len)
{
	return genericBase64Decode(data, len, decode_buffer, decode_buffer_len, base64STDMap);
}

//------------------
char *base64UrlEncode(const void *data, size_t len, char *encode_buffer, size_t encode_buffer_len)
{
	return genericBase64Encode(data, len, encode_buffer, encode_buffer_len, base64URLMap);
}

//------------------
void *base64UrlDecode(const char *data, size_t len, void *decode_buffer, size_t decode_buffer_len)
{
	return genericBase64Decode(data, len, decode_buffer, decode_buffer_len, base64URLMap);
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
