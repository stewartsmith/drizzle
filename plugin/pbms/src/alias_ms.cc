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
 * Barry Leslie
 *
 * 2008-12-30
 *
 * H&G2JCtL
 *
 * BLOB alias index.
 *
 */

#ifdef HAVE_ALIAS_SUPPORT
#include "cslib/CSConfig.h"

#include "string.h"

#ifdef DRIZZLED
#include <drizzled/common.h>
#endif

#include "cslib/CSGlobal.h"
#include "cslib/CSLog.h"
#include "cslib/CSStrUtil.h"
#include "cslib/CSFile.h"
#include "system_table_ms.h"
#include "database_ms.h"

#include "alias_ms.h"



//------------------------
MSAlias::~MSAlias()
{
	enter_();
	
	ASSERT(iClosing);
	ASSERT(iPoolSysTables.getSize() == 0);

	
	if (iFilePath) {
	
		if (iDelete)
			iFilePath->removeFile();
			
		iFilePath->release();
	}
	
	if (iFileShare)
		iFileShare->release();
		
	exit_();
}

//------------------------
MSAlias::MSAlias(MSDatabase *db_noref)
{
	iClosing = false;
	iDelete = false;
	iDatabase_br = db_noref;
	iFilePath = NULL;
	iFileShare = NULL;
}

//------------------------
void MSAlias::ma_close()
{
	enter_();
	
	iClosing = true;
	if (iFileShare)
		iFileShare->close();
	iPoolSysTables.clear();
	exit_();
}

//------------------------
// Compress the index bucket chain and free unused buckets.
void MSAlias::MSAliasCompress(CSFile *fa, CSSortedList	*freeList, MSABucketLinkedList *bucketChain)
{
	// For now I will just remove empty buckets. 
	// Later this function should also compress the records also 
	// thus making the searches faster and freeing up more space.
	MSABucketInfo *b_info, *next;
	
	b_info = bucketChain->getFront();
	while (b_info) {
		next = b_info->getNextLink();
		if (b_info->getSize() == 0) {
			bucketChain->remove(RETAIN(b_info));
			freeList->add(b_info);
		}		
		b_info = next;
	}
		
}

