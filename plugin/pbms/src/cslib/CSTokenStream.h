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
 * 2007-06-08
 *
 * CORE SYSTEM:
 * A token stream. The tokens are identified in the same
 * manner as the UNIX/DOS command line.
 *
 * This basically means that all tokens must be sepparated
 * by at least one space.
 *
 * The following punctuation are considered tokens or
 * have special meaning:
 *
 * =[]!#$%&*()"|;'\<>?/~`
 *
 * EOL and EOF are also tokens.
 *
 * If they appear in words they must be escaped using \,
 * or the word must be place in quotes " or '.
 *
 * The following punctuation can be part of words:
 *
 * -{}@^_+:,.
 *
 * Words next to each other, without a token or space
 * between are considered one word, e.g.:
 *
 * "hello""world" == helloworld
 *
 * Quotes in a quoted string are not allowed, e.g., to
 * write "it's mine" in single quotes it must be written
 * as: 'it'\''s mine'
 *
 * Characters can be encoded in octal form as follows \xxx where
 * 0 <= x <= 7
 *
 */

#ifndef __CSTOKENSTREAM_H__
#define __CSTOKENSTREAM_H__

#include <stdlib.h>
#include <string.h>

#include "CSDefs.h"
#include "CSStream.h"
#include "CSStorage.h"
#include "CSPath.h"
#include "CSString.h"

using namespace std;

#define CS_TKN_EQ		0		// =
#define CS_TKN_OSQR		1		// [
#define CS_TKN_CSQR		2		// ]
#define CS_TKN_BANG		3		// !
#define CS_TKN_HASH		4		// #
#define CS_TKN_DOLLAR	5		// $
#define CS_TKN_PERC		6		// %
#define CS_TKN_AMPER	7		// &
#define CS_TKN_STAR		8		// *
#define CS_TKN_ORND		9		// (
#define CS_TKN_CRND		10		// )
#define CS_TKN_PIPE		11		// |
#define CS_TKN_SEMIC	12		// ;
#define CS_TKN_LT		13		// <
#define CS_TKN_GT		14		// >
#define CS_TKN_QMARK	15		// ?
#define CS_TKN_TILDA	16		// ~
#define CS_TKN_EOS		17
#define CS_TKN_EOL		18
#define CS_TKN_TEXT		19

class CSToken : public CSObject {
public:
	/* Note this order matches the token numbers above!! */
	static const char *singletons;

	CSToken(int type) { iType = type; }
	virtual ~CSToken() { }

	/*
	 * Returns true if this token is the
	 * EOS token.
	 */
	bool isEOF() const { return iType == CS_TKN_EOS; }
	
	/*
	 * Returns true if this token is the
	 * EOL token.
	 */
	bool isEOL() const { return iType == CS_TKN_EOL; }

	/*
	 * Return the text of a token as a printable string.
	 */
	virtual CSString *getString() = 0;

	virtual const char *getCString() = 0;

	virtual int getInteger() = 0;

	/*
	 * Return true of the token matches the given text.
	 */
	virtual bool isEqual(const char *text) = 0;

	/*
	 * Return the text of the token as a file system
	 * path.
	 */
	//virtual CSPath *getPath() = 0;

private:
	int iType;
};

class UXToken : public CSToken {
public:
	UXToken(int type, const char *text): CSToken(type) { iText.append(text); }
	UXToken(int type, char ch): CSToken(type) { iText.append(ch); }
	virtual ~UXToken() { }

	virtual CSString *getString() { return CSString::newString(iText.getCString()); }

	virtual const char *getCString() { return iText.getCString(); }

	virtual int getInteger() { return atoi(iText.getCString()); }

	//virtual CSPath *getPath();

	virtual bool isEqual(const char *text) { return(strcmp(iText.getCString(), text) == 0); };

private:
	CSStringBuffer iText;
};

class CSTokenList : public CSVector {
public:
	CSTokenList():CSVector(2) { }
	virtual ~CSTokenList() { }

	CSToken *takeFront() { return (CSToken *) take(0); }

	CSToken *getAt(u_int idx) { return (CSToken *) get(idx); }

	CSToken *getFront() { return (CSToken *) get(0); }
};

class CSTokenStream : public CSObject {
public:
	CSTokenStream(): iStream(NULL), iLine(0), iChar(-2) { }

	virtual ~CSTokenStream();

	/*
	 * Open assumes the given stream is already referenced!
	 *
	 * When an error occurs an line number will be given, unless the
	 * line number is set to 0.
	 *
	 * If set to zero, the line number will also not be incremented
	 * when a new line is encountered.
	 */
	virtual void open(CSInputStream *stream, u_int line);

	virtual void close();
	
	virtual void nextChar();

	virtual CSToken *nextToken() { return NULL; }

	friend class UXTokenStream;

	static CSTokenStream *newTokenStream(CSInputStream *stream, u_int line);

private:
	CSInputStream *iStream;
	
	u_int iLine;

	int iChar;
};

class UXTokenStream : public CSTokenStream {
public:
	UXTokenStream(): CSTokenStream() { }

	virtual ~UXTokenStream() { }

	virtual CSToken *nextToken();

	virtual CSToken *newToken(int type, const char *text);

	virtual CSToken *newToken(int type, char ch);

	static CSTokenStream *newTokenStream(CSInputStream *stream, u_int line);
private:
};

#endif
