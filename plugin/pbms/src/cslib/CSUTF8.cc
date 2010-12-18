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
 * Original author: Paul McCullagh (H&G2JCtL)
 * Continued development: Barry Leslie
 *
 * 2007-06-14
 *
 * CORE SYSTEM:
 * Unicode / UTF-8 convertion
 *
 */

#include "CSConfig.h"

#include <assert.h>
#include <string.h>

#include "CSUTF8.h"
#include "CSMemory.h"
#include "CSGlobal.h"

size_t cs_utf_to_uni_char(const u_char *in_string, uint32_t *ret_value)
{
	const u_char *i_string =  in_string;
	size_t	s_len = strlen((char*)in_string);
	u_char	ch;
	uint32_t	val;
	size_t	clen;

	ch = *i_string;
	if ((ch & 0x80) == 0x00) {
		val = (uint32_t) ch & 0x0000007F;
		clen = 1;
	}
	else if ((ch & 0xE0) == 0xC0) {
		if (s_len > 1) {
			val = ((i_string[0] & 0x0000001F) << 6) |
						(i_string[1] & 0x0000003F);
			if (val < 0x00000080)
				val = '?';
			clen = 2;
		}
		else {
			val = '?';
			clen = s_len;
		}
	}
	else if ((ch & 0xF0) == 0xE0) {
		if (s_len > 2) {
			val = ((i_string[0] & 0x0000000F) << 12) |
						((i_string[1] & 0x0000003F) << 6) |
						(i_string[2] & 0x0000003F);
			if (val < 0x000000800)
				val = '?';
			clen = 3;
		}
		else {
			val = '?';
			clen = s_len;
		}
	}
	else if ((ch & 0xF8) == 0xF0) {
		if (s_len > 3) {
			val = ((i_string[0] & 0x00000007) << 18) |
						((i_string[1] & 0x0000003F) << 12) |
						((i_string[2] & 0x0000003F) << 6) |
						(i_string[3] & 0x0000003F);
			if (val < 0x00010000)
				val = '?';
			clen = 4;
		}
		else {
			val = '?';
			clen = s_len;
		}
	}
	else if ((ch & 0xFC) == 0xF8) {
		if (s_len > 4) {
			val = ((i_string[0] & 0x00000003) << 24) |
						((i_string[1] & 0x0000003F) << 18) |
						((i_string[2] & 0x0000003F) << 12) |
						((i_string[3] & 0x0000003F) << 6) |
						(i_string[4] & 0x0000003F);
			if (val < 0x00200000)
				val = '?';
			clen = 5;
		}
		else {
			val = '?';
			clen = s_len;
		}
	}
	else if ((ch & 0xFE) == 0xFC) {
		if (s_len > 5) {
			val = ((i_string[0] & 0x00000001) << 30) |
						((i_string[1] & 0x0000003F) << 24) |
						((i_string[2] & 0x0000003F) << 18) |
						((i_string[3] & 0x0000003F) << 12) |
						((i_string[4] & 0x0000003F) << 6) |
						(i_string[5] & 0x0000003F);
			if (val < 0x04000000)
				val = '?';
			clen = 6;
		}
		else {
			val = '?';
			clen = s_len;
		}
	}
	else {
		// Should not happen!
		val = '?';
		clen = 1;
	}
	*ret_value = val;
	return(clen);
}

void cs_utf8_to_uni(size_t out_len, unichar *out_string, const u_char *in_string)
{
	uint32_t	utf_value;

	out_len--;  // Space for zero terminator
	while (*in_string) {
		in_string += cs_utf_to_uni_char(in_string, &utf_value);
		if (out_len == 0)
			break;
		if (utf_value > 0x0000FFFF)
			*out_string = (unichar) '?';
		else
			*out_string = (unichar) utf_value;
		out_string++;
		out_len--;
	}
	*out_string = 0;
}

void cs_utf8_to_uni_no_term(size_t out_len, unichar *out_string, const u_char *in_string)
{
	uint32_t	utf_value;

	while (*in_string) {
		in_string += cs_utf_to_uni_char(in_string, &utf_value);
		if (out_len == 0)
			break;
		if (utf_value > 0x0000FFFF)
			*out_string = (unichar) '?';
		else
			*out_string = (unichar) utf_value;
		out_string++;
		out_len--;
	}
}

void cs_uni_to_utf8(size_t out_len, char *out_string, const unichar *in_string)
{
	out_len--;  // Space for zero terminator
	while (*in_string) {
		if (*in_string <= 0x007F) {
			if (out_len < 1)
				break;
			*out_string++ = (char) (u_char) *in_string;
			out_len--;
		}
		else if (*in_string <= 0x07FF) {
			if (out_len < 3)
				break;
			*out_string++ = (char) (u_char) ((0x00C0) | ((*in_string >> 6) & 0x001F));
			*out_string++ = (char) (u_char) ((0x0080) | (*in_string & 0x003F));
			out_len -= 2;
		}
		else /* <= 0xFFFF */ {
			if (out_len < 3)
				break;
			*out_string++ = (char) (u_char) ((0x00E0) | ((*in_string >> 12) & 0x000F));
			*out_string++ = (char) (u_char) ((0x0080) | ((*in_string >> 6) & 0x003F));
			*out_string++ = (char) (u_char) ((0x0080) | (*in_string & 0x003F));
			out_len -= 3;
		}
		in_string++;
	}
	*out_string = 0;
}

void cs_uni_to_utf8(size_t out_len, char *out_string, const unichar *in_string, s_int in_len)
{
	out_len--;  // Space for zero terminator
	while (in_len--) {
		if (*in_string <= 0x007F) {
			if (out_len < 1)
				break;
			*out_string++ = (char) (u_char) *in_string;
			out_len--;
		}
		else if (*in_string <= 0x07FF) {
			if (out_len < 3)
				break;
			*out_string++ = (char) (u_char) ((0x00C0) | ((*in_string >> 6) & 0x001F));
			*out_string++ = (char) (u_char) ((0x0080) | (*in_string & 0x003F));
			out_len -= 2;
		}
		else /* <= 0xFFFF */ {
			if (out_len < 3)
				break;
			*out_string++ = (char) (u_char) ((0x00E0) | ((*in_string >> 12) & 0x000F));
			*out_string++ = (char) (u_char) ((0x0080) | ((*in_string >> 6) & 0x003F));
			*out_string++ = (char) (u_char) ((0x0080) | (*in_string & 0x003F));
			out_len -= 3;
		}
		in_string++;
	}
	*out_string = 0;
}

size_t cs_utf8_to_uni_len(const char *in_string)
{
	size_t slen = 0;

	while (*in_string) {
		if ((*((u_char *) in_string) & 0xC0) == 0x80)
			// These are char data bytes (10xxxxxx)
			;
		else
			// These are single char (00xxxxx, 01xxxxx), or char start bytes (11xxxxxx)
			slen++;
		in_string++;
	}
	return slen;
}

size_t cs_uni_to_utf8_len(const unichar *in_string, s_int in_len)
{
	size_t slen = 0;

	while (in_len--) {
		if (*in_string <= 0x000007F) {
			slen++;
		}
		else if (*in_string <= 0x00007FF)
			slen += 2;
		else /* <= 0xFFFF */
			slen += 3;
		in_string++;
	}
	return slen;
}

/*
size_t cs_uni_len(const unichar *in_string)
{
	size_t len = 0;
	
	while (*in_string++) len++;
	return len;
}
*/

