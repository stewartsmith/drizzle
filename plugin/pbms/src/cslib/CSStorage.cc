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

#include "CSConfig.h"

#include <assert.h>
#include <string.h>

#include "CSMemory.h"
#include "CSUTF8.h"
#include "CSStorage.h"
#include "CSGlobal.h"

/*
 * ---------------------------------------------------------------
 * HASH TABLE
 */

CSHashTable::~CSHashTable()
{
	clear();
	iSize = 0;
	if (iTable) {
		cs_free(iTable);
		iTable = NULL;
	}

}

void CSHashTable::setSize(uint32_t size)
{
	enter_();
	if (size == 0) {
		if (iTable) {
			cs_free(iTable);
			iTable = NULL;
		}
	}
	else {
		cs_realloc((void **) &iTable, sizeof(CSObject *) * size);
		memset(iTable, 0, sizeof(CSObject *) * size);
	}
	iSize = size;
	exit_();
}

void CSHashTable::add(CSObject *item)
{
	uint32_t h = item->hashKey();

	remove(item->getKey());
	item->setHashLink(iTable[h % iSize]);
	iTable[h % iSize] = item;
}

CSObject *CSHashTable::find(CSObject *key)
{
	uint32_t h = key->hashKey();
	CSObject *item;
	
	item = iTable[h % iSize];
	while (item) {
		if (item->hashKey() == h && item->compareKey(key) == 0)
			return item;
		item = item->getHashLink();
	}
	return NULL;
}

bool CSHashTable::remove(CSObject *key)
{
	uint32_t h = key->hashKey();
	CSObject *item, *prev_item;

	prev_item = NULL;
	item = iTable[h % iSize];
	while (item) {
		if (item->hashKey() == h &&  item->compareKey(key) == 0) {
			/* Remove the object: */
			if (prev_item)
				prev_item->setHashLink(item->getHashLink());
			else
				iTable[h % iSize] = item->getHashLink();
			item->release();
			return true;
		}
		prev_item = item;
		item = item->getHashLink();
	}
	return false;
}

void CSHashTable::clear()
{
	CSObject *item, *tmp_item;

	for (uint32_t i=0; i<iSize; i++) {
		item = iTable[i];
		while ((tmp_item = item)) {
			item = tmp_item->getHashLink();
			tmp_item->release();
		}
	}
}

/*
 * ---------------------------------------------------------------
 * SORTED LIST
 */

void CSSortedList::clear()
{
	if (iList) {
		for (uint32_t i=0; i<iInUse; i++)
			iList[i]->release();
		cs_free(iList);
		iList = NULL;
	}
	iInUse = 0;
	iListSize = 0;
}

void CSSortedList::add(CSObject *item)
{
	CSObject	*old_item;
	uint32_t		idx;

	enter_();
	if ((old_item = search(item->getKey(), idx))) {
		iList[idx] = item;
		old_item->release();
	}
	else {
		if (iInUse == iListSize) {
			push_(item);
			cs_realloc((void **) &iList, (iListSize + SC_SORT_LIST_INC_SIZE) * sizeof(CSObject *));
			pop_(item);
			iListSize += SC_SORT_LIST_INC_SIZE;
		}
		memmove(&iList[idx+1], &iList[idx], (iInUse - idx) * sizeof(CSObject *));
		iInUse++;
		iList[idx] = item;
	}
	exit_();
}

CSObject *CSSortedList::find(CSObject *key)
{
	uint32_t idx;

	return search(key, idx);
}

CSObject *CSSortedList::itemAt(uint32_t idx)
{
	if (idx >= iInUse)
		return NULL;
	return iList[idx];
}

CSObject *CSSortedList::takeItemAt(uint32_t idx)
{
	CSObject	*item;

	if (idx >= iInUse)
		return NULL;
		
	item = 	iList[idx];
	
	iInUse--;
	memmove(&iList[idx], &iList[idx+1], (iInUse - idx) * sizeof(CSObject *));
	return item;
}

