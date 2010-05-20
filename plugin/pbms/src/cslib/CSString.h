/* Copyright (c) 2008 PrimeBase Technologies GmbH, Germany
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
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 *
 * Original author: Paul McCullagh (H&G2JCtL)
 * Continued development: Barry Leslie
 *
 * 2007-06-15
 *
 * CORE SYSTEM
 * This class encapsulates a basic string.
 *
 */

#ifndef __CSSTRING_H__
#define __CSSTRING_H__
#include <string.h>

#include "CSDefs.h"
#include "CSObject.h"

#ifdef OS_MACINTOSH
#include <CoreFoundation/CFString.h>
#endif


class CSString;

/*
 * A unsigned 16-bit unicode character:
 */
#define unichar			uint16_t

// CSStringBuffer_ is just a base class to define
// referenced and non referenced versions on.
class CSStringBuffer_ {
public:
	CSStringBuffer_();
	CSStringBuffer_(u_int grow);
	~CSStringBuffer_();

	void clear();

	void append(char ch);

	void append(const char *str, size_t len);

	void append(const char *str) {append(str, strlen(str));}

	void append(int value);

	void append(uint32_t value);

	char *getCString();

	char *getBuffer(u_int pos) { return iBuffer ? iBuffer + pos : NULL; }

	char *take();
	
	void setLength(u_int len);

	void setGrowSize(u_int size) { iGrow = size; }

	u_int length() { return myStrLen; }

	u_int ignore(u_int pos, char ch);

	u_int find(u_int pos, char ch);

	u_int trim(u_int pos, char ch);

	CSString *substr(u_int pos, u_int len);

private:
	char *iBuffer;
	u_int iGrow;
	u_int iSize;
	u_int myStrLen;
};

class CSStringBuffer : public CSStringBuffer_, public CSObject {
public:
	CSStringBuffer(): CSStringBuffer_(){ }
	CSStringBuffer(u_int grow): CSStringBuffer_(grow){ }
};

class CSRefStringBuffer : public CSStringBuffer_, public CSRefObject {
public:
	CSRefStringBuffer(): CSStringBuffer_(){ }
	CSRefStringBuffer(u_int grow): CSStringBuffer_(grow){ }
};

class CSSyncStringBuffer : public CSStringBuffer, public CSSync {
public:
	CSSyncStringBuffer(u_int growSize): CSStringBuffer(growSize), CSSync() { }
	CSSyncStringBuffer(): CSStringBuffer(), CSSync() { }
};

#define CS_CHAR		int

class CSString : public CSRefObject {
public:
	CSString() { }
	virtual ~CSString() { }

	/*
	 * Construct a string from a C-style UTF-8
	 * string.
	 */
	static CSString *newString(const char *cstr);

	/* Construct a string from a UTF-8 byte array: */
	static CSString *newString(const char *bytes, u_int len);

	/* Construct a string from string buffer: */
	static CSString *newString(CSStringBuffer *sb);

	/*
	 * Returns a pointer to a UTF-8 string.
	 * The returned string must be
	 * not be freed by the caller.
	 */
	virtual const char *getCString() = 0;

	/*
	 * Return the character at a certain point:
	 */
	virtual CS_CHAR charAt(u_int pos) = 0;
	virtual CS_CHAR upperCharAt(u_int pos) = 0;
	virtual void setCharAt(u_int pos, CS_CHAR ch) = 0;

	/*
	 * Returns < 0 if this string is
	 * sorted before val, 0 if equal,
	 * > 0 if sortede after.
	 * The comparison is case-insensitive.
	 */
	virtual int compare(CSString *val) = 0;
	virtual int compare(const char *val, u_int len = ((u_int) 0xFFFFFFFF)) = 0;

	/*
	 * Case sensitive match.
	 */
	virtual bool startsWith(u_int index, const char *) = 0;

	/* Returns the string length in characters. */
	virtual u_int length() = 0;
	virtual void setLength(u_int len) = 0;

	virtual bool equals(const char *str);

	/*
	 * Return a copy of this string.
	 */
	virtual CSString *clone(u_int pos, u_int len) = 0;

	/*
	 * Concatinate 2 strings.
	 */
	virtual CSString *concat(CSString *str);
	virtual CSString *concat(const char *str);

	/* Return an upper case version of the string: */
	virtual CSString *toUpper();

	/*
	 * Returns a case-insensitive has
	 * value.
	 */
	virtual u_int hashKey();

	/*
	 * Locate the count'th occurance of the given
	 * string, moving from left to right or right to
	 * left if count is negative.
	 *
	 * The index of the first character is zero.
	 * If not found, then index returned depends
	 * on the search direction.
	 *
	 * Search from left to right will return
	 * the length of the string, and search
	 * from right to left will return 0.
	 */
	virtual u_int locate(const char *, s_int count);
	virtual u_int locate(u_int pos, const char *);
	virtual u_int locate(u_int pos, CS_CHAR ch);

	virtual u_int skip(u_int pos, CS_CHAR ch);

	virtual CSString *substr(u_int index, u_int size);
	virtual CSString *substr(u_int index);

	virtual CSString *left(const char *, s_int count);
	virtual CSString *left(const char *);

	virtual CSString *right(const char *, s_int count);
	virtual CSString *right(const char *);

	virtual bool startsWith(const char *);
	virtual bool endsWith(const char *);

	/* Return the next position in the string, but do
	 * not go past the length of the string.
	 */
	virtual u_int nextPos(u_int pos);

	virtual CSString *clone(u_int len);
	virtual CSString *clone();
};

class CSCString : public CSString {
public:
	char *myCString;
	u_int myStrLen;

	CSCString();
	~CSCString();


	virtual const char *getCString();

	virtual CS_CHAR charAt(u_int pos);

	virtual CS_CHAR upperCharAt(u_int pos);

	virtual void setCharAt(u_int pos, CS_CHAR ch);

	virtual int compare(CSString *val);
	virtual int compare(const char *val, u_int len = ((u_int) 0xFFFFFFFF));

	virtual bool startsWith(u_int index, const char *);

	virtual u_int length() { return myStrLen; }

	virtual void setLength(u_int len);

	virtual CSString *clone(u_int pos, u_int len);

	virtual CSObject *getKey();

	virtual int compareKey(CSObject *);

	static CSString *newString(const char *cstr);

	static CSString *newString(const char *bytes, u_int len);

	static CSString *newString(CSStringBuffer *sb);
};

#endif

