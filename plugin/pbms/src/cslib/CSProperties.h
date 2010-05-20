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

#ifndef __CSPROPERTIES_H__
#define __CSPROPERTIES_H__

#include "CSDefs.h"
#include "CSTokenStream.h"
#include "CSObject.h"

class CSProperties : public CSObject {
public:
	CSProperties() { }
	virtual ~CSProperties() { }
	
	virtual void setProperty(CSToken *name, CSToken *value) {
		name->release();
		value->release();
	}

	void load(CSInputStream *stream);

	static CSProperties *newProperties(CSInputStream *stream);
private:
};

#endif
