/* Copyright (C) 2009 PrimeBase Technologies GmbH, Germany
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
 *  Created by Barry Leslie on 3/20/09.
 *
 */
 
#ifdef DRIZZLED
#include <config.h>
#include <drizzled/common.h>
#include <drizzled/session.h>
#include <drizzled/table.h>
#include <drizzled/message/table.pb.h>
#include <drizzled/charset.h>
#include <drizzled/table_proto.h>
#include <drizzled/session.h>
#include <drizzled/field.h>
#endif

#include "cslib/CSConfig.h"

#include <stdlib.h>
#include <ctype.h>
#include <string.h>

#include "cslib/CSGlobal.h"
#include "cslib/CSThread.h"
#include "cslib/CSLog.h"
#include "cslib/CSPath.h"
#include "cslib/CSFile.h"
#include "cslib/CSString.h"
#include "cslib/CSStrUtil.h"
#include "cslib/CSStorage.h"
#include "cslib/CSEncode.h"
#include "cslib/CSS3Protocol.h"

#include "backup_ms.h"
#include "cloud_ms.h"

CSSyncSparseArray	*MSCloudInfo::gCloudInfo;
uint32_t			MSCloudInfo::gMaxInfoRef;

uint32_t			CloudDB::gKeyIndex;
CSMutex			CloudDB::gCloudKeyLock;

//==============================
MSCloudInfo::MSCloudInfo(uint32_t id,
						const char *server,
						const char *bucket_arg,
						const char *publicKey,
						const char *privateKey
						):
	cloudRefId(id),
	bucket(NULL),
	s3Prot(NULL)
{		
	new_(s3Prot, CSS3Protocol());
	s3Prot->s3_setServer(server);
	s3Prot->s3_setPublicKey(publicKey);
	s3Prot->s3_setPrivateKey(privateKey);
	
	bucket = CSString::newString(bucket_arg);		
}
		
//-------------------------------
MSCloudInfo::~MSCloudInfo()
{
	if (bucket)
		bucket->release();
		
	if (s3Prot)
		s3Prot->release();
}
	
//-------------------------------
const char *MSCloudInfo::getServer() 
{ 
	return s3Prot->s3_getServer();
}

//-------------------------------
const char *MSCloudInfo::getBucket() 
{ 
	return bucket->getCString();
}

//-------------------------------
const char *MSCloudInfo::getPublicKey() 
{ 
	return s3Prot->s3_getPublicKey();
}

//-------------------------------
const char *MSCloudInfo::getPrivateKey() 
{ 
	return s3Prot->s3_getPrivateKey();
}

//-------------------------------
CSString *MSCloudInfo::getSignature(const char *key, const char *content_type, uint32_t *s3AuthorizationTime)
{
	return s3Prot->s3_getAuthorization(bucket->getCString(), key, content_type, s3AuthorizationTime);
}

//-------------------------------
CSString *MSCloudInfo::getDataURL(const char *key, int keep_alive)
{
	return s3Prot->s3_getDataURL(bucket->getCString(), key, keep_alive);
}

//-------------------------------
void MSCloudInfo::send(CSInputStream *input, const char *key, off64_t size)
{
	CSVector *headers;
	headers = s3Prot->s3_send(input, bucket->getCString(), key, size);
	headers->release();
}

//-------------------------------
CSVector *MSCloudInfo::list(const char *key_prefix, uint32_t max)
{
	return s3Prot->s3_list(bucket->getCString(), key_prefix, max);
}

//-------------------------------
void MSCloudInfo::receive(CSOutputStream *output, const char *key)
{
	bool found;
	CSVector *headers;
	
	headers = s3Prot->s3_receive(output, bucket->getCString(), key, &found);
	headers->release();
	if (!found) {
		CSStringBuffer *err;
		enter_();
		
		new_(err, CSStringBuffer());
		push_(err);
		err->append("S3 object not found: ");
		err->append(getServer());
		err->append("/");
		err->append(bucket->getCString());
		err->append("/");
		err->append(key);

		CSException::throwException(CS_CONTEXT, CS_ERR_GENERIC_ERROR, err->getCString());
		release_(err);
		outer_();
	}
}

//-------------------------------
void MSCloudInfo::cDelete(const char *key)
{
	s3Prot->s3_delete(bucket->getCString(), key);
}

//-------------------------------
void MSCloudInfo::copy(MSCloudInfo *dst_cloud, const char *dst_key, const char *src_key)
{
	enter_();
	push_(dst_cloud);
	
	s3Prot->s3_copy(dst_cloud->getServer() ,dst_cloud->bucket->getCString(), dst_key, bucket->getCString(), src_key);

	release_(dst_cloud);
	exit_();
}

