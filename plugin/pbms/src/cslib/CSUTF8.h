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

#pragma once
#ifndef __CSUTF8_H__
#define __CSUTF8_H__

#include "CSDefs.h"
#include "CSString.h"

/*
 * Convert a UTF-8 string to a unicode value.
 * This function returns the length of the unicode character.
 */
size_t cs_utf_to_uni_char(const u_char *in_string, uint32_t *uni_value);

void cs_utf8_to_uni(size_t out_len, unichar *out_string, const u_char *in_string);

/* Convert to UTF-8 without a terminator: */
void cs_utf8_to_uni_no_term(size_t out_len, unichar *out_string, const u_char *in_string);

void cs_uni_to_utf8(size_t out_len, char *out_string, const unichar *in_string);
void cs_uni_to_utf8(size_t out_len, char *out_string, const unichar *in_string, s_int in_len);

size_t cs_utf8_to_uni_len(const char *in_string);
size_t cs_uni_to_utf8_len(const unichar *in_string, s_int in_len);

#endif