//------------------------
void MSAlias::MSAliasLoad()
{	
	CSFile			*fa = NULL;
	CSSortedList	freeList;
	off64_t			fileSize;

	enter_();
	
	fa = CSFile::newFile(RETAIN(iFilePath));
	push_(fa);

	MSAliasHeadRec header;
	uint64_t free_list_offset;
	fa->open(CSFile::DEFAULT);
	fa->read(&header, 0, sizeof(header), sizeof(header));
	
	/* Check the file header: */
	if (CS_GET_DISK_4(header.ah_magic_4) != MS_ALIAS_FILE_MAGIC)
		CSException::throwFileError(CS_CONTEXT, iFilePath->getCString(), CS_ERR_BAD_HEADER_MAGIC);
	if (CS_GET_DISK_2(header.ah_version_2) != MS_ALIAS_FILE_VERSION)
		CSException::throwFileError(CS_CONTEXT, iFilePath->getCString(), CS_ERR_VERSION_TOO_NEW);
		
	free_list_offset = CS_GET_DISK_8(header.ah_free_list_8);
	
	fileSize = CS_GET_DISK_8(header.ah_file_size_8);

	// Do some sanity checks. 
	if (CS_GET_DISK_2(header.ah_head_size_2) != sizeof(header))
		CSException::throwFileError(CS_CONTEXT, iFilePath->getCString(), CS_ERR_BAD_FILE_HEADER);
	
	if (CS_GET_DISK_2(header.ah_num_buckets_2) != BUCKET_LIST_SIZE)
		CSException::throwFileError(CS_CONTEXT, iFilePath->getCString(), CS_ERR_BAD_FILE_HEADER);
	
	if (CS_GET_DISK_4(header.ah_bucket_size_4) != NUM_RECORDS_PER_BUCKET)
		CSException::throwFileError(CS_CONTEXT, iFilePath->getCString(), CS_ERR_BAD_FILE_HEADER);
	
	if (fileSize != fa->getEOF()) 
		CSException::throwFileError(CS_CONTEXT, iFilePath->getCString(), CS_ERR_BAD_FILE_HEADER);
	
	// Load the bucket headers into RAM
	MSADiskBucketHeadRec bucketHead = {0};
	uint64_t offset, start_offset;

	// Fist load the free list:
	if (free_list_offset) {
		start_offset = offset = free_list_offset;
		do {
			fa->read(&bucketHead, offset, sizeof(MSADiskBucketHeadRec), sizeof(MSADiskBucketHeadRec));
			freeList.add(MSABucketInfo::newMSABucketInfo(offset));
			offset = CS_GET_DISK_8(bucketHead.ab_next_bucket_8);					
		} while (offset != start_offset);
		
	}
	for (uint32_t i = 0; i < BUCKET_LIST_SIZE; i++) {
		uint64_t used, total_space;
		MSABucketLinkedList *bucketChain = &(iFileShare->msa_buckets[i]);
		
		start_offset = offset = sizeof(header) + i * sizeof(MSADiskBucketRec);
		used = total_space = 0;
		do {
			uint32_t num, end_of_records;
			
			fa->read(&bucketHead, offset, sizeof(MSADiskBucketHeadRec), sizeof(MSADiskBucketHeadRec));
			num = CS_GET_DISK_4(bucketHead.ab_num_recs_4);
			end_of_records = CS_GET_DISK_4(bucketHead.ab_eor_rec_4);
			total_space += NUM_RECORDS_PER_BUCKET;
			used += num;
			bucketChain->addFront(MSABucketInfo::newMSABucketInfo(offset, num, end_of_records));
			offset = CS_GET_DISK_8(bucketHead.ab_next_bucket_8);
			
		} while (offset != start_offset);
		
		// Pack the index if required
		if (((total_space - used) /  NUM_RECORDS_PER_BUCKET) > 1) 
			MSAliasCompress(fa, &freeList, bucketChain); 
			
	}
	
	// If there are free buckets try to free up some disk
	// space or add them to a free list to be reused later.
	if (freeList.getSize()) {
		uint64_t last_bucket = fileSize - sizeof(MSADiskBucketRec);
		MSABucketInfo *rec;
		bool reduce = false;
		
		// Search for freed buckets at the end of the file
		// so that they can be released and the file
		// shrunk.
		//
		// The free list has been sorted so that buckets
		// with the highest file offset are first.
		do {
			rec = (MSABucketInfo*) freeList.itemAt(0);
			if (rec->bi_bucket_offset != last_bucket);
				break;
				
			last_bucket -= sizeof(MSADiskBucketRec);
			freeList.remove(rec);
			reduce = true;
		} while (freeList.getSize());
		
		if (reduce) {
			// The file can be reduced in size.
			fileSize = last_bucket + sizeof(MSADiskBucketRec);	
			fa->setEOF(fileSize);
			CS_SET_DISK_8(header.ah_file_size_8, fileSize);
			fa->write(&header.ah_file_size_8, offsetof(MSAliasHeadRec,ah_file_size_8) , 8); 
		}
		
		// Add the empty buckets to the index file's empty bucket list.
		memset(&bucketHead, 0, sizeof(bucketHead));
		offset = 0;
		while (freeList.getSize()) { // Add the empty buckets to the empty_bucket list.
			rec = (MSABucketInfo*) freeList.takeItemAt(0);
			
			// buckets are added to the front of the list.
			fa->write(&offset, rec->bi_bucket_offset + offsetof(MSADiskBucketHeadRec,ab_next_bucket_8) , 8);
			offset =  rec->bi_bucket_offset;
			fa->write(&offset, offsetof(MSAliasHeadRec,ah_free_list_8) , 8); 
			
			iFileShare->msa_empty_buckets.addFront(rec);
		}
	}
	
	iFileShare->msa_fileSize = fa->getEOF();
	
	release_(fa);
	exit_();
}

//------------------------
void MSAlias::buildAliasIndex()
{
	MSBlobHeadRec	blob;
	MSRepository	*repo;
	uint64_t			blob_size, fileSize, offset;
	uint16_t			head_size;
	MSAliasFile		*afile;
	MSAliasRec		aliasRec;
	
	enter_();
	
	afile = getAliasFile();
	frompool_(afile);

	afile->startLoad();

	CSSyncVector	*repo_list = iDatabase_br->getRepositoryList();
	
	// No locking is required since the index is loaded before the database is opened
	// and the compactor thread is started.

	for (uint32_t repo_index =0; repo_index<repo_list->size(); repo_index++) {
		if ((repo = (MSRepository *) repo_list->get(repo_index))) {
			MSRepoFile	*repoFile = repo->openRepoFile();
			push_(repoFile);
			fileSize = repo->getRepoFileSize();
			offset = repo->getRepoHeadSize();
			
			aliasRec.repo_id = repoFile->myRepo->getRepoID();
			
			while (offset < fileSize) {
				if (repoFile->read(&blob, offset, sizeof(MSBlobHeadRec), 0) < sizeof(MSBlobHeadRec)) 
					break;
					
				if ((CS_GET_DISK_1(blob.rb_status_1) == MS_BLOB_REFERENCED) && CS_GET_DISK_2(blob.rb_alias_offset_2)) {
					aliasRec.repo_offset = offset;
					aliasRec.alias_hash = CS_GET_DISK_4(blob.rb_alias_hash_4);
					addAlias(afile, &aliasRec);
				}
				
				head_size = CS_GET_DISK_2(blob.rb_head_size_2);
				blob_size = CS_GET_DISK_6(blob.rb_blob_repo_size_6);
				offset += head_size + blob_size;
			}
			
			release_(repoFile);
		}
	}
	
	afile->finishLoad();
	backtopool_(afile);

	exit_();
}