void CSSortedList::remove(CSObject *key)
{
	CSObject	*item;
	uint32_t		idx;

	if ((item = search(key, idx))) {
		iInUse--;
		memmove(&iList[idx], &iList[idx+1], (iInUse - idx) * sizeof(CSObject *));
		item->release();
	}
}

CSObject *CSSortedList::search(CSObject *key, uint32_t& idx)
{
	register uint32_t		count = iInUse;
	register uint32_t		i;
	register uint32_t		guess;
	register int		r;

	i = 0;
	while (i < count) {
		guess = (i + count - 1) >> 1;
		r = iList[guess]->compareKey(key);
		if (r == 0) {
			idx = guess;
			return iList[guess];
		}
		if (r < 0)
			count = guess;
		else
			i = guess + 1;
	}

	idx = i;
	return NULL;
}

/*
 * ---------------------------------------------------------------
 * LINK ITEM
 */

/*
 * ---------------------------------------------------------------
 * LINK LIST
 */

void CSLinkedList::clear()
{
	while (iListFront)
		remove(iListFront);
}

void CSLinkedList::addFront(CSObject *item)
{
	if (iListFront != item) {
		remove(item);
		item->setNextLink(NULL);
		item->setPrevLink(iListFront);
		if (iListFront)
			iListFront->setNextLink(item);
		else
			iListBack = item;
		iListFront = item;
		iSize++;
	}
	else
		/* Must do this or I will have one reference too
		 * many!
		 * The input object was given to me referenced,
		 * but I already have the object on my list, and
		 * referenced!
		 */
		item->release();
}

void CSLinkedList::addBack(CSObject *item)
{
	if (iListBack != item) {
		remove(item);
		item->setNextLink(iListBack);
		item->setPrevLink(NULL);
		
		if (iListBack)
			iListBack->setPrevLink(item);
		else
			iListFront = item;
		iListBack = item;
		iSize++;
	}
	else
		/* Must do this or I will have one reference too
		 * many!
		 * The input object was given to me referenced,
		 * but I already have the object on my list, and
		 * referenced!
		 */
		item->release();
}

bool CSLinkedList::remove(CSObject *item)
{
	bool on_list = false;

	if (item->getNextLink()) {
		item->getNextLink()->setPrevLink(item->getPrevLink());
		on_list = true;
	}
	if (item->getPrevLink()) {
		item->getPrevLink()->setNextLink(item->getNextLink());
		on_list = true;
	}
	if (iListFront == item) {
		iListFront = item->getPrevLink();
		on_list = true;
	}
	if (iListBack == item) {
		iListBack = item->getNextLink();
		on_list = true;
	}
	item->setNextLink(NULL);
	item->setPrevLink(NULL);
	if (on_list) {
		item->release();
		iSize--;
		return true;
	}
	return false;
}

CSObject *CSLinkedList::removeBack()
{
	CSObject *item = iListBack;
	
	if (item) {
		/* Removing dereferences the object! */
		remove(RETAIN(item));
	}
	return item;
}

CSObject *CSLinkedList::getBack()
{
	return iListBack;
}

CSObject *CSLinkedList::removeFront()
{
	CSObject *item = iListFront;
	
	if (item) {
		/* Removing dereferences the object! */
		remove(RETAIN(item));
	}
	return item;
}

CSObject *CSLinkedList::getFront()
{
	return iListFront;
}

/*
 * ---------------------------------------------------------------
 * VECTOR
 */

void CSVector::free()
{
	clear();
	iMaxSize = 0;
	if (iArray) {
		cs_free(iArray);
		iArray = NULL;
	}
}

void CSVector::clear()
{
	uint32_t i = iUsage;

	for (;;) {
		if (i == 0)
			break;
		i--;
		if (iArray[i]) {
			CSObject *obj;
			
			obj = iArray[i];
			iArray[i] = NULL;
			obj->release();
		}
	}
	iUsage = 0;
}

CSObject *CSVector::take(uint32_t idx)
{
	CSObject *obj;

	if (idx >= iUsage)
		return NULL;

	obj = iArray[idx];
	iUsage--;
	memmove(&iArray[idx], &iArray[idx+1], (iUsage - idx) * sizeof(CSObject *));
	return obj;
}

