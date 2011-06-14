#pragma once
#ifndef __CLOUD_H__
#define __CLOUD_H__
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
#include "cslib/CSMd5.h"
#include <inttypes.h>


/* NOTES:
 *
 * - TODO: If cl_deleteData() fails then the BLOB deletion must fail and be rescheduled to try again
 *			later.
 * - TODO: Copying of BLOBs from one database to another needs to be handled. Look for copyBlob() and 
 *			resetBlobHead(). There are 3 cases to handle depending on if the databases involved use
 *			cload storage.
 */
 
//===============================
class CSS3Protocol;	
class MSCloudInfo : public CSRefObject {
	private:
	static uint32_t	gMaxInfoRef;
	static CSSyncSparseArray *gCloudInfo;

	friend class MSCloudTable;
	friend class CloudDB;
	
	private:	
	uint32_t			cloudRefId;
	CSString		*bucket;
	CSS3Protocol	*s3Prot;
	
public:

	static void startUp()
	{
		new_(gCloudInfo, CSSyncSparseArray(5));
		gMaxInfoRef = 0;
	}
	
	static void shutDown()
	{
		if (gCloudInfo) {
			gCloudInfo->clear();
			gCloudInfo->release();
			gCloudInfo = NULL;
		}	
	}


	static MSCloudInfo *getCloudInfo(uint32_t in_cloudRefId)
	{
		MSCloudInfo *info;
		enter_();
		
		lock_(gCloudInfo);
		
		info = (MSCloudInfo *) gCloudInfo->get(in_cloudRefId);
		if (!info) {
			char msg[80];
			snprintf(msg, 80, "Cloud info with reference ID %"PRIu32" not found", in_cloudRefId);
			CSException::throwException(CS_CONTEXT, CS_ERR_GENERIC_ERROR, msg);
		}
		info->retain();
		unlock_(gCloudInfo);
		return_(info);
	}
	
	MSCloudInfo(uint32_t id, const char *server, const char *bucket, const char *publicKey, const char *privateKey );
	~MSCloudInfo();
	
	uint32_t getCloudRefId() { return cloudRefId;}
	
	const char *getServer();
	
	const char *getBucket();
	
	const char *getPublicKey();
	
	const char *getPrivateKey();

	CSString *getSignature(const char *key, const char *content_type, uint32_t *s3AuthorizationTime);
	
	CSString *getDataURL(const char *key, int keep_alive);

	void send(CSInputStream *input, const char *key, off64_t size);
	
	void receive(CSOutputStream *output, const char *key);
	
	void copy(MSCloudInfo *dst_cloud, const char *dst_key, const char *src_key);

	void cDelete(const char *key);
	
	CSVector *list(const char *key_prefix, uint32_t max = 0);
};

typedef struct CloudKey {
	uint32_t creation_time;
	uint32_t ref_index;		// Just a sequence counter in case 2 blobs have the same creation time.
	uint32_t cloud_ref;		// A reference into the pbms.pbms_cloud table.
} CloudKeyRec, *CloudKeyPtr;


//===============================
class CloudObjectKey : public CSStringBuffer
{
	uint32_t default_db_id;
	
	public:
	CloudObjectKey(uint32_t id): CSStringBuffer(), default_db_id(id){ }
	~CloudObjectKey(){}
	
	static const uint32_t base_key_size = 64; // enough space for <db_id>/<backup_id>/<creation_time>/<ref_index>

	void setObjectKey(const char *object_key)
	{
		setLength(base_key_size + strlen(object_key) +1);
		
		snprintf(getBuffer(0), length(), "%"PRIu32"/0/%s",default_db_id,  object_key);
	}
	
	void setObjectKey(CloudKeyPtr key = NULL, uint32_t backup_id = 0, uint32_t db_id = 0)
	{
		if (!db_id) db_id = default_db_id;
		setLength(base_key_size);
		
		if (key)
			snprintf(getBuffer(0), length(), "%"PRIu32"/%"PRIu32"/%"PRIu32".%"PRIu32".%"PRIu32"", db_id, backup_id, key->cloud_ref, key->creation_time, key->ref_index);
		else 
			snprintf(getBuffer(0), length(), "%"PRIu32"/%"PRIu32"s/", db_id, backup_id);
			
	}
	