//------------------------
void MSAlias::MSAliasBuild()
{
	CSFile *fa;
	MSAliasHeadRec header = {0};
	uint64_t offset, size = sizeof(header) + BUCKET_LIST_SIZE * sizeof(MSADiskBucketRec);
	enter_();
	
	fa = CSFile::newFile(RETAIN(iFilePath));
	push_(fa);

	fa->open(CSFile::CREATE | CSFile::TRUNCATE);

	// Create an empty index with 1 empty bucket in each bucket chain.	

	CS_SET_DISK_4(header.ah_magic_4,  MS_ALIAS_FILE_MAGIC);
	CS_SET_DISK_2(header.ah_version_2, MS_ALIAS_FILE_VERSION);
		
	CS_SET_DISK_2(header.ah_head_size_2, sizeof(header));
	CS_SET_DISK_8(header.ah_file_size_8, size);
	
	CS_SET_DISK_2(header.ah_num_buckets_2, BUCKET_LIST_SIZE);
	CS_SET_DISK_2(header.ah_bucket_size_4, NUM_RECORDS_PER_BUCKET);
	
	fa->setEOF(size); // Grow the file.
	fa->write(&header, 0, sizeof(header));

	offset = sizeof(header);
	
	// Initialize the file bucket chains.
	MSADiskBucketHeadRec bucketHead = {0};
	for (uint32_t i = 0; i < BUCKET_LIST_SIZE; i++) {
		CS_SET_DISK_8(bucketHead.ab_prev_bucket_8, offset);
		CS_SET_DISK_8(bucketHead.ab_next_bucket_8, offset);
		fa->write(&bucketHead, offset, sizeof(MSADiskBucketHeadRec));
		// Add the bucket to the RAM based list.
		iFileShare->msa_buckets[i].addFront(MSABucketInfo::newMSABucketInfo(offset));
		offset += sizeof(MSADiskBucketRec); // NOTE: MSADiskBucketRec not MSADiskBucketHeadRec
	}
	
	fa->sync();
	
	
	
	fa->close();
	
	release_(fa);
	
	// Scan through all the BLOBs in the repository and add an entry
	// for each blob alias.
	buildAliasIndex();

	exit_();
}

//------------------------
void MSAlias::ma_open(const char *file_name)
{
	bool isdir = false;
	
	enter_();

	iFilePath = CSPath::newPath(RETAIN(iDatabase_br->myDatabasePath), file_name);
	
retry:
	new_(iFileShare, MSAliasFileShare(RETAIN(iFilePath)));
	
	if (iFilePath->exists(&isdir)) {
		try_(a) {
			MSAliasLoad(); 
		}
		catch_(a) {
			// If an error occurs delete the index and rebuild it.
			self->myException.log(NULL);
			iFileShare->release();
			iFilePath->removeFile();
			goto retry;
		}
		cont_(a);
	} else
		MSAliasBuild();
	
	
	exit_();
}

//------------------------
uint32_t MSAlias::hashAlias(const char *ptr)
{
	register uint32_t h = 0, g;
	
	while (*ptr) {
		h = (h << 4) + (uint32_t) toupper(*ptr++);
		if ((g = (h & 0xF0000000)))
			h = (h ^ (g >> 24)) ^ g;
	}

	return (h);
}

//------------------------
void MSAlias::addAlias(MSAliasFile *af, MSAliasRec *rec)
{
	MSDiskAliasRec diskRec;
	CS_SET_DISK_4(diskRec.ar_repo_id_4, rec->repo_id);	
	CS_SET_DISK_8(diskRec.ar_offset_8, rec->repo_offset);	
	CS_SET_DISK_4(diskRec.ar_hash_4, rec->alias_hash);
	af->addRec(&diskRec);

}

