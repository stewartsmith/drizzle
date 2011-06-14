/* Copyright (C) 2010 PrimeBase Technologies GmbH, Germany
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
 * Paul McCullagh (H&G2JCtL)
 *
 * 2010-01-12
 *
 * CORE SYSTEM:
 * XML Parsing
 *
 */

#include <inttypes.h>
#include <wchar.h>

#pragma once
#ifndef __CSXML_H__
#define __CSXML_H__

#define CS_XML_ERR_OUT_OF_MEMORY		-1
#define CS_XML_ERR_CHAR_TOO_LARGE		-2

#define CS_XML_EOF_CHAR					WCHAR_MAX

#define CS_MAX_XML_NAME_SIZE			48
#define CS_XML_ERR_MSG_SIZE				128

/* pxml.h 23.3.01 Paul McCullagh */
/* Parse XML */
/* Entities understood by XML:
   &gt;		(>)
   &lt;		(<)
   &amp;	(&)
   &apos;	(')
   &quot;	(")

   Processing Instructions		<? ... ?>
   CDATA Sections				<![CDATA[ ... ]]>
   Document Type Definition		<!DOCTYPE ... [ ...markup... ] >
   Conditional Sections			<![ ... [ ...markup... ]]>
 */

#define XML_BEFORE_CDATA				0		/* XXX */
#define XML_IN_CDATA					1		/* XXX */

#define XML_LT							2		/* < */
#define XML_LT_BANG						3		/* <! */
#define XML_LT_BANG_DASH				4		/* <!- */
#define XML_LT_BANG_SQR					5		/* <![ */
#define XML_LT_BANG_SQR_IN_NAME			6
#define XML_LT_BANG_SQR_AFTER_NAME		7

#define XML_IN_TAG_NAME					8		/* abc */

#define XML_BEFORE_ATTR					9		/* ' ' */
#define XML_IN_ATTR						10		/* xyz */

#define XML_BEFORE_EQUAL				11		/* ' ' */
#define XML_AFTER_EQUAL					12		/* ' ' */

#define XML_QUOTE_BEFORE_VALUE			13		/* " or ' */
#define XML_IN_VALUE					14		/* ... */
#define XML_QUOTE_AFTER_VALUE			15		/* " or ' */

#define XML_SLASH						16		/* / */
#define XML_QMARK						17		/* ? */
#define XML_SQR							18		/* ] */

#define XML_IN_COMMENT					19		/* <!--... */
#define XML_IN_COMMENT_DASH				20		/* - */
#define XML_IN_COMMENT_DASH_DASH		21		/* -- */
#define XML_IN_COMMENT_3_DASH			22		/* --- */

#define XML_IN_CDATA_TAG				23		/* <![CDATA[... */
#define XML_IN_CDATA_TAG_SQR			24		/* ] */
#define XML_IN_CDATA_TAG_SQR_SQR		25		/* ]] */
#define XML_IN_CDATA_TAG_3_SQR			26		/* ]]] */

#define PARSE_BUFFER_SIZE				20
#define PARSE_STACK_SIZE				200

#define END_TAG_TYPE(x)					(x->nesting-1 < PARSE_STACK_SIZE ? x->end_type[x->nesting-1] : XML_OP_1_END_UNKNOWN_TAG)

#define TO_LONG_CHAR(ch)				((unsigned char) (ch))

#define XML_STEP_NONE					0
#define XML_STEP_TAG					1
#define XML_STEP_ATTR					2
#define XML_STEP_VALUE					3
#define XML_STEP_NESTED					4

class CSXMLParser {
	public:
	CSXMLParser() :
		state(0),
		quote(0),
		step(0),
		type(0),
		count(0),
		nesting(0) {
	}
	virtual ~CSXMLParser() { }

	int32_t parseChar(wchar_t ch);
	void setDataType(int32_t t) { type = t; }
	int32_t getDataLen() { return count; }
	wchar_t *getDataPtr() { return buffer; }