	static void parseObjectKey(const char *object_key, CloudKeyPtr key, uint32_t *backup_id = NULL, uint32_t *db_id = NULL)
	{
		uint32_t v1;
		
		if (!backup_id) backup_id = &v1;
		if (!db_id) db_id = &v1;
		
		sscanf(object_key, "%"PRIu32"/%"PRIu32"/%"PRIu32".%"PRIu32".%"PRIu32"", db_id, backup_id, &(key->cloud_ref), &(key->creation_time), &(key->ref_index));
	}
};

//===============================
class MSBackupInfo;
class CloudDB: public CSRefObject {
	
private:
	static uint32_t	gKeyIndex;
	static CSMutex	gCloudKeyLock;
	
	uint32_t	dfltCloudRefId;
	
	uint32_t	keep_alive; // The length of time a redirect URL will remain valid. In seconds.
	uint32_t	blob_recovery_no; // This is the backup number from which the recovery should be done.
	uint32_t	blob_db_id;
	
	bool isBackup;
	MSBackupInfo *backupInfo;
	MSCloudInfo	*backupCloud;
	
	static const uint32_t base_key_size = 64; // enough space for <db_id>/<backup_id>/<creation_time>/<ref_index>

public:
	CSStringBuffer		*clObjectKey;
	
	CloudDB(uint32_t db_id);
	~CloudDB();
	
	void cl_setDefaultCloudRef(uint32_t dflt) { dfltCloudRefId = dflt;}
	uint32_t cl_getDefaultCloudRef() { return dfltCloudRefId;}

	MSCloudInfo *cl_getCloudInfo(uint32_t cloudRefId = 0)
	{
		return MSCloudInfo::getCloudInfo((cloudRefId)?cloudRefId:dfltCloudRefId);
	}
	
	void cl_getNewKey(CloudKeyPtr key)
	{
		enter_();
		lock_(&gCloudKeyLock);	
		
		key->creation_time = time(NULL);
		key->ref_index = gKeyIndex++;
		key->cloud_ref = dfltCloudRefId;
		
		unlock_(&gCloudKeyLock);	
		exit_();
	}

	bool cl_mustRecoverBlobs() { return (blob_recovery_no != 0);}
	
	void cl_setRecoveryNumber(const char *number)
	{
		blob_recovery_no = atol(number);
	}

	const char *cl_getRecoveryNumber()
	{
		static char number[20];
		
		snprintf(number, 20, "%"PRIu32"", blob_recovery_no);
		return number;
	}

	CSString *cl_getObjectKey(CloudKeyPtr key)
	{
		CloudObjectKey *objectKey;
		enter_();
		
		new_(objectKey, CloudObjectKey(blob_db_id));
		push_(objectKey);
		
		objectKey->setObjectKey(key);
		
		CSString *str = CSString::newString(objectKey->getCString());
		release_(objectKey);
		
		return_(str);
	}
	
	void cl_setKeepAlive(uint32_t keep_alive_arg) {keep_alive = keep_alive_arg;}
	
	void cl_createDB();
	void cl_dropDB();
	void cl_restoreDB();
	uint32_t cl_getNextBackupNumber(uint32_t cloud_ref = 0);
	bool cl_dbExists();
	
	// setting backup_blob_no to -1 ensures that if the database is dropped no BLOBs will be deleted.
	void cl_setCloudIsBackup(){ isBackup = true;}
	void cl_setBackupInfo(MSBackupInfo *info){ backupInfo = info;}
	MSBackupInfo *cl_getBackupInfo();
	void cl_clearBackupInfo();
	
	void cl_backupBLOB(CloudKeyPtr key);
	void cl_restoreBLOB(CloudKeyPtr key, uint32_t backup_db_id);

	void cl_putData( CloudKeyPtr key, CSInputStream *stream, off64_t size);
	off64_t cl_getData(CloudKeyPtr key, char *data, off64_t size);
	CSString *cl_getDataURL(CloudKeyPtr key);
	void cl_deleteData(CloudKeyPtr key);
	CSString *cl_getSignature(CloudKeyPtr key, CSString *content_type, uint32_t *s3AuthorizationTime);
	
};

#endif // __CLOUD_H__
