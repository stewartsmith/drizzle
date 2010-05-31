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
 * EOL and EOF are also tokens.
 *
 */

#include "CSConfig.h"

#include "CSGlobal.h"
#include "CSTokenStream.h"

const char *CSToken::singletons = "=[]!#$^&*()|;<>?~";

CSTokenStream::~CSTokenStream()
{
	enter_();
	if (iStream)
		iStream->release();
	exit_();
}

void CSTokenStream::open(CSInputStream *stream, uint32_t line)
{
	enter_();
	if (iStream)
		iStream->release();
	iStream = stream;
	iLine = line;
	iChar = -2;
	exit_();
}

void CSTokenStream::close()
{
	enter_();
	iStream->close();
	exit_();
}
	
void CSTokenStream::nextChar()
{
	enter_();
	iChar = iStream->read();
	exit_();
}

CSTokenStream *CSTokenStream::newTokenStream(CSInputStream *stream, uint32_t line)
{
	return UXTokenStream::newTokenStream(stream, line);
}

CSToken *UXTokenStream::nextToken()
{
	char	quote;
	char	*ptr;

	enter_();
	/* Initialize the current character: */
	if (iChar == -2)
		nextChar();

	/* Ignore space: */
	while (iChar == ' ' || iChar == '\t')
		nextChar();

	/* Handle the singletons: */
	if ((ptr = strchr(CSToken::singletons, iChar))) {
		int ch = iChar;

		iChar = -2;
		return_(newToken((int) (ptr - CSToken::singletons), ch));
	}

	/* Handle EOF and EOL: */
	switch (iChar) {
		case -1:
			return_(newToken(CS_TKN_EOS, "[EOF]"));
		case '\r':
			nextChar();
			if (iChar == '\n') {
				iChar = -2;
				if (iLine) iLine++;
				return_(newToken(CS_TKN_EOL, "\r\n"));
			}
			if (iLine) iLine++;
			return_(newToken(CS_TKN_EOL, "\r"));
		case '\n':
			iChar = -2;
			if (iLine) iLine++;
			return_(newToken(CS_TKN_EOL, "\n"));
	}

	/* Now we have some text
	 * The text ends with the first space, EOF, EOL
	 * or a singleton.
	 */
	CSStringBuffer *text;
	
	new_(text, CSStringBuffer());
	push_(text);

	for (;;) {
		switch (iChar) {
			case '`':
				// ` are included in the string to indicate it should be executed
				text->append((char) iChar);
			case '"':
			case '\'':
				/* Quoted text: */
				quote = iChar;
				nextChar();
				while (iChar != quote && iChar != -1 && iChar != '\r' && iChar != '\n') {
					text->append((char) iChar);
					nextChar();
				}
				if (iChar == quote) {
					if (quote == '`')
						text->append((char) quote);
					nextChar();
				}
				break;
			case '\\':
				/* Escaped character: */
				nextChar();
				if (iChar != -1) {
					text->append((char) iChar);
					nextChar();
				}
				break;
			case -1:
			case ' ':
			case '\t':
			case '\r':
			case '\n':
				/* These characters terminated the text: */
				goto exit_loop;
			default:
				/* Singletons terminate the text as well, unless they
				 * were escaped or in quotes.
				 */
				if ((ptr = strchr(CSToken::singletons, iChar)))
					goto exit_loop;
				text->append((char) iChar);
				nextChar();
		}
	}
	exit_loop:
	
	char *str = text->take();
	release_(text);
	return_(newToken(CS_TKN_TEXT, str));
}

CSToken *UXTokenStream::newToken(int type, const char *text)
{
	CSToken *tk;

	new_(tk, UXToken(type, text));
	return tk;
}

CSToken *UXTokenStream::newToken(int type, char ch)
{
	CSToken *tk;

	new_(tk, UXToken(type, ch));
	return tk;
}

CSTokenStream *UXTokenStream::newTokenStream(CSInputStream *stream, uint32_t line)
{
	CSTokenStream *s;
	enter_();
	push_(stream);
	new_(s, UXTokenStream());
	pop_(stream);
	s->open(stream, line);
	return_(s);
}

