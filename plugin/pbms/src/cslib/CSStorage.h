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
 * 2007-06-15
 *
 * CORE SYSTEM STORAGE
 * Basic storage structures.
 *
 */

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

	void setSize(u_int size);

	void clear();

	/* Value must be given referenced. */
	void add(CSObject *item);

	/* Value is returned NOT referenced. */
	CSObject *find(CSObject *key);
	
	void remove(CSObject *key);

private:
	u_int iSize;

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

	CSObject *itemAt(u_int idx);
	
	CSObject *takeItemAt(u_int idx); // Takes item off of list.
	
	void remove(CSObject *key);
	
	u_int getSize() { return iInUse; }


private:
	u_int iListSize;

	u_int iInUse;

	CSObject **iList;

	CSObject *search(CSObject *key, u_int& idx);
};

class CSSyncSortedList : public CSSortedList, public CSSync {
public:
	CSSyncSortedList(): CSSortedList(), CSSync() { }
};

class CSLinkedItem : public CSRefObject {
public:
	CSLinkedItem(): CSRefObject() { }
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
	
	u_int getSize() { return iSize; }

	/* Value must be given referenced. */
	void addFront(CSObject *item);

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
	u_int iSize;
	CSObject *iListFront;
	CSObject *iListBack;
};

class CSVector : public CSObject {
public:
	CSVector(u_int growSize): iGrowSize(growSize), iMaxSize(0), iUsage(0), iArray(NULL) { }
	virtual ~CSVector() { free(); }

	void free();

	void clear();

	/*
	 * Remove and object from the vector, and
	 * The object is rfemoved from the list.
	 * return a reference.
	 */
	CSObject *take(u_int idx);

	/*
	 * Remove an object from the vector.
	 */
	void remove(u_int idx);

	/*
	 * Get a reference to an object in the vector.
	 * A reference to the object remains on the list.
	 * Value returned is NOT referenced!
	 */
	CSObject *get(u_int idx);

	/* Set a specific index: */
	void set(u_int idx, CSObject *);

	/*
	 * Add an object to the end of the vector.
	 * Value must be referenced.
	 */
	void add(CSObject *);

	u_int size() { return iUsage; }

private:
	u_int iGrowSize;
	u_int iMaxSize;
	u_int iUsage;

	CSObject **iArray;
};

class CSSyncVector : public CSVector, public CSSync {
public:
	CSSyncVector(u_int growSize): CSVector(growSize), CSSync() { }
};

typedef struct CSSpareArrayItem {
	u_int		sa_index;
	CSObject	*sa_object;
} CSSpareArrayItemRec, *CSSpareArrayItemPtr;

class CSSparseArray : public CSObject {
public:
	CSSparseArray(u_int growSize): iGrowSize(growSize), iMaxSize(0), iUsage(0), iArray(NULL) { }
	CSSparseArray(): iGrowSize(10), iMaxSize(0), iUsage(0), iArray(NULL) { }
	virtual ~CSSparseArray() { free(); }

	void free();

	void clear();

	CSObject *take(u_int idx);

	void remove(u_int idx);

	void removeFirst();

	CSObject *itemAt(u_int idx);

	CSObject *get(u_int idx);
	
	u_int getIndex(u_int idx);
	
	void set(u_int idx, CSObject *);

	u_int size() { return iUsage; }

	u_int minIndex() {
		if (iUsage == 0)
			return 0;
		return iArray[0].sa_index;
	}

	u_int maxIndex() {
		if (iUsage == 0)
			return 0;
		return iArray[iUsage-1].sa_index;
	}

	CSObject *first();

	CSObject *last();

private:
	u_int				iGrowSize;
	u_int				iMaxSize;
	u_int				iUsage;
	CSSpareArrayItemPtr	iArray;

	CSObject *search(u_int idx, u_int& pos);
};

class CSSyncSparseArray : public CSSparseArray, public CSSync {
public:
	CSSyncSparseArray(u_int growSize): CSSparseArray(growSize), CSSync() { }
};

class CSOrderKey : public CSObject {
public:
	virtual int compareKey(CSOrderKey *key) = 0;
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
	CSObject *itemAt(u_int idx);
	
	void remove(CSOrderKey *key);

private:
	u_int iListSize;

	u_int iInUse;

	CSOrderedListItemPtr iList;

	CSOrderedListItemPtr search(CSOrderKey *key, u_int *idx);
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
	u_int iQueueSize;
	u_int iHighWater;

	CSQueueItemPtr	iFront;
	CSQueueItemPtr	iBack;
	CSQueueItemPtr	iFree;
};

#endif
