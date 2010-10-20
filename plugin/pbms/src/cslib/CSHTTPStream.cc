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
 * A basic syncronized object.
 *
 */
#include "CSConfig.h"

#include <stdlib.h>
#include <inttypes.h>

#include "string.h"

#include "CSHTTPStream.h"
#include "CSGlobal.h"

#ifdef DEBUG
//#define PRINT_HEADER
#endif

/*
 * ---------------------------------------------------------------
 * HTTP HEADERS
 */

CSHeader::~CSHeader()
{
	if (iName) {
		iName->release();
		iName = NULL;
	}
	if (iValue) {
		iValue->release();
		iValue = NULL;
	}
}

void CSHeader::setName(const char *name)
{
	iName = CSString::newString(name);
}

void CSHeader::setName(const char *name, uint32_t len)
{
	iName = CSString::newString(name, len);
}

void CSHeader::setName(CSString *name)
{
	iName = name;
}

void CSHeader::setValue(const char *value)
{
	iValue = CSString::newString(value);
}

void CSHeader::setValue(const char *value, uint32_t len)
{
	iValue = CSString::newString(value, len);
}

void CSHeader::setValue(CSString *value)
{
	iValue = value;
}

void CSHeader::write(CSOutputStream *out)
{
	out->print(iName);
	out->print(": ");
	out->print(iValue);
	out->print("\r\n");
}

void CSHTTPHeaders::clearHeaders()
{
	iKeepAlive = false;
	iExpect100Continue = false;
	iUnknownEpectHeader = false;
	if (iHeaders) {
		iHeaders->release();
		iHeaders = NULL;
	}
}
CSVector *CSHTTPHeaders::takeHeaders()
{
	CSVector *headers = iHeaders;
	iHeaders = NULL;
	return headers;
}

void CSHTTPHeaders::setHeaders(CSVector *headers)
{
	if (iHeaders) 
		iHeaders->release();
	iHeaders = headers;
}

void CSHTTPHeaders::addHeader(CSHeader *h)
{
	if (!iHeaders)
		new_(iHeaders, CSVector(5));

	if (strcasecmp(h->getNameCString(), "Connection") == 0 && strcasecmp(h->getValueCString(), "Keep-Alive") == 0)
		iKeepAlive = true;
		
	if (strcasecmp(h->getNameCString(), "Expect") == 0) {
		if (strcasecmp(h->getValueCString(), "100-continue") == 0)
			iExpect100Continue = true;
		else
			iUnknownEpectHeader = true;
	}
		
	iHeaders->add(h);
}

void CSHTTPHeaders::addHeaders(CSHTTPHeaders *headers)
{
	CSHeader *h;
	uint32_t i =0;
	while ((h = headers->getHeader(i++))) {
		addHeader(h);
	}
}

void CSHTTPHeaders::addHeader(const char *name, const char *value)
{
	CSHeader *h;

	enter_();
	if (!iHeaders)
		new_(iHeaders, CSVector(5));

	new_(h, CSHeader());
	push_(h);
	h->setName(name);
	h->setValue(value);
	pop_(h);

	addHeader(h);
	exit_();
}

void CSHTTPHeaders::addHeader(const char *name, uint32_t nlen, const char *value, uint32_t vlen)
{
	CSHeader *h;

	enter_();
	if (!iHeaders)
		new_(iHeaders, CSVector(5));

	new_(h, CSHeader());
	push_(h);
	h->setName(name, nlen);
	h->setValue(value, vlen);
	pop_(h);
	addHeader(h);
	exit_();
}

void CSHTTPHeaders::addHeader(CSString *name, CSString *value)
{
	CSHeader *h;

	enter_();
	push_(name);
	push_(value);
	if (!iHeaders)
		new_(iHeaders, CSVector(5));

	new_(h, CSHeader());
	pop_(value);
	pop_(name);
	h->setName(name);
	h->setValue(value);
	addHeader(h);
	exit_();
}

void CSHTTPHeaders::addHeader(const char *name, CSString *value)
{
	CSHeader *h;
	CSString *n;

	enter_();
	push_(value);
	n = CSString::newString(name);
	push_(n);
	if (!iHeaders)
		new_(iHeaders, CSVector(5));
	new_(h, CSHeader());
	pop_(n);
	pop_(value);
	h->setName(n);
	h->setValue(value);
	addHeader(h);
	exit_();
}