	private:
	/* Internal information: */
	int32_t			state;
	int32_t			quote;
	int32_t			step;

	/* Data: output is always in the buffer: */
	int32_t			type;							/* Type of data in the buffer. */
	int32_t			count;							/* Size of the buffer.  */
	wchar_t			buffer[PARSE_BUFFER_SIZE];		/* Contains data to be added. */

	/* Signals: tag start and end: */
	int32_t			nesting;						/* Tag nesting depth. */
	uint8_t			end_type[PARSE_STACK_SIZE];		/* Stack of tag types */

	bool match_string(const char *ch);
	void increment_nesting(wchar_t ch);
};

#define XML_OP_1_MASK					0x0000000F
#define XML_ERROR						0x00001000

#define XML_OP_1_NOOP					0x00000000
#define XML_OP_1_END_TAG				0x00000001		/* < ... >   */
#define XML_OP_1_END_CLOSE_TAG			0x00000002		/* </ ... >  */
#define XML_OP_1_END_EMPTY_TAG			0x00000003		/* < ... />  */
#define XML_OP_1_END_PI_TAG				0x00000004		/* <? ... ?> */
#define XML_OP_1_END_ENTITY_TAG			0x00000005		/* <! ... >  */
#define XML_OP_1_END_BRACKET_TAG		0x00000006		/* <![ ... ]> */
#define XML_OP_1_END_UNKNOWN_TAG		0x00000007		/* <_ ... > */
#define XML_OP_1_START_CDATA_TAG		0x00000008		/* <![CDATA[ ... */
#define XML_OP_1_START_COMMENT			0x00000009		/* <!-- ... */
#define XML_OP_1_START_TAG				0x0000000A		/* <... */
#define XML_OP_1_ADD_ATTR				0x0000000B
#define XML_OP_1_END_CDATA				0x0000000C
#define XML_OP_1_END_CDATA_TAG			0x0000000D		/* ... ]]> */
#define XML_OP_1_END_COMMENT			0x0000000E		/* ... --> */

#define XML_DATA_MASK					0x000000F0

#define XML_NO_DATA						0x00000000
#define XML_DATA_TAG					0x00000010
#define XML_DATA_ATTR					0x00000020
#define XML_DATA_CDATA					0x00000030
#define XML_DATA_CDATA_TAG				0x00000040
#define XML_COMMENT						0x00000050
#define XML_DATA_VALUE					0x00000060

#define XML_OP_2_MASK					0x00000F00

#define XML_OP_2_NOOP					0x00000000
#define XML_OP_2_END_TAG				0x00000100
#define XML_OP_2_END_CLOSE_TAG			0x00000200
#define XML_OP_2_END_EMPTY_TAG			0x00000300
#define XML_OP_2_END_PI_TAG				0x00000400
#define XML_OP_2_END_ENTITY_TAG			0x00000500
#define XML_OP_2_END_BRACKET_TAG		0x00000600
#define XML_OP_2_END_UNKNOWN_TAG		0x00000700
#define XML_OP_2_START_CDATA_TAG		0x00000800
#define XML_OP_2_START_COMMENT			0x00000900

#define XML_noop						(XML_OP_2_NOOP|XML_NO_DATA)