//==============================
CloudDB::CloudDB(uint32_t db_id):
	dfltCloudRefId(0),
	keep_alive(5 * 60),// default URL keep alive in seconds.
	blob_recovery_no(0),
	blob_db_id(db_id),
	isBackup(false),
	backupInfo(NULL),
	backupCloud(NULL),
	clObjectKey(NULL)
{
	enter_();

	new_(clObjectKey, CSStringBuffer());
	clObjectKey->setLength(base_key_size);
		
	exit_();
}

//-------------------------------
CloudDB::~CloudDB()
{
	
	if (backupInfo)
		backupInfo->release();
		
	if (backupCloud)
		backupCloud->release();
		
	if (clObjectKey)
		clObjectKey->release();
		
}
//-------------------------------
MSBackupInfo *CloudDB::cl_getBackupInfo()
{ 
	if (backupInfo)
		backupInfo->retain();
		
	return backupInfo;
}

//-------------------------------
void CloudDB::cl_clearBackupInfo(){ backupInfo->release(); backupInfo = NULL;}

//-------------------------------
void CloudDB::cl_createDB()
{
// This is a no-op. 
}

//-------------------------------
// Restore all the 
void CloudDB::cl_restoreDB()
{
	CSVector *list = NULL;
	CSString *key = NULL;
	CloudObjectKey *src_objectKey = NULL, *dst_objectKey = NULL;
	CloudKeyRec		cloudKey;
	uint32_t		src_cloudRef, dst_cloudRef = 0;
	MSBackupInfo	*backup_info = NULL;
	MSCloudInfo		*src_cloud = NULL, *dst_cloud = NULL;
	enter_();

	if (!blob_recovery_no)
		exit_(); // nothing to do.
 
	backup_info = MSBackupInfo::getBackupInfo(blob_recovery_no);
	push_(backup_info);
	
	src_cloudRef = backup_info->getcloudRef();
	src_cloud = MSCloudInfo::getCloudInfo(src_cloudRef);
	push_(src_cloud);
	
	new_(dst_objectKey, CloudObjectKey(blob_db_id));
	push_(dst_objectKey);
	
	// Get the key for the backup BLOB
	new_(src_objectKey, CloudObjectKey(blob_db_id));
	push_(src_objectKey);
	src_objectKey->setObjectKey(NULL, backup_info->getcloudBackupNo(), backup_info->getDatabaseId()); 

	// Get a list of all the BLOBs that were backed up.
	list = src_cloud->list(src_objectKey->getCString());
	release_(src_objectKey);
	push_(list);
	
	
	// Go through the list copying the keys.
	dst_cloudRef = src_cloudRef;
	dst_cloud = src_cloud;
	dst_cloud->retain();
	
	push_ref_(dst_cloud); // Push a reference to dst_cloud so that what ever it references will be released.

	while ((key = (CSString*)(list->take(0))) ) {
		push_(key);

		// The source key name must be parsed to get its
		// destination cloud reference. The destination for
		// the BLOBs may not all be in the same cloud. 
		CloudObjectKey::parseObjectKey(key->getCString(), &cloudKey);
		
		// Reset the destination cloud if required.
		if (cloudKey.cloud_ref != dst_cloudRef) {
			if (dst_cloud) {
				dst_cloud->release();
				dst_cloud = NULL;
			}
			dst_cloudRef = 	cloudKey.cloud_ref;
			dst_cloud = MSCloudInfo::getCloudInfo(dst_cloudRef);
		}

		// Copy the BLOB to the recovered database.
		dst_objectKey->setObjectKey(&cloudKey);
		src_cloud->copy(RETAIN(dst_cloud), dst_objectKey->getCString(), key->getCString());
		release_(key);
		
	}
	
	release_(dst_cloud);
	
	blob_recovery_no = 0;
	release_(list);
	release_(dst_objectKey);	
	release_(src_cloud);
	release_(backup_info);
	exit_();	
}

//-------------------------------
uint32_t CloudDB::cl_getNextBackupNumber(uint32_t cloud_ref)
{
	CloudObjectKey *objectKey;
	CSVector *list;
	uint32_t backup_no = 0, size = 1;
	MSCloudInfo *s3Cloud;
	enter_();

	s3Cloud = MSCloudInfo::getCloudInfo((cloud_ref)?cloud_ref:dfltCloudRefId);
	push_(s3Cloud);
	
	new_(objectKey, CloudObjectKey(blob_db_id));
	push_(objectKey);

	// Find the next available backup number
	while (size) {
		backup_no++;
		objectKey->setObjectKey(NULL, backup_no); // use the key prefix with the backup number for listing.
		list = s3Cloud->list(objectKey->getCString(), 1);
		size = list->size();
		list->release();
	}
	
	release_(objectKey);
	release_(s3Cloud);

	return_(backup_no);
}

