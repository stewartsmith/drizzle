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
 * 2007-06-15
 *
 * CORE SYSTEM
 * This class encapsulates a basic string.
 *
 */

#pragma once
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

/* CSStringBufferImpl is the string buffer implementation.
 * Of this implementation we have various types.
 */
class CSStringBufferImpl {
public:
	CSStringBufferImpl();
	CSStringBufferImpl(uint32_t grow);
	~CSStringBufferImpl();

	void clear();

	void append(char ch);

	void append(const char *str, size_t len);

	void append(const char *str) {append(str, strlen(str));}

	void append(int value);

	void append(uint32_t value);

	void append(uint64_t value);

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

	void take(CSStringBufferImpl *buf) {
		clear();
		iGrow = buf->iGrow;
		iSize = buf->iSize;
		myStrLen = buf->myStrLen;
		iBuffer = buf->take();
	}

private:
	char *iBuffer;
	uint32_t iGrow;
	uint32_t iSize;
	uint32_t myStrLen;
};

class CSStringBuffer : public CSStringBufferImpl, public CSObject {
public:
	CSStringBuffer(): CSStringBufferImpl(), CSObject() { }
	CSStringBuffer(uint32_t grow): CSStringBufferImpl(grow), CSObject() { }
};

class CSStaticStringBuffer : public CSStringBufferImpl, public CSStaticObject {
	virtual void finalize() { clear(); }

public:
	CSStaticStringBuffer(): CSStringBufferImpl(), CSStaticObject() { }
	CSStaticStringBuffer(uint32_t grow): CSStringBufferImpl(grow), CSStaticObject() { }
};

class CSRefStringBuffer : public CSStringBufferImpl, public CSRefObject {
public:
	CSRefStringBuffer(): CSStringBufferImpl(), CSRefObject() { }
	CSRefStringBuffer(uint32_t grow): CSStringBufferImpl(grow), CSRefObject() { }
};

class CSSyncStringBuffer : public CSStringBuffer, public CSSync {
public:
	CSSyncStringBuffer(uint32_t growSize): CSStringBuffer(growSize), CSSync() { }
	CSSyncStringBuffer(): CSStringBuffer(), CSSync() { }
};

#define CS_CHAR		int

class CSString : public CSRefObject {
public:
	CSString();
	CSString(const char *cstr);
	virtual ~CSString();

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
	virtual const char *getCString();

	/*
	 * Return the character at a certain point:
	 */
	virtual CS_CHAR charAt(uint32_t pos);
	virtual CS_CHAR upperCharAt(uint32_t pos);
	virtual void setCharAt(uint32_t pos, CS_CHAR ch);

	/*
	 * Returns < 0 if this string is
	 * sorted before val, 0 if equal,
	 * > 0 if sortede after.
	 * The comparison is case-insensitive.
	 */
	virtual int compare(CSString *val);
	virtual int compare(const char *val, uint32_t len = ((uint32_t) 0xFFFFFFFF));

	/*
	 * Case sensitive match.
	 */
	virtual bool startsWith(uint32_t index, const char *);

	/* Returns the string length in characters. */
	virtual uint32_t length() { return myStrLen; }
	virtual void setLength(uint32_t len);

	virtual bool equals(const char *str);

	/*
	 * Return a copy of this string.
	 */
	virtual CSString *clone(uint32_t pos, uint32_t len);

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
	virtual uint32_t locate(const char *, int32_t count);
	virtual uint32_t locate(uint32_t pos, const char *);
	virtual uint32_t locate(uint32_t pos, CS_CHAR ch);

	virtual uint32_t skip(uint32_t pos, CS_CHAR ch);

	virtual CSString *substr(uint32_t index, uint32_t size);
	virtual CSString *substr(uint32_t index);

	virtual CSString *left(const char *, int32_t count);
	virtual CSString *left(const char *);

	virtual CSString *right(const char *, int32_t count);
	virtual CSString *right(const char *);

	virtual bool startsWith(const char *);
	virtual bool endsWith(const char *);

	/* Return the next position in the string, but do
	 * not go past the length of the string.
	 */
	virtual uint32_t nextPos(uint32_t pos);

	virtual CSString *clone(uint32_t len);
	virtual CSString *clone();

	virtual CSObject *getKey();

	virtual int compareKey(CSObject *);

private:
	char *myCString;
	uint32_t myStrLen;
};

#endif