#define XML_CDATA_CH					(XML_DATA_CDATA)
#define XML_end_cdata_TAG_CH			(XML_OP_1_END_CDATA|XML_DATA_TAG)
#define XML_start_tag_TAG_CH			(XML_OP_1_START_TAG|XML_DATA_TAG)
#define XML_add_attr_TAG_CH				(XML_OP_1_ADD_ATTR|XML_DATA_TAG)
#define XML_TAG_CH						(XML_DATA_TAG)
#define XML_start_tag_ATTR_CH			(XML_OP_1_START_TAG|XML_DATA_ATTR)
#define XML_add_attr_ATTR_CH			(XML_OP_1_ADD_ATTR|XML_DATA_ATTR)
#define XML_ATTR_CH						(XML_DATA_ATTR)
#define XML_start_tag_VALUE_CH			(XML_OP_1_START_TAG|XML_DATA_VALUE)
#define XML_add_attr_VALUE_CH			(XML_OP_1_ADD_ATTR|XML_DATA_VALUE)
#define XML_VALUE_CH					(XML_DATA_VALUE)
#define XML_start_tag_end_tag(x)		(XML_OP_1_START_TAG|((x) << 8))
#define XML_add_attr_end_tag(x)			(XML_OP_1_ADD_ATTR|((x) << 8))
#define XML_end_tag(x)					(x)
#define XML_start_tag_end_empty_tag		XML_start_tag_end_tag(XML_OP_1_END_EMPTY_TAG)
#define XML_add_attr_end_empty_tag		XML_add_attr_end_tag(XML_OP_1_END_EMPTY_TAG)
#define XML_end_empty_tag				XML_end_tag(XML_OP_1_END_EMPTY_TAG)
#define XML_start_tag_end_pi_tag		XML_start_tag_end_tag(XML_OP_1_END_PI_TAG)
#define XML_add_attr_end_pi_tag			XML_add_attr_end_tag(XML_OP_1_END_PI_TAG)
#define XML_end_pi_tag					XML_end_tag(XML_OP_1_END_PI_TAG)

#define XML_end_cdata_start_cdata_tag	(XML_OP_1_END_CDATA|XML_OP_2_START_CDATA_TAG)
#define XML_start_tag_start_cdata_tag	(XML_OP_1_START_TAG|XML_OP_2_START_CDATA_TAG)
#define XML_add_attr_start_cdata_tag	(XML_OP_1_ADD_ATTR|XML_OP_2_START_CDATA_TAG)
#define XML_start_cdata_tag				(XML_OP_1_START_CDATA_TAG)
#define XML_CDATA_TAG_CH				(XML_DATA_CDATA_TAG)
#define XML_end_cdata_tag				(XML_OP_1_END_CDATA_TAG)

#define XML_end_cdata_start_comment		(XML_OP_1_END_CDATA|XML_OP_2_START_COMMENT)
#define XML_start_tag_start_comment		(XML_OP_1_START_TAG|XML_OP_2_START_COMMENT)
#define XML_add_attr_start_comment		(XML_OP_1_ADD_ATTR|XML_OP_2_START_COMMENT)
#define XML_start_comment				(XML_OP_1_START_COMMENT)
#define XML_COMMENT_CH					(XML_COMMENT)
#define XML_end_comment					(XML_OP_1_END_COMMENT)

/* Standard charsets are ISO-8879-1, US-ASCII or UNICODE. None
 * require conversion!
 */
#define CHARSET_STANDARD				0
#define CHARSET_UTF_8					1
#define CHARSET_TO_CONVERT_8_BIT		2

class CSXMLProcessor : public CSXMLParser {
	public:
	CSXMLProcessor() :
		err_no(0),
		ip(false),
		tlength(0),
		nlength(0),
		vlength(0),
		utf8_count(0),
		utf8_length(0),
		elength(0) {
		err_message[0] = 0;
		charset[0] = 0;
		pr_tag[0] = 0;
		pr_name[0] = 0;
		pr_value[0] = 0;
		utf8_buffer[0] = 0;
		entity[0] = 0;
	}
	virtual ~CSXMLProcessor() { }

	/* This function processes a UNICODE character from an XML
	 * document returns parsing instructions (operations).
	 * Each instruction can consist of up to 3 operations. The
	 * operations must be executed in the following order:
	 * - Operation 1
	 * - Data operation, record one of the following:
	 *   - part of a tag name
	 *   - part of an attribute name
	 *   - part of an attribute value
	 *   - part of CDATA
	 * - Operation 2
	 * Output for the data operation (if any) is placed in the buffer
	 * in the state structure. The input state structure must be zeroed
	 * before processing begins. Input characters may be 1 byte or
	 * 2 byte. Output is always 2-byte UNICODE.
	 */
	int32_t processChar(wchar_t ch);

