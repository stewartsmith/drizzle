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
 * CORE SYSTEM STORAGE
 * Basic storage structures.
 *
 */

#pragma once
#ifndef __CSSTORAGE_H__
#define __CSSTORAGE_H__

#include <stdio.h>

#include "CSDefs.h"
#include "CSString.h"

class CSHashTable;

class CSHashTable : public CSObject {
public:
	CSHashTable(): iSize(0), iTable(NULL) { }
	virtual ~CSHashTable();

	void setSize(uint32_t size);

	void clear();

	/* Value must be given referenced. */
	void add(CSObject *item);

	/* Value is returned NOT referenced. */
	CSObject *find(CSObject *key);
	
	bool remove(CSObject *key);

private:
	uint32_t iSize;

	CSObject **iTable;
};

#define SC_SORT_LIST_INC_SIZE		20

class CSSortedList : public CSObject {
public:
	CSSortedList(): iListSize(0), iInUse(0), iList(NULL) { }
	virtual ~CSSortedList() { clear(); }

	void clear();

	/* Value must be given referenced. */
	void add(CSObject *item);
	
	/* Value is returned NOT referenced. */
	CSObject *find(CSObject *key);

	CSObject *itemAt(uint32_t idx);
	
	CSObject *takeItemAt(uint32_t idx); // Takes item off of list.
	
	void remove(CSObject *key);
	
	uint32_t getSize() { return iInUse; }


private:
	uint32_t iListSize;

	uint32_t iInUse;

	CSObject **iList;

	CSObject *search(CSObject *key, uint32_t& idx);
};

class CSSyncSortedList : public CSSortedList, public CSSync {
public:
	CSSyncSortedList(): CSSortedList(), CSSync() { }
};

class CSLinkedItem : public CSRefObject {
public:
	CSLinkedItem(): CSRefObject(), iNextLink(NULL), iPrevLink(NULL) { }
	virtual ~CSLinkedItem() { }

	virtual CSObject *getNextLink() { return iNextLink; }
	virtual CSObject *getPrevLink() { return iPrevLink; }
	virtual void setNextLink(CSObject *link) { iNextLink = link; }
	virtual void setPrevLink(CSObject *link) { iPrevLink = link; }

private:
	CSObject		*iNextLink;
	CSObject		*iPrevLink;
};

/*
 * Items are linked so that following the previous pointers
 * you move from front to back.
 * Following the next pointers you move from back to front.
 */
class CSLinkedList : public CSObject {
public:
	CSLinkedList(): iSize(0), iListFront(NULL), iListBack(NULL) { }
	virtual ~CSLinkedList() { clear(); }

	void clear();
	
	uint32_t getSize() { return iSize; }

	/* Value must be given referenced. */
	void addFront(CSObject *item);
	void addBack(CSObject *item);

	bool remove(CSObject *item);

	/* Value is returned referenced. */
	CSObject *removeBack();

	/* Value is returned NOT referenced. */
	CSObject *getBack();

	/* Value is returned NOT referenced. */
	CSObject *getFront();

	/* Value is returned referenced. */
	CSObject *removeFront();
private:
	uint32_t iSize;
	CSObject *iListFront;
	CSObject *iListBack;
};

class CSSyncLinkedList : public CSLinkedList, public CSSync {
public:
	CSSyncLinkedList(): CSLinkedList(), CSSync() { }
};

class CSVector : public CSObject {
public:
	CSVector(uint32_t growSize): iGrowSize(growSize), iMaxSize(0), iUsage(0), iArray(NULL) { }
	virtual ~CSVector() { free(); }

	void free();

	void clear();

	/*
	 * Remove and object from the vector, and
	 * The object is rfemoved from the list.
	 * return a reference.
	 */
	CSObject *take(uint32_t idx);

	/*
	 * Remove an object from the vector.
	 */
	void remove(uint32_t idx);

	/*
	 * Get a reference to an object in the vector.
	 * A reference to the object remains on the list.
	 * Value returned is NOT referenced!
	 */
	CSObject *get(uint32_t idx);

	/* Set a specific index: */
	void set(uint32_t idx, CSObject *);