//-------------------------------
void CloudDB::cl_backupBLOB(CloudKeyPtr key)
{
	CloudObjectKey *src_objectKey, *dst_objectKey;
	uint32_t cloudRef, backupNo;
	MSCloudInfo *src_cloud = NULL, *dst_cloud = NULL;
	enter_();

	ASSERT(backupInfo);
	
	if ((cloudRef = backupInfo->getcloudRef()) == 0) {
		backupInfo->setcloudRef(dfltCloudRefId);
		cloudRef = dfltCloudRefId;
	}
		
	if ((backupNo = backupInfo->getcloudBackupNo()) == 0) {		
		backupNo = cl_getNextBackupNumber(cloudRef);
		backupInfo->setcloudBackupNo(backupNo);
	}
	
	// Set the source object's key
	new_(src_objectKey, CloudObjectKey(blob_db_id));
	push_(src_objectKey);
	src_objectKey->setObjectKey(key);

	// Set the destination object's key
	new_(dst_objectKey, CloudObjectKey(blob_db_id));
	push_(dst_objectKey);
	dst_objectKey->setObjectKey(key, backupNo);

	// Get the source cloud
	src_cloud = MSCloudInfo::getCloudInfo((key->cloud_ref)?key->cloud_ref:dfltCloudRefId);
	push_(src_cloud);
	
	// Copy the object to the destination cloud
	dst_cloud = MSCloudInfo::getCloudInfo(cloudRef);
	src_cloud->copy(dst_cloud, dst_objectKey->getCString(), src_objectKey->getCString());

	release_(src_cloud);
	release_(dst_objectKey);
	release_(src_objectKey);
	exit_();
}

//-------------------------------
void CloudDB::cl_restoreBLOB(CloudKeyPtr key, uint32_t backup_db_id)
{
	CloudObjectKey *src_objectKey, *dst_objectKey;
	uint32_t cloudRef, backupNo;
	MSCloudInfo *src_cloud = NULL, *dst_cloud = NULL;
	enter_();

	ASSERT(backupInfo);
	
	if ((cloudRef = backupInfo->getcloudRef()) == 0) {
		backupInfo->setcloudRef(dfltCloudRefId);
		cloudRef = dfltCloudRefId;
	}
		
	if ((backupNo = backupInfo->getcloudBackupNo()) == 0) {		
		backupNo = cl_getNextBackupNumber(cloudRef);
		backupInfo->setcloudBackupNo(backupNo);
	}
	
	// Set the source object's key
	new_(src_objectKey, CloudObjectKey(backup_db_id));
	push_(src_objectKey);
	src_objectKey->setObjectKey(key, backupNo);

	// Set the destination object's key
	new_(dst_objectKey, CloudObjectKey(blob_db_id));
	push_(dst_objectKey);
	dst_objectKey->setObjectKey(key);

	// Get the source cloud
	src_cloud = MSCloudInfo::getCloudInfo(cloudRef);
	push_(src_cloud);
	
	// Copy the object to the destination cloud
	dst_cloud = MSCloudInfo::getCloudInfo((key->cloud_ref)?key->cloud_ref:dfltCloudRefId);
	src_cloud->copy(dst_cloud, dst_objectKey->getCString(), src_objectKey->getCString());

	release_(src_cloud);
	release_(dst_objectKey);
	release_(src_objectKey);
	exit_();
}

//-------------------------------
// Drop database deletes all objects with the database key prefix
void CloudDB::cl_dropDB()
{
	CSVector *list;
	CSString *key;
	CloudObjectKey *objectKey;	
	MSCloudInfo *s3Cloud = NULL;
	int i;
	const char *key_str;
	
	enter_();
	new_(objectKey, CloudObjectKey(blob_db_id));
	push_(objectKey);
	
	lock_(MSCloudInfo::gCloudInfo);

	if (isBackup) {
		uint32_t backup_no;
		if (backupInfo && (backup_no = backupInfo->getcloudBackupNo())) {
			objectKey->setObjectKey(NULL, backup_no); // use the key prefix for the backup for listing.
			if ((s3Cloud = MSCloudInfo::getCloudInfo(backupInfo->getcloudRef())))
				push_(s3Cloud);
		}
	} else {
		objectKey->setObjectKey(); // use the key prefix for listing.
		i = 0;
		s3Cloud = (MSCloudInfo*)MSCloudInfo::gCloudInfo->itemAt(i++); // <-- unreferenced object 
	}
		
	key_str = objectKey->getCString();

	// For non backup BLOBs all known clouds must be searched 
	// for possible BLOBs and deleted. The BLOBs belonging to a backup
	// will ever only be in one cloud storage location.
	while (s3Cloud) {
		list = s3Cloud->list(key_str);
		push_(list);
		
		// Go through the list deleting the keys.
		while ((key = (CSString*)(list->take(0))) ) {
			push_(key);
			s3Cloud->cDelete(key->getCString());
			release_(key);
		}
		
		release_(list);
		if (isBackup) {
			release_(s3Cloud); // Only the backup s3Cloud needs to be released.
			s3Cloud = NULL;
		} else
			s3Cloud = (MSCloudInfo*)MSCloudInfo::gCloudInfo->itemAt(i++);// <-- unreferenced object
	}
	
	unlock_(MSCloudInfo::gCloudInfo);
	release_(objectKey);
	exit_();
}

