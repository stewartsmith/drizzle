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
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA
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
	CSStringBuffer_(uint32_t grow);
	~CSStringBuffer_();

	void clear();

	void append(char ch);

	void append(const char *str, size_t len);

	void append(const char *str) {append(str, strlen(str));}

	void append(int value);

	void append(uint32_t value);

	char *getCString();

	char *getBuffer(uint32_t pos) { return iBuffer ? iBuffer + pos : NULL; }

	char *take();
	
	void setLength(uint32_t len);

	void setGrowSize(uint32_t size) { iGrow = size; }

	uint32_t length() { return myStrLen; }

	uint32_t ignore(uint32_t pos, char ch);

	uint32_t find(uint32_t pos, char ch);

	uint32_t trim(uint32_t pos, char ch);

	CSString *substr(uint32_t pos, uint32_t len);

private:
	char *iBuffer;
	uint32_t iGrow;
	uint32_t iSize;
	uint32_t myStrLen;
};

class CSStringBuffer : public CSStringBuffer_, public CSObject {
public:
	CSStringBuffer(): CSStringBuffer_(){ }
	CSStringBuffer(uint32_t grow): CSStringBuffer_(grow){ }
};

class CSRefStringBuffer : public CSStringBuffer_, public CSRefObject {
public:
	CSRefStringBuffer(): CSStringBuffer_(){ }
	CSRefStringBuffer(uint32_t grow): CSStringBuffer_(grow){ }
};

class CSSyncStringBuffer : public CSStringBuffer, public CSSync {
public:
	CSSyncStringBuffer(uint32_t growSize): CSStringBuffer(growSize), CSSync() { }
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
	static CSString *newString(const char *bytes, uint32_t len);

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
	virtual CS_CHAR charAt(uint32_t pos) = 0;
	virtual CS_CHAR upperCharAt(uint32_t pos) = 0;
	virtual void setCharAt(uint32_t pos, CS_CHAR ch) = 0;

	/*
	 * Returns < 0 if this string is
	 * sorted before val, 0 if equal,
	 * > 0 if sortede after.
	 * The comparison is case-insensitive.
	 */
	virtual int compare(CSString *val) = 0;
	virtual int compare(const char *val, uint32_t len = ((uint32_t) 0xFFFFFFFF)) = 0;

	/*
	 * Case sensitive match.
	 */
	virtual bool startsWith(uint32_t index, const char *) = 0;

	/* Returns the string length in characters. */
	virtual uint32_t length() = 0;
	virtual void setLength(uint32_t len) = 0;

	virtual bool equals(const char *str);

	/*
	 * Return a copy of this string.
	 */
	virtual CSString *clone(uint32_t pos, uint32_t len) = 0;

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
	virtual uint32_t hashKey();

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
	virtual uint32_t locate(const char *, s_int count);
	virtual uint32_t locate(uint32_t pos, const char *);
	virtual uint32_t locate(uint32_t pos, CS_CHAR ch);

	virtual uint32_t skip(uint32_t pos, CS_CHAR ch);

	virtual CSString *substr(uint32_t index, uint32_t size);
	virtual CSString *substr(uint32_t index);

	virtual CSString *left(const char *, s_int count);
	virtual CSString *left(const char *);

	virtual CSString *right(const char *, s_int count);
	virtual CSString *right(const char *);

	virtual bool startsWith(const char *);
	virtual bool endsWith(const char *);

	/* Return the next position in the string, but do
	 * not go past the length of the string.
	 */
	virtual uint32_t nextPos(uint32_t pos);

	virtual CSString *clone(uint32_t len);
	virtual CSString *clone();
};

class CSCString : public CSString {
public:
	char *myCString;
	uint32_t myStrLen;

	CSCString();
	~CSCString();


	virtual const char *getCString();

	virtual CS_CHAR charAt(uint32_t pos);

	virtual CS_CHAR upperCharAt(uint32_t pos);

	virtual void setCharAt(uint32_t pos, CS_CHAR ch);

	virtual int compare(CSString *val);
	virtual int compare(const char *val, uint32_t len = ((uint32_t) 0xFFFFFFFF));

	virtual bool startsWith(uint32_t index, const char *);
	virtual bool startsWith(const char *str) { return CSString::startsWith(str);}

	virtual uint32_t length() { return myStrLen; }

	virtual void setLength(uint32_t len);

	virtual CSString *clone(uint32_t pos, uint32_t len);
	virtual CSString *clone(uint32_t len){ return CSString::clone(len);}
	virtual CSString *clone(){ return CSString::clone();}

	virtual CSObject *getKey();

	virtual int compareKey(CSObject *);

	static CSCString *newString(const char *cstr);

	static CSCString *newString(const char *bytes, uint32_t len);

	static CSCString *newString(CSStringBuffer *sb);
};

#endif

