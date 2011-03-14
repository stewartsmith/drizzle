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
 *
 */

#pragma once
#ifndef __CSOBJECT_H__
#define __CSOBJECT_H__

#include "CSDefs.h"
#include "CSMemory.h"
#include "CSMutex.h"

// The RETAIN() macro is useful when passing object references to functions
// that you want to retain. For example istead of:
// x->retain();
// foo(x);
// You can do:
// foo(RETAIN(x));
//
// After 20 years of programming I have finally found a use for the 'C' comma
// operator out side of a 'for' loop. :)
#define RETAIN(x) ((x)->retain(),(x))

// A back reference is a reference stored by an object pointing to it's owner.
// Since the owner will free all of it's object when it is released the  object
// itself cannot hold a reference to its owner.
// Use this macro to make it clear that this is what you intended. 
#define BACK_REFERENCE(x) (x)

class CSObject {
public:
	CSObject() { }
	virtual ~CSObject() { finalize(); }

#ifdef DEBUG
	virtual void retain(const char *func, const char *file, uint32_t line);
	virtual void release(const char *func, const char *file, uint32_t line);
#else
	virtual void retain();
	virtual void release();
#endif

	/*
	 * All objects are sortable, hashable and linkable,
	 * as long as these methods are implemented:
	 */
	virtual void finalize() { }
	virtual CSObject *getKey();
	virtual int compareKey(CSObject *);
	virtual uint32_t hashKey();
	virtual CSObject *getHashLink();
	virtual void setHashLink(CSObject *);
	virtual CSObject *getNextLink();
	virtual CSObject *getPrevLink();
	virtual void setNextLink(CSObject *);
	virtual void setPrevLink(CSObject *);

#ifdef DEBUG
	static void *operator new(size_t size, const char *func, const char *file, uint32_t line) {
		return cs_mm_malloc(func, file, line, size);
	}

	static void operator delete(void *ptr, size_t size) {
		UNUSED(size);
		cs_mm_free(ptr);
	}

#endif
};

class CSStaticObject : public CSObject {
#ifdef DEBUG
	virtual void retain(const char *func, const char *file, uint32_t line);
	virtual void release(const char *func, const char *file, uint32_t line);

	int		iTrackMe;
#else
	virtual void retain();
	virtual void release();
#endif
};

class CSRefObject : public CSObject {
public:
	CSRefObject();
	virtual ~CSRefObject();

#ifdef DEBUG
	virtual void startTracking();
	virtual void retain(const char *func, const char *file, uint32_t line);
	virtual void release(const char *func, const char *file, uint32_t line);

	int		iTrackMe;
#else
	virtual void retain();
	virtual void release();
#endif

	uint32_t getRefCount() { return iRefCount;}

private:
	uint32_t	iRefCount;
};

class CSSharedRefObject : public CSObject, public CSSync {
public:
	CSSharedRefObject();
	virtual ~CSSharedRefObject();

#ifdef DEBUG
	virtual void startTracking();
	virtual void retain(const char *func, const char *file, uint32_t line);
	virtual void release(const char *func, const char *file, uint32_t line);

	int		iTrackMe;
#else
	virtual void retain();
	virtual void release();
#endif

	uint32_t getRefCount() { return iRefCount;}

private:
	uint32_t	iRefCount;
};

#ifdef DEBUG
#define new                    new(__FUNC__, __FILE__, __LINE__)

#define retain()	retain(__FUNC__, __FILE__, __LINE__)
#define release()	release(__FUNC__, __FILE__, __LINE__)
#endif

class CSPooled {
public:
	virtual ~CSPooled() {}
	virtual void returnToPool() = 0;
};

#endif