//------------------------
uint32_t MSAlias::addAlias(uint32_t repo_id, uint64_t repo_offset, const char *alias)
{
	MSDiskAliasRec diskRec;
	uint32_t hash;
	uint32_t f_repo_id;
	uint64_t f_repo_offset;
	bool referenced = false;
	enter_();
	
	hash = hashAlias(alias);
	
	// Use a lock to make sure that the same alias cannot be added at the same time.
	lock_(this);
	
	MSAliasFile *af = getAliasFile();
	frompool_(af);

	if (findBlobByAlias(RETAIN(af), alias, &referenced, &f_repo_id, &f_repo_offset)) {
		if ((f_repo_id == repo_id) && (f_repo_offset == repo_offset))
			goto done; // Do not treat this as an error.
		if (!referenced) {
			// If the alias is in use by a non referenced BLOB then delete it.
			// This can happen because I allow newly created BLOBs to be accessed
			// by their alias even before a reference to the BLOB has been added to
			// the database.
			af->deleteCurrentRec();
		} else	{
#ifdef xxDEBUG
			CSL.log(self, CSLog::Protocol, "Alias: ");
			CSL.log(self, CSLog::Protocol, alias);
			CSL.log(self, CSLog::Protocol, "\n");
#endif
			CSException::throwException(CS_CONTEXT, MS_ERR_DUPLICATE, "Alias Exists");
		}
	}
		
	CS_SET_DISK_4(diskRec.ar_repo_id_4, repo_id);	
	CS_SET_DISK_8(diskRec.ar_offset_8, repo_offset);	
	CS_SET_DISK_4(diskRec.ar_hash_4, hash);	

	af->addRec(&diskRec);
done:
	backtopool_(af);

	unlock_(this);
	return_(hash);
}

//------------------------
void MSAlias::deleteAlias(MSDiskAliasPtr diskRec)
{
	enter_();
	
	MSAliasFile *af = getAliasFile();
	frompool_(af);
	if (af->findRec(diskRec))
		af->deleteCurrentRec();
	backtopool_(af);

	exit_();
}

//------------------------
void MSAlias::deleteAlias(uint32_t repo_id, uint64_t repo_offset, uint32_t alias_hash)
{
	MSDiskAliasRec diskRec;
	
	CS_SET_DISK_4(diskRec.ar_repo_id_4, repo_id);	
	CS_SET_DISK_8(diskRec.ar_offset_8, repo_offset);	
	CS_SET_DISK_4(diskRec.ar_hash_4, alias_hash);	
	deleteAlias(&diskRec);
	
}
//------------------------
void MSAlias::resetAlias(uint32_t old_repo_id, uint64_t old_repo_offset, uint32_t alias_hash, uint32_t new_repo_id, uint64_t new_repo_offset)
{
	MSDiskAliasRec diskRec;
	bool found;
	enter_();
	
	CS_SET_DISK_4(diskRec.ar_repo_id_4, old_repo_id);	
	CS_SET_DISK_8(diskRec.ar_offset_8, old_repo_offset);	
	CS_SET_DISK_4(diskRec.ar_hash_4, alias_hash);	

	lock_(this);
	
	MSAliasFile *af = getAliasFile();
	frompool_(af);
	found = af->findRec(&diskRec);
	CS_SET_DISK_4(diskRec.ar_repo_id_4, new_repo_id);	
	CS_SET_DISK_8(diskRec.ar_offset_8, new_repo_offset);	

	if (found) 
		af->updateCurrentRec(&diskRec);
	else {
		CSException::logException(CS_CONTEXT, MS_ERR_NOT_FOUND, "Alias doesn't exists");
		af->addRec(&diskRec);
	}
			
	backtopool_(af);

	unlock_(this);
	exit_();
}

//------------------------
// Check to see if the blob with the given repo_id
// and repo_offset has the specified alias.
bool MSAlias::hasBlobAlias(uint32_t repo_id, uint64_t repo_offset, const char *alias, bool *referenced)
{
	bool found = false;
	MSRepoFile *repoFile;
	MSBlobHeadRec	blob;
	uint8_t status;
	uint64_t offset;
	uint32_t alias_size = strlen(alias) +1;
	char blob_alias[BLOB_ALIAS_LENGTH +1];
	
	if (alias_size > BLOB_ALIAS_LENGTH)
		return false;

	enter_();
	
	repoFile = iDatabase_br->getRepoFileFromPool(repo_id, false);
	frompool_(repoFile);

	repoFile->read(&blob, repo_offset, sizeof(MSBlobHeadRec), sizeof(MSBlobHeadRec));
	status = CS_GET_DISK_1(blob.rb_status_1);
	if (IN_USE_BLOB_STATUS(status)) {
		offset = repo_offset + CS_GET_DISK_2(blob.rb_alias_offset_2);
		
		blob_alias[BLOB_ALIAS_LENGTH] = 0;
		if (repoFile->read(blob_alias, offset, alias_size, 0) == alias_size) {
			found = !my_strcasecmp(&my_charset_utf8_general_ci, blob_alias, alias);
			if (found)
				*referenced = (status == MS_BLOB_REFERENCED);
		}
	} else {
		CSException::logException(CS_CONTEXT, MS_ERR_ENGINE, "Deleted BLOB alias found. (Rebuild BLOB alias index.)");
	}
	
		
	backtopool_(repoFile);	

	return_(found);
}