void CSHTTPHeaders::removeHeader(CSString *name)
{
	enter_();
	push_(name);
	if (iHeaders) {
		CSHeader *h;

		for (uint32_t i=0; i<iHeaders->size(); ) {
			h = (CSHeader *) iHeaders->get(i);
			if (h->getName()->compare(name) == 0) {
				iHeaders->remove(i);
			} else 
				i++;
		}
	}
	release_(name);
	
	exit_();
}

void CSHTTPHeaders::removeHeader(const char *name)
{
	removeHeader(CSString::newString(name));
}

CSString *CSHTTPHeaders::getHeaderValue(const char *name)
{
	CSString *n;
	CSString *v;

	enter_();
	n = CSString::newString(name);
	push_(n);
	v = NULL;
	if (iHeaders) {
		CSHeader *h;

		for (uint32_t i=0; i<iHeaders->size(); i++) {
			h = (CSHeader *) iHeaders->get(i);
			if (h->getName()->compare(n) == 0) {
				v = h->getValue();
				v->retain();
				break;
			}
		}
	}
	release_(n);
	return_(v);
}

void CSHTTPHeaders::writeHeader(CSOutputStream *out)
{
	if (iHeaders) {
		CSHeader *h;

		for (uint32_t i=0; i<iHeaders->size(); i++) {
			h = (CSHeader *) iHeaders->get(i);
			h->write(out);
		}
	}
}

bool CSHTTPHeaders::keepAlive()
{
	return iKeepAlive;
}

bool CSHTTPHeaders::expect100Continue()
{
	return iExpect100Continue;
}

bool CSHTTPHeaders::unknownEpectHeader()
{
	return iUnknownEpectHeader;
}

/*
 * ---------------------------------------------------------------
 * HTTP INPUT STREAMS
 */

CSHTTPInputStream::CSHTTPInputStream(CSInputStream* in):
CSHTTPHeaders(),
iInput(NULL),
iMethod(NULL),
iRequestURI(NULL),
iHTTPVersion(NULL),
iStatusPhrase(NULL)
{
	iInput = in;
}

CSHTTPInputStream::~CSHTTPInputStream()
{
	freeHead();
	if (iInput)
		iInput->release();
}

void CSHTTPInputStream::readHead()
{
	CSStringBuffer	*sb = NULL;
	bool			first_line = true;
	uint32_t			start, end;

	enter_();
	freeHead();
	for (;;) {
		sb = iInput->readLine();
		if (!sb)
			break;
#ifdef PRINT_HEADER
		printf("HTTP: %s\n", sb->getCString());
#endif
		if (sb->length() == 0) {
			sb->release();
			break;
		}
		push_(sb);
		
		if (first_line) {
			CSString *str;
			start = sb->ignore(0, ' ');
			end = sb->find(start, ' ');
			str = sb->substr(start, end - start);
			if (str->startsWith("HTTP")) { // Reply header
				iMethod = NULL;
				iRequestURI = NULL;
				iHTTPVersion = str;
				start = sb->ignore(end, ' ');
				end = sb->find(start, ' ');
				if (start > end)
					CSException::throwException(CS_CONTEXT, CS_ERR_BAD_HTTP_HEADER, "Bad HTTP header");

				str = sb->substr(start, end - start);
				iStatus = atol(str->getCString());
				str->release();
				start = sb->ignore(end, ' ');
				end = sb->find(start, '\r');
				if (start > end)
					CSException::throwException(CS_CONTEXT, CS_ERR_BAD_HTTP_HEADER, "Bad HTTP header");
				iStatusPhrase = sb->substr(start, end - start);
			} else {
				iStatus = 0;
				iStatusPhrase = NULL;
				iMethod = str;
			start = sb->ignore(end, ' ');
			end = sb->find(start, ' ');
			if (start > end)
				CSException::throwException(CS_CONTEXT, CS_ERR_BAD_HTTP_HEADER, "Bad HTTP header");
			iRequestURI = sb->substr(start, end - start);
			start = sb->ignore(end, ' ');
			end = sb->find(start, ' ');
			if (start > end)
				CSException::throwException(CS_CONTEXT, CS_ERR_BAD_HTTP_HEADER, "Bad HTTP header");
			iHTTPVersion = sb->substr(start, end - start);
			} 				
			first_line = false;
		}
		else {
			uint32_t nstart, nend;
			uint32_t vstart, vend;

			nstart = sb->ignore(0, ' ');
			nend = sb->find(nstart, ':');

			vstart = sb->ignore(nend+1, ' ');
			vend = sb->find(vstart, '\r');

			nend = sb->trim(nend, ' ');
			vend = sb->trim(vend, ' ');
			
			if (vstart > vend)
				CSException::throwException(CS_CONTEXT, CS_ERR_BAD_HTTP_HEADER, "Bad HTTP header");
			addHeader(sb->getBuffer(nstart), nend-nstart, sb->getBuffer(vstart), vend-vstart);
		}

		release_(sb);
	}
	exit_();
}

