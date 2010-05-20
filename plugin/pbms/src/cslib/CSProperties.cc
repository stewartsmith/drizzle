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
 * 2007-06-14
 *
 * Reads properties from a stream
 *
 */

#include "CSConfig.h"

#include <assert.h>

#include "CSGlobal.h"
#include "CSProperties.h"

CSProperties *CSProperties::newProperties(CSInputStream *stream)
{
	CSProperties *prop;

	enter_();
	push_(stream);
	new_(prop, CSProperties());
	push_(prop);
	prop->load(stream);
	pop_(prop);
	release_(stream);
	return_(prop);
}

void CSProperties::load(CSInputStream *stream)
{
	CSTokenStream	*ts;
	CSToken			*token;
	CSToken			*name = NULL;

	enter_();
	ts = CSTokenStream::newTokenStream(RETAIN(stream), 1);
	push_(ts);

	for (;;) {
		token = ts->nextToken();
		if (token->isEOF()) {
			token->release();
			break;
		}

		if (!token->isEOL()) {
			if (!token->isEqual("#")) {
				name = token;
				push_(name);
				token = ts->nextToken();
				if (token->isEqual("=")) {
					token->release();
					token = ts->nextToken();
				}
				pop_(name);
				if (!token->isEOF() && !token->isEOL()) {
					/* We pass referenced objects to setProperty!: */
					setProperty(name, token);
					token = ts->nextToken();
				}
				else
					/* We pass referenced objects to setProperty!: */
					setProperty(name, NULL);
			}
			while (!token->isEOF() && !token->isEOL()) {
				token->release();
				token = ts->nextToken();
			}
		}

		token->release();
	}

	release_(ts);
	exit_();
}