	bool getError(int32_t *err, char **msg);
	void setError(int32_t err, char *msg);
	void printError(char *prefix);

	private:
	int32_t			err_no;
	char			err_message[CS_XML_ERR_MSG_SIZE];

	private:
	/* When this function is called, use the name of the charset.
	 * to build the conversion table which maps characters in the
	 * range 128 to 255 to the unicode eqivalent.
	 */
	virtual bool buildConversionTable();

	int32_t			charset_type;
	char			charset[CS_MAX_XML_NAME_SIZE];
	wchar_t			conversion_table[128];

	bool			ip;
	size_t			tlength;
	char			pr_tag[CS_MAX_XML_NAME_SIZE];
	size_t			nlength;
	char			pr_name[CS_MAX_XML_NAME_SIZE];
	size_t			vlength;
	char			pr_value[CS_MAX_XML_NAME_SIZE];

	int32_t			utf8_count;
	int32_t			utf8_length;
	uint32_t		utf8_buffer[6];

	int32_t			elength;
	char			entity[CS_MAX_XML_NAME_SIZE];

	int32_t capture_initializer(wchar_t ch);
	int32_t entity_translator(wchar_t ch);
	int32_t charset_transformer(wchar_t ch);
	void appendWCharToString(char *dstr, size_t *dlen, size_t dsize, wchar_t *schars, size_t slen);
};

/* path is a / separated list of nodes to date. */
/* Name and path are given in lower-case!!! */

#define XML_KEEP_EMPTY_CDATA	1

class CSXMLString {
	public:
	CSXMLString() : stringPtr(NULL), stringLen(0), stringSize(0) {}
	virtual ~CSXMLString() { }

	public:
	bool addChar(char ch, CSXMLProcessor *xml);
	bool addChars(size_t size, wchar_t *buffer, bool to_lower, CSXMLProcessor *xml);
	bool addString(const char *string, CSXMLProcessor *xml);
	void setEmpty();
	void setNull();
	char *lastComponent();
	char *findTrailingComponent(const char *comp);
	void truncate(char *ptr);

	char			*stringPtr;
	size_t			stringLen;
	size_t			stringSize;
};

class CSXML : public CSXMLProcessor {
	public:
	bool parseXML(int32_t flags);

	private:
	/*
	 * Return CS_XML_EOF_CHAR when there are no more characters.
	 */
	virtual bool getChar(wchar_t *ch) = 0;

	/*
	 * These methods are called as the input data
	 * is parsed.
	 */
	virtual bool openNode(char *path, char *value) = 0;
	virtual bool closeNode(char *path) = 0;
	virtual bool addAttribute(char *path, char *name, char *value) = 0;

	private:
	uint32_t		flags;

	CSXMLString		xml_path;
	CSXMLString		xml_name;
	CSXMLString		xml_value;

	int32_t nodeType(char *name);
	bool internalCloseNode(const char *name, bool single);
	bool internalOpenNode(const char *name);
};

class CSXMLPrint : public CSXML {
	private:
	virtual bool openNode(char *path, char *value);
	virtual bool closeNode(char *path);
	virtual bool addAttribute(char *path, char *name, char *value);
};

class CSXMLBuffer : public CSXMLPrint {
	public:
	bool parseString(const char *data, int32_t flags);
	bool parseData(const char *data, size_t len, int32_t flags);

	private:
	virtual bool getChar(wchar_t *ch);

	private:
	const char		*charData;
	size_t			dataLen;
	size_t			dataPos;
};

class CSXMLFile : public CSXMLPrint {
	public:
	bool parseFile(char *file_name, int32_t flags);

	private:
	virtual bool getChar(wchar_t *ch);

	private:
	char			*fileName;
	FILE			*file;
};

#endif