//------------------------
bool MSAlias::findBlobByAlias( MSAliasFile *af, const char *alias, bool *referenced, uint32_t *repo_id, uint64_t *repo_offset)
{
	bool found = false;
	uint32_t hash, l_repo_id, l_repo_offset;
	MSDiskAliasPtr diskRec;
	enter_();

	push_(af);
	
	hash = hashAlias(alias);
	diskRec = af->findRec(hash);
	
	while (diskRec && !found) {
		l_repo_id = CS_GET_DISK_4(diskRec->ar_repo_id_4);
		l_repo_offset = CS_GET_DISK_8(diskRec->ar_offset_8);
		if (hasBlobAlias(l_repo_id, l_repo_offset, alias, referenced))
			found = true;
		else
			diskRec = af->nextRec();
	}
		
	if (found) {
		if (repo_id)
			*repo_id = l_repo_id;
			
		if (repo_offset)
			*repo_offset = l_repo_offset;
	}
	
	release_(af);
	return_(found);
}
//------------------------
bool MSAlias::findBlobByAlias( const char *alias, bool *referenced, uint32_t *repo_id, uint64_t *repo_offset)
{
	bool found;
	enter_();
	
	MSAliasFile *af = getAliasFile();
	frompool_(af);
	
	found = findBlobByAlias(RETAIN(af), alias, referenced, repo_id, repo_offset);

	backtopool_(af);
	return_(found);
}

//------------------------
bool MSAlias::blobAliasExists(uint32_t repo_id, uint64_t repo_offset, uint32_t alias_hash)
{
	bool found;
	MSDiskAliasRec diskRec;
	
	CS_SET_DISK_4(diskRec.ar_repo_id_4, repo_id);	
	CS_SET_DISK_8(diskRec.ar_offset_8, repo_offset);	
	CS_SET_DISK_4(diskRec.ar_hash_4, alias_hash);	

	enter_();
	
	MSAliasFile *af = getAliasFile();
	frompool_(af);
	
	found = af->findRec(&diskRec);

	backtopool_(af);
	return_(found);
}

/////////////////////////////////////
MSSysMeta::MSSysMeta(MSAlias *msa)
{
	md_myMSAlias = msa;
	md_isFileInUse = false;
	md_NextLink = md_PrevLink = NULL;
	
	mtab = MSMetaDataTable::newMSMetaDataTable(RETAIN(msa->iDatabase_br));
}

//------------------------
MSSysMeta::~MSSysMeta()
{
	if (mtab)
		mtab->release();

	if (md_myMSAlias)
		md_myMSAlias->release();
}

//------------------------
void MSSysMeta::returnToPool()
{
	enter_();
	push_(this);
	
		
	md_isFileInUse = false;
		
	if (!md_myMSAlias->iClosing) {
		lock_(&md_myMSAlias->iSysTablePoolLock); // It may be better if the pool had it's own lock.
		md_nextFile = md_myMSAlias->iSysTablePool;
		md_myMSAlias->iSysTablePool - this;
		unlock_(&md_myMSAlias->iSysTablePoolLock);
	}
		
	release_(this);
	exit_();
}
//------------------------
bool MSSysMeta::matchAlias(uint32_t repo_id, uint64_t repo_offset, const char *alias)
{
	mtab->seqScanInit(); 
	return mtab->matchAlias(repo_id, repo_offset, alias); 
}

/////////////////////////////////////
/////////////////////////////////////
MSAliasFile::MSAliasFile(MSAliasFileShare *share)
{
	ba_share = share;
	ba_isFileInUse = false;
	ba_NextLink = ba_PrevLink = NULL;
	
	iCurrentRec = 0;
	iBucketCache = NULL;
	iStartBucket = iCurrentBucket = NULL;
	iBucketChain = NULL;
	iLoading = false;
	ba_nextFile = NULL;
	
	iFile = CSFile::newFile(RETAIN(ba_share->msa_filePath));	
	iFile->open(CSFile::DEFAULT);
	
	
}

//------------------------
MSAliasFile::~MSAliasFile()
{
	if (iFile)
		iFile->release();
		
	if (iBucketCache)
		cs_free(iBucketCache);
}

//------------------------
void MSAliasFile::startLoad()
{
	enter_();
	
	ASSERT(!iLoading);
	
//	iBucketCache = (MSADiskBucketRec*) cs_malloc(BUCKET_LIST_SIZE * sizeof(MSADiskBucketRec));
//	memset(iBucketCache, 0, BUCKET_LIST_SIZE * sizeof(MSADiskBucketRec));
	iLoading = true;
	
	exit_();
}