void CSVector::remove(uint32_t idx)
{
	CSObject *obj;

	if (idx >= iUsage)
		return;

	obj = iArray[idx];
	iUsage--;
	memmove(&iArray[idx], &iArray[idx+1], (iUsage - idx) * sizeof(CSObject *));
	obj->release();
}

CSObject *CSVector::get(uint32_t idx)
{
	if (idx >= iUsage)
		return NULL;
	return iArray[idx];
}

void CSVector::set(uint32_t idx, CSObject *val)
{
	enter_();
	if (idx >= iMaxSize) {
		push_(val);
		cs_realloc((void **) &iArray, sizeof(CSObject *) * (idx + iGrowSize - 1));
		pop_(val);
		iMaxSize = idx + iGrowSize - 1;
	}
	if (idx >= iUsage) {
		if (idx > iUsage)
			memset(&iArray[iUsage], 0, sizeof(CSObject *) * (idx - iUsage));
		iUsage = idx + 1;
	}
	else if (iArray[idx]) {
		push_(val);
		iArray[idx]->release();
		pop_(val);
	}
	iArray[idx] = val;
	exit_();
}

void CSVector::add(CSObject *obj)
{
	enter_();
	if (iUsage == iMaxSize) {
		push_(obj);
		cs_realloc((void **) &iArray, sizeof(CSObject *) * (iMaxSize + iGrowSize));
		pop_(obj);
		iMaxSize += iGrowSize;
	}
	iArray[iUsage] = obj;
	iUsage++;
	exit_();
}

/*
 * ---------------------------------------------------------------
 * SPARSE ARRAY
 */

void CSSparseArray::free()
{
	clear();
	iMaxSize = 0;
	if (iArray) {
		cs_free(iArray);
		iArray = NULL;
	}
}

void CSSparseArray::clear()
{
	uint32_t i = iUsage;

	for (;;) {
		if (i == 0)
			break;
		i--;
		if (iArray[i].sa_object) {
			CSObject *obj;
			
			obj = iArray[i].sa_object;
			iArray[i].sa_object = NULL;
			obj->release();
		}
	}
	iUsage = 0;
}

CSObject *CSSparseArray::take(uint32_t idx)
{
	uint32_t		pos;
	CSObject	*obj;

	if (!(obj = search(idx, pos)))
		return NULL;
	iUsage--;
	memmove(&iArray[pos], &iArray[pos+1], (iUsage - pos) * sizeof(CSSpareArrayItemRec));
	return obj;
}

void CSSparseArray::remove(uint32_t idx)
{
	uint32_t		pos;
	CSObject	*obj;

	if (!(obj = search(idx, pos)))
		return;
	iUsage--;
	memmove(&iArray[pos], &iArray[pos+1], (iUsage - pos) * sizeof(CSSpareArrayItemRec));
	obj->release();
}

CSObject *CSSparseArray::itemAt(uint32_t idx)
{
	if (idx >= iUsage)
		return NULL;
	return iArray[idx].sa_object;
}

CSObject *CSSparseArray::get(uint32_t idx)
{
	uint32_t pos;

	return search(idx, pos);
}

uint32_t CSSparseArray::getIndex(uint32_t idx)
{
	uint32_t pos;

	// If search fails then pos will be > iUsage
	search(idx, pos);
	return pos;
}

void CSSparseArray::set(uint32_t idx, CSObject *val)
{
	uint32_t		pos;
	CSObject	*obj;

	enter_();
	push_(val);

	if ((obj = search(idx, pos)))
		obj->release();
	else {
		if (iUsage == iMaxSize) {
			cs_realloc((void **) &iArray, (iMaxSize + iGrowSize) * sizeof(CSSpareArrayItemRec));
			iMaxSize += iGrowSize;
		}
		memmove(&iArray[pos+1], &iArray[pos], (iUsage - pos) * sizeof(CSSpareArrayItemRec));
		iUsage++;
		iArray[pos].sa_index = idx;
	}
	pop_(val);
	iArray[pos].sa_object = val;
	exit_();
}