bool CSHTTPInputStream::getRange(uint64_t *size, uint64_t *offset)
{
	CSString	*val;
	bool haveRange = false;

	if ((val = getHeaderValue("Range"))) {
		uint64_t		first_byte = 0, last_byte = 0;
		const char	*range = val->getCString();
		
		if (range && (val->compare("bytes=", 6) == 0)) {
			if ((sscanf(range + 6, "%"PRIu64"-%"PRIu64"", &first_byte, &last_byte) == 2) && (last_byte >= first_byte)) {
				*offset = (uint64_t) first_byte;
				*size =last_byte - first_byte + 1;
				haveRange = true;
			}	
		}
		val->release();
				
	}
	return haveRange;
}

uint64_t CSHTTPInputStream::getContentLength()
{
	CSString	*val;
	uint64_t		size = 0;

	if ((val = getHeaderValue("Content-Length"))) {
		const char	*len = val->getCString();

		if (len)  
			sscanf(len, "%"PRIu64"", &size);		
		val->release();
	}
	return size;
}

const char *CSHTTPInputStream::getMethod()
{
	if (!iMethod)
		return NULL;
	return iMethod->getCString();
}

void CSHTTPInputStream::close()
{
	enter_();
	iInput->close();
	exit_();
}

size_t CSHTTPInputStream::read(char *b, size_t len)
{
	enter_();
	return_(iInput->read(b, len));
}

int CSHTTPInputStream::read()
{
	enter_();
	return_(iInput->read());
}

int CSHTTPInputStream::peek()
{
	enter_();
	return_(iInput->peek());
}

void CSHTTPInputStream::freeHead()
{
	enter_();
	clearHeaders();
	if (iMethod) {
		iMethod->release();
		iMethod = NULL;
	}
	if (iRequestURI) {
		iRequestURI->release();
		iRequestURI = NULL;
	}
	if (iHTTPVersion) {
		iHTTPVersion->release();
		iHTTPVersion = NULL;
	}
	if (iStatusPhrase) {
		iStatusPhrase->release();
		iStatusPhrase = NULL;
	}
	iStatus = 0;
	exit_();
}

CSHTTPInputStream *CSHTTPInputStream::newStream(CSInputStream* i)
{
	CSHTTPInputStream *s;

	if (!(s = new CSHTTPInputStream(i))) {
		i->release();
		CSException::throwOSError(CS_CONTEXT, ENOMEM);
	}
	return s;
}

/*
 * ---------------------------------------------------------------
 * HTTP OUTPUT STREAMS
 */

CSHTTPOutputStream::CSHTTPOutputStream(CSOutputStream* out):
CSHTTPHeaders(),
iOutput(NULL),
iStatus(0),
iContentLength(0),
iRangeSize(0),
iRangeOffset(0),
iTotalLength(0)
{
	iOutput = out;
	iBody.setGrowSize(120);
}

CSHTTPOutputStream::~CSHTTPOutputStream()
{
	clearHeaders();
	clearBody();
	if (iOutput)
		iOutput->release();
}

void CSHTTPOutputStream::writeHeaders()
{
	writeHeader(this);
	clearHeaders();
}

