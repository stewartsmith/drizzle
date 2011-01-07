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
 * 2007-05-20
 *
 * A basic syncronized object.
 *
 */

#include "CSConfig.h"

#include <assert.h>

#include "CSGlobal.h"
#include "CSDefs.h"
#include "CSObject.h"
#include "CSMemory.h"

#ifdef DEBUG
#undef retain
#undef release
#endif

/*
 * ---------------------------------------------------------------
 * BASIC OBJECTS
 */

#ifdef DEBUG
void CSObject::retain(const char *func, const char *file, uint32_t line)
{
	CSException::throwAssertion(func, file, line, "Non-referenced object cannot be referenced");
}
#else
void CSObject::retain()
{
	CSException::throwAssertion(CS_CONTEXT, "Non-referenced object cannot be referenced");
}
#endif

#ifdef DEBUG
void CSObject::release(const char *, const char *, uint32_t )
#else
void CSObject::release()
#endif
{
	delete this;
}

CSObject *CSObject::getKey() { CSException::throwCoreError(CS_CONTEXT, CS_ERR_IMPL_MISSING, __FUNC__); return NULL; }

int CSObject::compareKey(CSObject *)  { CSException::throwCoreError(CS_CONTEXT, CS_ERR_IMPL_MISSING, __FUNC__); return 0; }

uint32_t CSObject::hashKey()  { CSException::throwCoreError(CS_CONTEXT, CS_ERR_IMPL_MISSING, __FUNC__); return 0; }

CSObject *CSObject::getHashLink() { CSException::throwCoreError(CS_CONTEXT, CS_ERR_IMPL_MISSING, __FUNC__); return NULL; }

void CSObject::setHashLink(CSObject *) { CSException::throwCoreError(CS_CONTEXT, CS_ERR_IMPL_MISSING, __FUNC__); }

CSObject *CSObject::getNextLink() { CSException::throwCoreError(CS_CONTEXT, CS_ERR_IMPL_MISSING, __FUNC__); return NULL; }

CSObject *CSObject::getPrevLink() { CSException::throwCoreError(CS_CONTEXT, CS_ERR_IMPL_MISSING, __FUNC__); return NULL; }

void CSObject::setNextLink(CSObject *) { CSException::throwCoreError(CS_CONTEXT, CS_ERR_IMPL_MISSING, __FUNC__); }

void CSObject::setPrevLink(CSObject *) { CSException::throwCoreError(CS_CONTEXT, CS_ERR_IMPL_MISSING, __FUNC__); }

/*
 * ---------------------------------------------------------------
 * STATIC OBJECTS
 */

#ifdef DEBUG
void CSStaticObject::retain(const char *, const char *, uint32_t )
#else
void CSStaticObject::retain()
#endif
{
}

#ifdef DEBUG
void CSStaticObject::release(const char *, const char *, uint32_t )
#else
void CSStaticObject::release()
#endif
{
	finalize();
}

/*
 * ---------------------------------------------------------------
 * REFERENCE OBJECTS
 */

CSRefObject::CSRefObject():
CSObject(),
iRefCount(1)
{
#ifdef DEBUG
	iTrackMe = 0;
	cs_mm_track_memory(NULL, NULL, 0, this, true, iRefCount, iTrackMe);
#endif
}

CSRefObject::~CSRefObject()
{
	ASSERT(iRefCount == 0);
}

#ifdef DEBUG
void CSRefObject::retain(const char *func, const char *file, uint32_t line)
#else
void CSRefObject::retain()
#endif
{
	if (!iRefCount)
		CSException::throwAssertion(CS_CONTEXT, "Freed object being retained.");
		
	iRefCount++;
#ifdef DEBUG
	cs_mm_track_memory(func, file, line, this, true, iRefCount, iTrackMe);
#endif
}

#ifdef DEBUG
void CSRefObject::release(const char *func, const char *file, uint32_t line)
#else
void CSRefObject::release()
#endif
{
	bool terminate;

#ifdef DEBUG
	cs_mm_track_memory(func, file, line, this, false, iRefCount, iTrackMe);
#endif
	iRefCount--;
	if (!iRefCount)
		terminate = true;
	else
		terminate = false;

	if (terminate)
		delete this;
}

#ifdef DEBUG
void CSRefObject::startTracking()
{
	iTrackMe = 1;
	cs_mm_track_memory(NULL, NULL, 0, this, true, iRefCount, iTrackMe);
}
#endif

/*
 * ---------------------------------------------------------------
 * SHARED REFERENCE OBJECTS
 */

CSSharedRefObject::CSSharedRefObject():
CSObject(),
CSSync(),
iRefCount(1)
{
#ifdef DEBUG
	iTrackMe = 0;
	cs_mm_track_memory(NULL, NULL, 0, this, true, iRefCount, iTrackMe);
#endif
}

CSSharedRefObject::~CSSharedRefObject()
{
	ASSERT(iRefCount == 0);
}

#ifdef DEBUG
void CSSharedRefObject::retain(const char *func, const char *file, uint32_t line)
#else
void CSSharedRefObject::retain()
#endif
{
	lock();
	iRefCount++;
#ifdef DEBUG
	cs_mm_track_memory(func, file, line, this, true, iRefCount, iTrackMe);
#endif
	unlock();
}

#ifdef DEBUG
void CSSharedRefObject::release(const char *func, const char *file, uint32_t line)
#else
void CSSharedRefObject::release()
#endif
{
	bool terminate;

	lock();
#ifdef DEBUG
	cs_mm_track_memory(func, file, line, this, false, iRefCount, iTrackMe);
#endif
	iRefCount--;
	if (!iRefCount)
		terminate = true;
	else
		terminate = false;
	unlock();

	if (terminate)
		delete this;
}

#ifdef DEBUG
void CSSharedRefObject::startTracking()
{
	iTrackMe = 1;
	cs_mm_track_memory(NULL, NULL, 0, this, true, iRefCount, iTrackMe);
}
#endif

#ifdef DEBUG
/*
void CSSharedRefObject::retain(const char *func, const char *file, uint32_t line)
{
	lock();
	iRefCount++;
	cs_mm_print_track(func, file, line, this, true, iRefCount);
	unlock();
}

void CSSharedRefObject::release(const char *func, const char *file, uint32_t line)
{
	bool terminate;

	lock();
	cs_mm_track_memory(func, file, line, this, false, iRefCount, iTrackMe);
	iRefCount--;
	if (!iRefCount)
		terminate = true;
	else
		terminate = false;
	unlock();

	if (terminate)
		delete this;
}
*/
#endif


