/*
  Copyright (C) 1999, 2002 Aladdin Enterprises.  All rights reserved.

  This software is provided 'as-is', without any express or implied
  warranty.  In no event will the authors be held liable for any damages
  arising from the use of this software.

  Permission is granted to anyone to use this software for any purpose,
  including commercial applications, and to alter it and redistribute it
  freely, subject to the following restrictions:

  1. The origin of this software must not be misrepresented; you must not
     claim that you wrote the original software. If you use this software
     in a product, an acknowledgment in the product documentation would be
     appreciated but is not required.
  2. Altered source versions must be plainly marked as such, and must not be
     misrepresented as being the original software.
  3. This notice may not be removed or altered from any source distribution.

  L. Peter Deutsch
  ghost@aladdin.com

 */
/* $Id: md5.h,v 1.4 2002/04/13 19:20:28 lpd Exp $ */
/*
  Independent implementation of MD5 (RFC 1321).

  This code implements the MD5 Algorithm defined in RFC 1321, whose
  text is available at
	http://www.ietf.org/rfc/rfc1321.txt
  The code is derived from the text of the RFC, including the test suite
  (section A.5) but excluding the rest of Appendix A.  It does not include
  any code or documentation that is identified in the RFC as being
  copyrighted.

  The original and principal author of md5.h is L. Peter Deutsch
  <ghost@aladdin.com>.  Other authors are noted in the change history
  that follows (in reverse chronological order):

  2002-04-13 lpd Removed support for non-ANSI compilers; removed
	references to Ghostscript; clarified derivation from RFC 1321;
	now handles byte order either statically or dynamically.
  1999-11-04 lpd Edited comments slightly for automatic TOC extraction.
  1999-10-18 lpd Fixed typo in header comment (ansi2knr rather than md5);
	added conditionalization for C++ compilation from Martin
	Purschke <purschke@bnl.gov>.
  1999-05-03 lpd Original version.
 */

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
 *  Modified by Barry Leslie on 10/17/08.
 *	I have put a C++ wrapper around the data and functions to create an Md5 class.
 *
 */
#pragma once
#ifndef md5_INCLUDED
#define md5_INCLUDED

#include <string.h>

#include "CSDefs.h"

/* Define the state of the MD5 Algorithm. */
typedef unsigned int md5_word_t;

#define MD5_CHECKSUM_SIZE			CHECKSUM_VALUE_SIZE
#define MD5_CHECKSUM_STRING_SIZE	(CHECKSUM_VALUE_SIZE * 2 + 1)

class CSMd5 {
	private:
	struct md5_state_s {
		md5_word_t	count[2];		/* message length in bits, lsw first */
		md5_word_t	abcd[4];		/* digest buffer */
		u_char		buf[64];		/* accumulate block */
	} md5_state;

	u_char			digest[MD5_CHECKSUM_SIZE];
	char			digest_cstr[MD5_CHECKSUM_STRING_SIZE];

	void md5_process(const u_char *data /*[64]*/);
	void md5_digest();

	public:
	CSMd5() {
		md5_init();
	}

	void md5_init();

	/* Append a string to the message. */
	void md5_append(const u_char  *data, int nbytes);

	void md5_append(const char  *data) {
		md5_append((const u_char  *) data, strlen(data));
	}

	/* Finish the message and return the digest. */
	
	// Returns the raw bin digest.
	const u_char *md5_get_digest() {
		if (!digest_cstr[0])
			md5_digest();
		return digest;
	}

	// Returns the bin/hex digest.
	void md5_get_digest(Md5Digest *d) {
		if (!digest_cstr[0])
			md5_digest();
			
		memcpy(d->val, digest, CHECKSUM_VALUE_SIZE);
	}
	
	// Returns the bin/hex digest.
	const char *md5_get_digest_cstr() {
		if (!digest_cstr[0])
			md5_digest();
		return digest_cstr;
	}
	
};

#endif /* md5_INCLUDED */