//------------------------
void MSAliasFile::finishLoad()
{
	enter_();
	ASSERT(iLoading);
	// Write the bucket cache to disk.
//	for (iCurrentBucket && iCurrentBucket->getSize()) {
		// To Be implemented.
//	}
//	cs_free(iBucketCache);
	iBucketCache = NULL;
	iLoading = false;
	exit_();
}

//------------------------
void MSAliasFile::returnToPool()
{
	enter_();
	push_(this);
	
	if (iLoading) {
		// If iLoading is still set then probably an exception has been thrown.
		try_(a) {
			finishLoad();
		}
		catch_(a) 
			iLoading = false;
		cont_(a);
	}

	ba_isFileInUse = false;
		
	if (!ba_share->msa_closing) {
		lock_(&ba_share->msa_poolLock);
		ba_nextFile = ba_share->msa_pool;
		ba_share->msa_pool = this;
		unlock_(&ba_share->msa_poolLock);
	}
		
	release_(this);
	exit_();
}

//------------------------
// The bucket chain is treated as a circular list.
bool MSAliasFile::nextBucket(bool with_space)
{
	bool have_bucket = false;
	enter_();
	
	while (!have_bucket){
		if (iCurrentBucket) {
			iCurrentBucket = iCurrentBucket->getNextLink();
			if (!iCurrentBucket)
				iCurrentBucket = iBucketChain->getFront();
			if (iCurrentBucket == iStartBucket)
				break;
		} else {
			iCurrentBucket = iBucketChain->getFront();
			iStartBucket = iCurrentBucket;
		}
		
		if ((iCurrentBucket->getSize() && !with_space) || (with_space && (iCurrentBucket->getSize() < NUM_RECORDS_PER_BUCKET))){
			// Only read the portion of the bucket containing records.
			iCurrentRec = iCurrentBucket->getEndOfRecords(); // The current record is set just beyond the last valid record.
			size_t size = iCurrentRec * sizeof(MSDiskAliasRec);		
			iFile->read(iBucket, iCurrentBucket->bi_records_offset, size, size);			
			have_bucket = true;
		}
	}
	
	return_(have_bucket);
}

//------------------------
MSDiskAliasPtr MSAliasFile::nextRec()
{
	MSDiskAliasPtr rec = NULL;
	bool have_rec;
	enter_();
	
	while ((!(have_rec = scanBucket())) && nextBucket(false));
	
	if (have_rec) 
		rec = &(iBucket[iCurrentRec]);
		
	return_(rec);
}

//------------------------
// When starting a search:
// If a bucket is already loaded and it is in the correct bucket chain
// then search it first. In this case then the search starts at the current
// bucket in the chain.
//
// Searches are from back to front with the idea that the more recently
// added objects will be seached for more often and they are more likely
// to be at the end of the chain.
MSDiskAliasPtr MSAliasFile::findRec(uint32_t hash)
{
	MSDiskAliasPtr rec = NULL;
	MSABucketLinkedList *list = ba_share->getBucketChain(hash);
	enter_();
	
	CS_SET_DISK_4(iDiskHash_4, hash);
	if (list == iBucketChain) {
		// The search is performed back to front.
		iCurrentRec = iCurrentBucket->getEndOfRecords();  // Position the start just beyond the last valid record.
		iStartBucket = iCurrentBucket;
		if (scanBucket()) {
			rec = &(iBucket[iCurrentRec]);
			goto done;
		}
	} else {
		iBucketChain = list;
		iCurrentBucket = NULL;
		iStartBucket = NULL;
	}

	if (nextBucket(false))
		rec = nextRec();
		
done:
	return_(rec);
}

//------------------------
bool MSAliasFile::findRec(MSDiskAliasPtr theRec)
{
	MSDiskAliasPtr aRec = NULL;
	bool found = false;
	enter_();
	
	aRec = findRec(CS_GET_DISK_4(theRec->ar_hash_4));
	while ( aRec && !found) {
		if (CS_EQ_DISK_4(aRec->ar_repo_id_4, theRec->ar_repo_id_4) && CS_EQ_DISK_8(aRec->ar_offset_8, theRec->ar_offset_8))
			found = true;
		else
			aRec = nextRec();
	}	
	return_(found);
}