	/*
	 * Add an object to the end of the vector.
	 * Value must be referenced.
	 */
	void add(CSObject *);

	uint32_t size() { return iUsage; }

private:
	uint32_t iGrowSize;
	uint32_t iMaxSize;
	uint32_t iUsage;

	CSObject **iArray;
};

class CSSyncVector : public CSVector, public CSSync {
public:
	CSSyncVector(uint32_t growSize): CSVector(growSize), CSSync() { }
};

typedef struct CSSpareArrayItem {
	uint32_t		sa_index;
	CSObject	*sa_object;
} CSSpareArrayItemRec, *CSSpareArrayItemPtr;

class CSSparseArray : public CSObject {
public:
	CSSparseArray(uint32_t growSize): iGrowSize(growSize), iMaxSize(0), iUsage(0), iArray(NULL) { }
	CSSparseArray(): iGrowSize(10), iMaxSize(0), iUsage(0), iArray(NULL) { }
	virtual ~CSSparseArray() { free(); }

	void free();

	void clear();

	CSObject *take(uint32_t sparse_idx);

	void remove(uint32_t sparse_idx);

	void removeFirst();

	CSObject *itemAt(uint32_t idx);

	CSObject *get(uint32_t sparse_idx);
	
	uint32_t getIndex(uint32_t sparse_idx);
	
	void set(uint32_t sparse_idx, CSObject *);

	uint32_t size() { return iUsage; }

	uint32_t minIndex() {
		if (iUsage == 0)
			return 0;
		return iArray[0].sa_index;
	}

	uint32_t maxIndex() {
		if (iUsage == 0)
			return 0;
		return iArray[iUsage-1].sa_index;
	}

	CSObject *first();

	CSObject *last();

private:
	uint32_t				iGrowSize;
	uint32_t				iMaxSize;
	uint32_t				iUsage;
	CSSpareArrayItemPtr	iArray;

	CSObject *search(uint32_t idx, uint32_t& pos);
};

class CSSyncSparseArray : public CSSparseArray, public CSSync {
public:
	CSSyncSparseArray(uint32_t growSize): CSSparseArray(growSize), CSSync() { }
};

class CSOrderKey : public CSObject {
public:
	virtual int compareKey(CSOrderKey *key) = 0;
	int compareKey(CSObject *key) {return CSObject::compareKey(key);}
};

typedef struct CSOrderedListItem {
	CSOrderKey	*li_key;
	CSObject	*li_item;
} CSOrderedListItemRec, *CSOrderedListItemPtr;

class CSOrderedList : public CSObject {
public:
	CSOrderedList(): iListSize(0), iInUse(0), iList(NULL) { }
	virtual ~CSOrderedList() { clear(); }

	void clear();

	/* Value must be given referenced. */
	void add(CSOrderKey *key, CSObject *item);
	
	/* Value is returned NOT referenced. */
	CSObject *find(CSOrderKey *key);
	
	/* Value is returned NOT referenced. */
	CSObject *itemAt(uint32_t idx);
	
	void remove(CSOrderKey *key);

private:
	uint32_t iListSize;

	uint32_t iInUse;

	CSOrderedListItemPtr iList;

	CSOrderedListItemPtr search(CSOrderKey *key, uint32_t *idx);
};

class CSSyncOrderedList : public CSOrderedList, public CSSync {
public:
	CSSyncOrderedList(): CSOrderedList(), CSSync() { }
};

class CSQueue;

typedef struct CSQueueItem {
	CSObject			*qi_object;
	CSQueue				*qi_next;
} CSQueueItemRec, *CSQueueItemPtr;

class CSQueue : public CSObject {
public:
	CSQueue(): iQueueSize(0), iHighWater(0), iFront(NULL), iBack(NULL), iFree(NULL) { }
	virtual ~CSQueue() { clear(); }

	void clear();

	/* Value must be given referenced. */
	void add(CSObject *item);

	/* Returns a referenced value, on a FIFO basis. */
	CSObject *remove();

private:
	uint32_t iQueueSize;
	uint32_t iHighWater;

	CSQueueItemPtr	iFront;
	CSQueueItemPtr	iBack;
	CSQueueItemPtr	iFree;
};

#endif