void CSSparseArray::removeFirst()
{
	if (iUsage > 0)
		remove(iArray[0].sa_index);
}

CSObject *CSSparseArray::first()
{
	if (iUsage == 0)
		return NULL;
	return iArray[0].sa_object;
}

CSObject *CSSparseArray::last()
{
	if (iUsage == 0)
		return NULL;
	return iArray[iUsage-1].sa_object;
}

CSObject *CSSparseArray::search(uint32_t idx, uint32_t& pos)
{
	register uint32_t	count = iUsage;
	register uint32_t	i;
	register uint32_t	guess;

	i = 0;
	while (i < count) {
		guess = (i + count - 1) >> 1;
		if (idx == iArray[guess].sa_index) {
			pos = guess;
			return iArray[guess].sa_object;
		}
		if (idx < iArray[guess].sa_index)
			count = guess;
		else
			i = guess + 1;
	}

	pos = i;
	return NULL;
}

/*
 * ---------------------------------------------------------------
 * ORDERED LIST
 */

void CSOrderedList::clear()
{
	if (iList) {
		for (uint32_t i=0; i<iInUse; i++) {
			if (iList[i].li_key)
				iList[i].li_key->release();
			if (iList[i].li_item)
				iList[i].li_item->release();
		}
		cs_free(iList);
		iList = NULL;
	}
	iInUse = 0;
	iListSize = 0;
}

CSObject *CSOrderedList::itemAt(uint32_t idx)
{
	if (idx >= iInUse)
		return NULL;
	return iList[idx].li_item;
}


void CSOrderedList::add(CSOrderKey *key, CSObject *item)
{
	CSOrderedListItemPtr	old_item;
	uint32_t					idx;

	enter_();
	if ((old_item = search(key, &idx))) {
		iList[idx].li_key = key;
		iList[idx].li_item = item;
		if (old_item->li_key)
			old_item->li_key->release();
		if (old_item->li_item)
			old_item->li_item->release();
	}
	else {
		if (iInUse == iListSize) {
			push_(key);
			push_(item);
			cs_realloc((void **) &iList, (iListSize + SC_SORT_LIST_INC_SIZE) * sizeof(CSOrderedListItemRec));
			pop_(item);
			pop_(key);
			iListSize += SC_SORT_LIST_INC_SIZE;
		}
		memmove(&iList[idx+1], &iList[idx], (iInUse - idx) * sizeof(CSOrderedListItemRec));
		iInUse++;
		iList[idx].li_key = key;
		iList[idx].li_item = item;
	}
	exit_();
}

CSObject *CSOrderedList::find(CSOrderKey *key)
{
	uint32_t					idx;
	CSOrderedListItemPtr	ptr;

	if ((ptr = search(key, &idx)))
		return ptr->li_item;
	return NULL;
}

void CSOrderedList::remove(CSOrderKey *key)
{
	CSOrderedListItemPtr	item;
	uint32_t					idx;

	if ((item = search(key, &idx))) {
		CSOrderedListItemRec ir;

		memcpy(&ir, item, sizeof(CSOrderedListItemRec));
		iInUse--;
		memmove(&iList[idx], &iList[idx+1], (iInUse - idx) * sizeof(CSOrderedListItemRec));
		if (ir.li_key)
			ir.li_key->release();
		if (ir.li_item)
			ir.li_item->release();
	}
}

CSOrderedListItemPtr CSOrderedList::search(CSOrderKey *key, uint32_t *idx)
{
	register uint32_t		count = iInUse;
	register uint32_t		i;
	register uint32_t		guess;
	register int		r;

	i = 0;
	while (i < count) {
		guess = (i + count - 1) >> 1;
		r = iList[guess].li_key->compareKey(key);
		if (r == 0) {
			*idx = guess;
			return &iList[guess];
		}
		if (r < 0)
			count = guess;
		else
			i = guess + 1;
	}

	*idx = i;
	return NULL;
}