//------------------------
void MSAliasFile::addRec(MSDiskAliasPtr new_rec)
{
	MSABucketLinkedList *list = ba_share->getBucketChain(CS_GET_DISK_4(new_rec->ar_hash_4));
	enter_();
	lock_(&ba_share->msa_writeLock);

	if (iBucketChain != list) {
		iBucketChain = list;
		iCurrentBucket = NULL;
		iStartBucket = NULL;
	} else 
		iStartBucket = iCurrentBucket;

	if ((iCurrentBucket && (iCurrentBucket->getSize() < NUM_RECORDS_PER_BUCKET)) || nextBucket(true)) { // Find a bucket with space in it for a record.
		uint32_t size = iCurrentBucket->getSize();
		uint32_t end_of_records = iCurrentBucket->getEndOfRecords();

		if (size == end_of_records) { // No holes in the recored list
			iCurrentRec = end_of_records;			
		} else { // Search for the empty record
			iCurrentRec = end_of_records -2;			
			while (iCurrentRec && !CS_IS_NULL_DISK_4(iBucket[iCurrentRec].ar_repo_id_4))
				iCurrentRec--;
				
			ASSERT(CS_IS_NULL_DISK_4(iBucket[iCurrentRec].ar_repo_id_4));
		}
		
		memcpy(&iBucket[iCurrentRec], new_rec, sizeof(MSDiskAliasRec)); // Add the record to the cached bucket.
		
		iCurrentBucket->recAdded(iFile, iCurrentRec); // update the current bucket header.
	} else { // A new bucket must be added to the chain.
		MSADiskBucketHeadRec new_bucket = {0};
		CSDiskValue8 disk_8_value;
		uint64_t new_bucket_offset;
		MSABucketInfo *next, *prev;
		
		next = iBucketChain->getFront();
		prev = iBucketChain->getBack();
		
		// Set the next and prev bucket offsets in the new bucket record.
		CS_SET_DISK_8(new_bucket.ab_prev_bucket_8, prev->bi_bucket_offset);
		CS_SET_DISK_8(new_bucket.ab_next_bucket_8, next->bi_bucket_offset);
		
		if (ba_share->msa_empty_buckets.getSize()) { // Get a bucket from the empty bucket list.
			MSABucketInfo *empty_bucket = ba_share->msa_empty_buckets.removeFront();
			
			new_bucket_offset = empty_bucket->bi_bucket_offset;			
			empty_bucket->release();
			
			// Update the index file's empty bucket list 
			if (ba_share->msa_empty_buckets.getSize() == 0) 
				CS_SET_NULL_DISK_8(disk_8_value);
			else
				CS_SET_DISK_8(disk_8_value, iBucketChain->getFront()->bi_bucket_offset);
			
			iFile->write(&disk_8_value, offsetof(MSAliasHeadRec,ah_free_list_8) , 8); 
		} else // There are no empty buckets so grow the file.
			new_bucket_offset = ba_share->msa_fileSize;
			
		// Write the new bucket's record header to the file
		iFile->write(&new_bucket, new_bucket_offset, sizeof(MSADiskBucketHeadRec)); 
		
		// Insert the new bucket into the bucket chain on the disk.
		CS_SET_DISK_8(disk_8_value, new_bucket_offset);
		iFile->write(&disk_8_value, prev->bi_bucket_offset +  offsetof(MSADiskBucketHeadRec,ab_next_bucket_8), 8); 
		iFile->write(&disk_8_value, next->bi_bucket_offset +  offsetof(MSADiskBucketHeadRec,ab_prev_bucket_8), 8); 
		
		// Update the file size in the file header if required
		if (ba_share->msa_fileSize == new_bucket_offset) {
			ba_share->msa_fileSize += sizeof(MSADiskBucketRec); // Note this is MSADiskBucketRec not MSADiskBucketHeadRec

			CS_SET_DISK_8(disk_8_value, ba_share->msa_fileSize);
			iFile->write(&disk_8_value, offsetof(MSAliasHeadRec,ah_file_size_8) , 8); 
		}
		
		// Add the info rec into the bucket chain in RAM.
		iCurrentBucket = MSABucketInfo::newMSABucketInfo(new_bucket_offset, 1, 0);
		iBucketChain->addFront(iCurrentBucket);
		iCurrentRec = 0;
	}
	
	uint64_t offset;
	offset = iCurrentBucket->bi_records_offset + iCurrentRec * sizeof(MSDiskAliasRec);

	// Write the new index entry to the index file.
	iFile->write(new_rec, offset, sizeof(MSDiskAliasRec)); 
	
	unlock_(&ba_share->msa_writeLock);
	
	exit_();	
}
//------------------------
void MSAliasFile::deleteCurrentRec()
{
	MSDiskAliasPtr rec = &(iBucket[iCurrentRec]);
	uint64_t	offset;
	enter_();
	
	CS_SET_NULL_DISK_4(rec->ar_repo_id_4);
	offset = iCurrentBucket->bi_records_offset + iCurrentRec * sizeof(MSDiskAliasRec);
	
	lock_(&ba_share->msa_writeLock);

	// Update the index file. It is assumed that repo_id is the first 4 bytes of 'rec'.
	iFile->write(rec, offset, 4); 
	
	iCurrentBucket->recRemoved(iFile, iCurrentRec, iBucket);
	
	unlock_(&ba_share->msa_writeLock);
	
	exit_();	
}

