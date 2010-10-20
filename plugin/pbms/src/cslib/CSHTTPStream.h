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
 * 2007-06-10
 *
 *
 */

#ifndef __CSHTTPSTREAM_H__
#define __CSHTTPSTREAM_H__

#include "CSDefs.h"
#include "CSStorage.h"
#include "CSStream.h"

// The http tags before and after the exception message text must be well defined so that
// the client api can parse the error reply and get the error text out.
#define EXCEPTION_REPLY_MESSAGE_PREFIX_TAG "<P><B>"
#define EXCEPTION_REPLY_MESSAGE_SUFFIX_TAG "</B></P><PRE>"
#define EXCEPTION_REPLY_STACK_TRACE_SUFFIX_TAG "</PRE><P><font size=-1>"

class CSHeader : public CSRefObject {
public:
	CSHeader():iName(NULL), iValue(NULL) { }
	virtual ~CSHeader();

	void setName(const char *name);
	void setName(const char *name, uint32_t len);
	void setName(CSString *name);

	void setValue(const char *value);
	void setValue(const char *value, uint32_t len);
	void setValue(CSString *value);

	const char *getNameCString() { return iName ? iName->getCString() : ""; }
	const char *getValueCString() { return iValue ? iValue->getCString() : ""; }

	void write(CSOutputStream *out);
	
	friend class CSHTTPHeaders;
private:
	CSString *getName() { return iName; } // Return a none referenced object!!
	CSString *getValue() { return iValue; }// Return a none referenced object!!

	CSString	*iName;
	CSString	*iValue;
};

class CSHTTPHeaders {
public:
	CSHTTPHeaders(): iHeaders(NULL), iKeepAlive(false), iExpect100Continue(false), iUnknownEpectHeader(false) { }
	virtual ~CSHTTPHeaders() { if (iHeaders) iHeaders->release();}

	void clearHeaders();
	CSVector * takeHeaders();
	void setHeaders(CSVector *headers);
	void addHeaders(CSHTTPHeaders *h);
	void addHeader(CSHeader *h);
	void addHeader(const char *name, const char *value);
	void addHeader(const char *name, uint32_t nlen, const char *value, uint32_t vlen);
	void addHeader(CSString *name, CSString *value);
	void addHeader(const char *name, CSString *value);
	void removeHeader(const char *name);
	void removeHeader(CSString *name);
	CSString *getHeaderValue(const char *name);
	void writeHeader(CSOutputStream *out);
	bool keepAlive();
	bool expect100Continue();
	bool unknownEpectHeader();

	uint32_t numHeaders() { return (iHeaders)?iHeaders->size(): 0; }
	CSHeader *getHeader(uint32_t idx) 
	{ 
		CSHeader *header = NULL;
		
		if (iHeaders) 
			header = (CSHeader *)(iHeaders->get(idx));
			
		if (header)
			header->retain();
		return header;
	}
	
private:
	CSVector *iHeaders;
	bool iKeepAlive;
	bool iExpect100Continue;
	bool iUnknownEpectHeader;
};

class CSHTTPInputStream : public CSInputStream, public CSHTTPHeaders {
public:
	CSHTTPInputStream(CSInputStream *s);
	virtual ~CSHTTPInputStream();

	void readHead();
	uint64_t getContentLength();
	const char *getMethod();
	int getStatus() { return iStatus; }
	CSString *getStatusPhrase() { return iStatusPhrase; }
	CSString *getRequestURI() { return iRequestURI; }
	bool getRange(uint64_t *size, uint64_t *offset);

	virtual void close();

	virtual size_t read(char *b, size_t len);

	virtual int read();

	virtual int peek();

	virtual void reset() {iInput->reset();}

	static CSHTTPInputStream *newStream(CSInputStream* i);

private:
	void freeHead();

	// Request-Line   = Method SP Request-URI SP HTTP-Version CRLF
	CSInputStream *iInput;
	int		iStatus;
	CSString *iMethod;
	CSString *iRequestURI;
	CSString *iHTTPVersion;
	CSString *iStatusPhrase;
};

class CSHTTPOutputStream : public CSOutputStream, public CSHTTPHeaders {
public:
	CSHTTPOutputStream(CSOutputStream* s);
	virtual ~CSHTTPOutputStream();

	void setStatus(int status) { iStatus = status; }
	int getStatus() { return iStatus; }
	void setContentLength(uint64_t size) { iContentLength = size; }
	void setRange(uint64_t size, uint64_t offset, uint64_t total) { iRangeSize = size; iRangeOffset = offset; iTotalLength = total;}

	void writeHead();	// Writes a standard HTTP header.
	void writeHeaders(); // Write the current headers.

	void clearBody();
	void writeBody();

	void appendBody(const char *str);
	void appendBody(int value);

	virtual void close();

	virtual void write(const char *b, size_t len);

	virtual const char *getEOL() { return "\r\n"; };

	virtual void flush();

	virtual void write(char b);

	virtual void reset() {iOutput->reset();}

	static const char *getReasonPhrase(int code);

	static CSHTTPOutputStream *newStream(CSOutputStream* i);

private:
	// Status-Line = HTTP-Version SP Status-Code SP Reason-Phrase CRLF
	CSOutputStream	*iOutput;
	int				iStatus;
	uint64_t			iContentLength;
	CSStringBuffer	iBody;
	uint64_t			iRangeSize;
	uint64_t			iRangeOffset;
	uint64_t			iTotalLength;
};

#endif