//-------------------------------
void CloudDB::cl_putData(CloudKeyPtr key, CSInputStream *stream, off64_t size)
{
	CloudObjectKey *objectKey;
	MSCloudInfo *s3Cloud;
	
	enter_();
	
	push_(stream);
	
	new_(objectKey, CloudObjectKey(blob_db_id));
	push_(objectKey);
	
	objectKey->setObjectKey(key);
	
	s3Cloud = MSCloudInfo::getCloudInfo((key->cloud_ref)?key->cloud_ref:dfltCloudRefId);
	push_(s3Cloud);
	s3Cloud->send(RETAIN(stream), objectKey->getCString(), size);
	release_(s3Cloud);
	
	release_(objectKey);
	release_(stream);
	
	exit_();
}

//-------------------------------
off64_t CloudDB::cl_getData(CloudKeyPtr key,  char *buffer, off64_t size)
{	
	CloudObjectKey *objectKey;
	CSStaticMemoryOutputStream *output;
	MSCloudInfo *s3Cloud;
	enter_();
	
	new_(objectKey, CloudObjectKey(blob_db_id));
	push_(objectKey);
	
	s3Cloud = MSCloudInfo::getCloudInfo(key->cloud_ref);
	push_(s3Cloud);

	new_(output, CSStaticMemoryOutputStream((u_char *)buffer, size));
	push_(output);
	
	objectKey->setObjectKey(key);
	
	s3Cloud->receive(RETAIN(output), objectKey->getCString());	
	size = output->getSize();
	release_(output);
	
	release_(s3Cloud);
	release_(objectKey);
	return_(size);
}

//-------------------------------
void CloudDB::cl_deleteData(CloudKeyPtr key)
{
	MSCloudInfo *s3Cloud;
	CloudObjectKey *objectKey;
	enter_();
	
	new_(objectKey, CloudObjectKey(blob_db_id));
	push_(objectKey);
	
	s3Cloud = MSCloudInfo::getCloudInfo(key->cloud_ref);
	push_(s3Cloud);

	objectKey->setObjectKey(key);

	s3Cloud->cDelete(objectKey->getCString());	
	
	release_(s3Cloud);
	release_(objectKey);

	exit_();
}

//-------------------------------
CSString *CloudDB::cl_getDataURL(CloudKeyPtr key)
{
	CloudObjectKey *objectKey;
	CSString *url;
	MSCloudInfo *s3Cloud;
	enter_();
	
	new_(objectKey, CloudObjectKey(blob_db_id));
	push_(objectKey);
	
	objectKey->setObjectKey(key);
	
	s3Cloud = MSCloudInfo::getCloudInfo(key->cloud_ref);  
	push_(s3Cloud);
		
	url = s3Cloud->getDataURL(objectKey->getCString(), keep_alive);
	
	release_(s3Cloud);
	release_(objectKey);

	return_(url);
}

//-------------------------------
CSString *CloudDB::cl_getSignature(CloudKeyPtr key, CSString *content_type_arg, uint32_t *s3AuthorizationTime)
{
	CSString *signature;
	CloudObjectKey *objectKey;
	const char *content_type = NULL;
	MSCloudInfo *s3Cloud;
	enter_();
	
	new_(objectKey, CloudObjectKey(blob_db_id));
	push_(objectKey);
	
	if (content_type_arg) {
		push_(content_type_arg);
		content_type = content_type_arg->getCString();
	}
	
	objectKey->setObjectKey(key);
	s3Cloud = MSCloudInfo::getCloudInfo(key->cloud_ref);  
	push_(s3Cloud);
	
	signature = s3Cloud->getSignature(objectKey->getCString(), content_type, s3AuthorizationTime);
	
	if (content_type_arg) 
		release_(content_type_arg);

	release_(s3Cloud);
	release_(objectKey);
	
	return_(signature);
}

//==============================