//------------------------
void MSAliasFile::updateCurrentRec(MSDiskAliasPtr update_rec)
{
	uint64_t	offset;
	enter_();
	
	// ASSERT that the updated rec still belongs to this bucket chain.
	ASSERT(ba_share->getBucketChain(CS_GET_DISK_4(update_rec->ar_hash_4)) == iBucketChain);
	ASSERT(!CS_IS_NULL_DISK_4(iBucket[iCurrentRec].ar_repo_id_4)); // We should not be updating a deleted record.
	
	lock_(&ba_share->msa_writeLock);
	offset = iCurrentBucket->bi_records_offset + iCurrentRec * sizeof(MSDiskAliasRec);
	
	// Update the record on disk.
	iFile->write(update_rec, offset, sizeof(MSDiskAliasRec));
	
	// Update the record in memory. 
	CS_COPY_DISK_4(iBucket[iCurrentRec].ar_repo_id_4, update_rec->ar_repo_id_4);	
	CS_COPY_DISK_8(iBucket[iCurrentRec].ar_offset_8, update_rec->ar_offset_8);	

	unlock_(&ba_share->msa_writeLock);
	exit_();	
}


//------------------------
MSABucketInfo *MSABucketInfo::newMSABucketInfo(uint64_t offset, uint32_t num, uint32_t last)
{
	MSABucketInfo *bucket;
	new_(bucket, MSABucketInfo(offset, num, last));
	return bucket;
}
//------------------------
void MSABucketInfo::recRemoved(CSFile *iFile, uint32_t idx, MSDiskAliasRec bucket[])
{
	MSADiskBucketHeadRec head;
	enter_();
	
	ASSERT(idx < bi_end_of_records);
	
	bi_num_recs--;
	if (!bi_num_recs) {
		// It would be nice to remove this bucket from the 
		// bucket list and place it on the empty list.
		// Before this can be done a locking method would
		// be needed to block anyone from reading this
		// bucket while it was being moved.
		//
		// I haven't done this because I have been trying
		// to avoid read locks.
		bi_end_of_records = 0;
	} else if ((bi_end_of_records -1) == idx) {
		while (idx && CS_IS_NULL_DISK_4(bucket[idx].ar_repo_id_4))
			idx--;
			
		if ((idx ==0) && CS_IS_NULL_DISK_4(bucket[0].ar_repo_id_4))
			bi_end_of_records = 0;
		else
			bi_end_of_records = idx +1;
		
		ASSERT(bi_end_of_records >= bi_num_recs);
	}
	
	// Update the index file.
	CS_SET_DISK_4(head.ab_num_recs_4, bi_num_recs);
	CS_SET_DISK_4(head.ab_eor_rec_4, bi_end_of_records);
	iFile->write(&head.ab_num_recs_4, bi_bucket_offset +  offsetof(MSADiskBucketHeadRec,ab_num_recs_4), 8); 
	exit_();
}

//------------------------
void MSABucketInfo::recAdded(CSFile *iFile, uint32_t idx)
{
	MSADiskBucketHeadRec head;
	enter_();
	
	ASSERT(bi_num_recs < NUM_RECORDS_PER_BUCKET);
	ASSERT(idx < NUM_RECORDS_PER_BUCKET);

	bi_num_recs++;
	if (idx == bi_end_of_records)
		bi_end_of_records++;
		
	// Update the index file.
	CS_SET_DISK_4(head.ab_num_recs_4, bi_num_recs);
	CS_SET_DISK_4(head.ab_eor_rec_4, bi_end_of_records);
	iFile->write(&head.ab_num_recs_4, bi_bucket_offset +  offsetof(MSADiskBucketHeadRec,ab_num_recs_4), 8); 
	exit_();	
}

//////////////////////////////////
MSAliasFile *MSAliasFileShare::getPoolFile()
{
	MSAliasFile *af;
	enter_();
	
	lock_(&msa_poolLock); 
	if ((af = msa_pool)) {
		msa_pool = af->ba_nextFile;
	} else {
		new_(af, MSAliasFile(this));
		msa_poolFiles.addFront(af);
	}
	unlock_(&msa_poolLock);
	
	af->ba_nextFile = NULL;
	ASSERT(!af->ba_isFileInUse);
	af->ba_isFileInUse = true;
	af->retain();
	
	return_(af);
}
#endif // HAVE_ALIAS_SUPPORT