void CSHTTPOutputStream::writeHead()
{
	iOutput->print("HTTP/1.1 ");
	iOutput->print(iStatus);
	iOutput->print(" ");
	iOutput->print(getReasonPhrase(iStatus));
	iOutput->print("\r\n");
	writeHeader(iOutput);
	iOutput->print("Content-Length: ");
	iOutput->print(iContentLength);
	iOutput->print("\r\n");
	if (iRangeSize && (iStatus == 200)) {
		iOutput->print("Content-Range: bytes ");
		iOutput->print(iRangeOffset);
		iOutput->print("-");
		iOutput->print(iRangeOffset + iRangeSize -1);
		iOutput->print("/");
		iOutput->print(iTotalLength);
		iOutput->print("\r\n");
	}
	iOutput->print("\r\n");
}

void CSHTTPOutputStream::clearBody()
{
	iRangeSize = 0;
	iRangeOffset = 0;
	iTotalLength = 0;
	iContentLength = 0;
	iBody.clear();
}

void CSHTTPOutputStream::writeBody()
{
	iOutput->write(iBody.getBuffer(0), iBody.length());
}

void CSHTTPOutputStream::appendBody(const char *str)
{
	iBody.append(str);
	iContentLength = iBody.length();
}

void CSHTTPOutputStream::appendBody(int value)
{
	iBody.append(value);
	iContentLength = iBody.length();
}

void CSHTTPOutputStream::close()
{
	enter_();
	iOutput->close();
	exit_();
}

void CSHTTPOutputStream::write(const char *b, size_t len)
{
	enter_();
	iOutput->write(b, len);
	exit_();
}

void CSHTTPOutputStream::flush()
{
	enter_();
	iOutput->flush();
	exit_();
}

void CSHTTPOutputStream::write(char b)
{
	enter_();
	iOutput->write(b);
	exit_();
}

const char *CSHTTPOutputStream::getReasonPhrase(int code)
{
	const char *message = "Unknown Code";

	switch (code) {
		case 100: message = "Continue"; break;
		case 101: message = "Switching Protocols"; break;
		case 200: message = "OK"; break;
		case 201: message = "Created"; break;
		case 202: message = "Accepted"; break;
		case 203: message = "Non-Authoritative Information"; break;
		case 204: message = "No Content"; break;
		case 205: message = "Reset Content"; break;
		case 206: message = "Partial Content"; break;
		case 300: message = "Multiple Choices"; break;
		case 301: message = "Moved Permanently"; break;
		case 302: message = "Found"; break;
		case 303: message = "See Other"; break;
		case 304: message = "Not Modified"; break;
		case 305: message = "Use Proxy"; break;
		case 307: message = "Temporary Redirect"; break;
		case 400: message = "Bad Request"; break;
		case 401: message = "Unauthorized"; break;
		case 402: message = "Payment Required"; break;
		case 403: message = "Forbidden"; break;
		case 404: message = "Not Found"; break;
		case 405: message = "Method Not Allowed"; break;
		case 406: message = "Not Acceptable"; break;
		case 407: message = "Proxy Authentication Required"; break;
		case 408: message = "Request Time-out"; break;
		case 409: message = "Conflict"; break;
		case 410: message = "Gone"; break;
		case 411: message = "Length Required"; break;
		case 412: message = "Precondition Failed"; break;
		case 413: message = "Request Entity Too Large"; break;
		case 414: message = "Request-URI Too Large"; break;
		case 415: message = "Unsupported Media Type"; break;
		case 416: message = "Requested range not satisfiable"; break;
		case 417: message = "Expectation Failed"; break;
		case 500: message = "Internal Server Error"; break;
		case 501: message = "Not Implemented"; break;
		case 502: message = "Bad Gateway"; break;
		case 503: message = "Service Unavailable"; break;
		case 504: message = "Gateway Time-out"; break;
		case 505: message = "HTTP Version not supported"; break;
	}
	return message;
}

CSHTTPOutputStream *CSHTTPOutputStream::newStream(CSOutputStream* i)
{
	CSHTTPOutputStream *s;

	if (!(s = new CSHTTPOutputStream(i))) {
		i->release();
		CSException::throwOSError(CS_CONTEXT, ENOMEM);
	}
	return s;
}


