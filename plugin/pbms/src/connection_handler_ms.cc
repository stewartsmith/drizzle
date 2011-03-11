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
 * Original author: Paul McCullagh
 * Continued development: Barry Leslie
 *
 * 2007-05-25
 *
 * H&G2JCtL
 *
 * Network interface.
 *
 */

#include "cslib/CSConfig.h"
#include <inttypes.h>

#include "defs_ms.h"

#include "cslib/CSGlobal.h"
#include "cslib/CSSocket.h"
#include "cslib/CSStrUtil.h"
#include "cslib/CSHTTPStream.h"

#include "connection_handler_ms.h"
#include "network_ms.h"
#include "open_table_ms.h"
#include "engine_ms.h"
#include "version_ms.h"

//#include "mysql_ms.h"

u_long MSConnectionHandler::gMaxKeepAlive;

MSConnectionHandler::MSConnectionHandler(CSThreadList *list):
	CSDaemon(list),
	amWaitingToListen(false),
	shuttingDown(false),
	lastUse(0),
	replyPending(false),
	iInputStream(NULL),
	iOutputStream(NULL),
	iTableURI(NULL)
{
}

void MSConnectionHandler::close()
{
	closeStream();
	freeRequestURI();
}

MSConnectionHandler *MSConnectionHandler::newHandler(CSThreadList *list)
{
	return new MSConnectionHandler(list);
}

/* Return false if not connection was openned, and the thread must quit. */
bool MSConnectionHandler::openStream()
{
	CSSocket		*sock;
	CSInputStream	*in;
	CSOutputStream	*out;
	
	enter_();
	if (!(sock = MSNetwork::openConnection(this)))
		return_(false);
	push_(sock);
	in = sock->getInputStream();
	in = CSBufferedInputStream::newStream(in);
	iInputStream = CSHTTPInputStream::newStream(in);

	out = sock->getOutputStream();
	out = CSBufferedOutputStream::newStream(out);
	iOutputStream = CSHTTPOutputStream::newStream(out);
	release_(sock);
	return_(true);
}

int MSConnectionHandler::getHTTPStatus(int err)
{
	int code;

	switch (err) {
		case MS_OK:						code = 200; break;
		case MS_ERR_ENGINE:				code = 500; break;
		case MS_ERR_UNKNOWN_TABLE:		code = 404; break;
		case MS_ERR_UNKNOWN_DB:			code = 404; break;
		case MS_ERR_DATABASE_DELETED:	code = 404; break;
		case MS_ERR_NOT_FOUND:			code = 404; break;
		case MS_ERR_REMOVING_REPO:		code = 404; break;
		case MS_ERR_TABLE_LOCKED:		code = 412; break; // Precondition Failed
		case MS_ERR_INCORRECT_URL:		code = 404; break;
		case MS_ERR_AUTH_FAILED:		code = 403; break; // Forbidden
		default:						code = 500; break;
	}
	return code;
}

void MSConnectionHandler::writeException(const char *qualifier)
{
	int code;

	enter_();
	iOutputStream->clearHeaders();
	iOutputStream->clearBody();
	code = getHTTPStatus(myException.getErrorCode());
	iOutputStream->setStatus(code);
	iOutputStream->appendBody("<HTML><HEAD><TITLE>HTTP Error ");
	iOutputStream->appendBody(code);
	iOutputStream->appendBody(": ");
	iOutputStream->appendBody(CSHTTPOutputStream::getReasonPhrase(code));
	iOutputStream->appendBody("</TITLE></HEAD>");
	iOutputStream->appendBody("<BODY><H2>HTTP Error ");
	iOutputStream->appendBody(code);
	iOutputStream->appendBody(": ");
	iOutputStream->appendBody(CSHTTPOutputStream::getReasonPhrase(code));
	iOutputStream->appendBody("</H2>");
	if (qualifier)
		iOutputStream->appendBody(qualifier);
	iOutputStream->appendBody(EXCEPTION_REPLY_MESSAGE_PREFIX_TAG);
	iOutputStream->appendBody(myException.getMessage());
	iOutputStream->appendBody(EXCEPTION_REPLY_MESSAGE_SUFFIX_TAG);
	iOutputStream->appendBody(myException.getStackTrace());
	iOutputStream->appendBody(EXCEPTION_REPLY_STACK_TRACE_SUFFIX_TAG);
	iOutputStream->appendBody("MySQL ");
	iOutputStream->appendBody(PBMSVersion::getCString());
	iOutputStream->appendBody(", PBMS ");
	iOutputStream->appendBody(PBMSVersion::getCString());
	iOutputStream->appendBody("<br>Copyright &#169; 2009, PrimeBase Technologies GmbH</font></P></BODY></HTML>");

	replyPending = false;
	iOutputStream->writeHead();
	iOutputStream->writeBody();
	iOutputStream->flush();
	exit_();
}

void MSConnectionHandler::writeException()
{
	writeException(NULL);
}

void MSConnectionHandler::closeStream()
{
	enter_();
	if (iOutputStream) {
		if (replyPending) {
			try_(a) {
				writeException();
			}
			catch_(a) {
			}
			cont_(a);
		}
		iOutputStream->release();
		iOutputStream = NULL;
	}
	if (iInputStream) {
		iInputStream->release();
		iInputStream = NULL;
	}
	exit_();
}

void MSConnectionHandler::parseRequestURI()
{
	CSString	*uri = iInputStream->getRequestURI();
	uint32_t		pos = 0, end;
	enter_();
	
	freeRequestURI();
	pos = uri->locate(0, "://");
	if (pos < uri->length())
		pos += 3;
	else
		pos = uri->skip(0, '/');

	// Table URI
	end = uri->locate(pos, '/');
	//end = uri->locate(uri->nextPos(end), '/'); I am not sure why this was done.
	//iTableURI = uri->substr(pos, end - pos);
	iTableURI = uri->substr(pos);

	exit_();
}

void MSConnectionHandler::freeRequestURI()
{
	if (iTableURI)
		iTableURI->release();
	iTableURI = NULL;
}

void MSConnectionHandler::writeFile(CSString *file_path)
{
	CSPath			*path;
	CSFile			*file;

	enter_();
	push_(file_path);

	path = CSPath::newPath(RETAIN(file_path));
	pop_(file_path);
	push_(path);
	if (path->exists()) {
		file = path->openFile(CSFile::READONLY);
		push_(file);

		iOutputStream->setContentLength((uint64_t) path->getSize());
		replyPending = false;
		iOutputStream->writeHead();
		
		CSStream::pipe(RETAIN(iOutputStream), file->getInputStream());

		release_(file);
	}
	else {
		myException.initFileError(CS_CONTEXT, path->getCString(), ENOENT);
		writeException();
	}
	release_(path);

	exit_();
}

/*
 * Request URI: /<blob URL>
* OR
 * Request URI: /<database>/<blob alias>
 */
void MSConnectionHandler::handleGet(bool info_only)
{
	const char	*bad_url_comment = "Incorrect URL: ";
	MSOpenTable	*otab;
	CSString	*info_request;
	CSString	*ping_request;

	enter_();
	self->myException.setErrorCode(0);

	iOutputStream->clearHeaders();
	iOutputStream->clearBody();
	//iOutputStream->setStatus(200); This is done in the send now.
	
	parseRequestURI();

	ping_request = iInputStream->getHeaderValue(MS_PING_REQUEST);
	if (ping_request) {
		MSDatabase *db;
		
		db = MSDatabase::getDatabase(ping_request, false);
		if (db) {
			push_(db);
			if (db->myBlobCloud->cl_getDefaultCloudRef()) {
				MSCloudInfo *info = db->myBlobCloud->cl_getCloudInfo();
				push_(info);
				iOutputStream->addHeader(MS_CLOUD_SERVER, info->getServer());
				release_(info);
			}
			release_(db);
		}
		
		iOutputStream->setStatus(200); 
		iOutputStream->writeHead();
		iOutputStream->flush();
		exit_();

	}
	
	info_request = iInputStream->getHeaderValue(MS_BLOB_INFO_REQUEST);
	if (info_request) {
		info_only = (info_request->compare("yes") == 0);
		info_request->release();
	}
	
  
	
	if (iTableURI->length() == 0)
		goto bad_url;
	

	MSBlobURLRec blob;

	if (iTableURI->equals("favicon.ico")) {
		iOutputStream->setStatus(200); 
		writeFile(iTableURI);
	} else if (PBMSBlobURLTools::couldBeURL(iTableURI->getCString(), &blob)) {  
		uint64_t size, offset;
    
		if ((! info_only) && iInputStream->getRange(&size, &offset)) { 
			if (offset >= blob.bu_blob_size) {
				iOutputStream->setStatus(416); // Requested range not satisfiable.
				iOutputStream->writeHead();
				iOutputStream->flush();
				exit_();
			}
			
			if (size > (blob.bu_blob_size - offset))
				size = blob.bu_blob_size - offset;				

			iOutputStream->setRange(size, offset, blob.bu_blob_size);
		} else {
			size = blob.bu_blob_size;
			offset = 0;
		}
 
		if (blob.bu_type == MS_URL_TYPE_BLOB) {
			otab = MSTableList::getOpenTableByID(blob.bu_db_id, blob.bu_tab_id);
			frompool_(otab);
			otab->sendRepoBlob(blob.bu_blob_id, offset, size, blob.bu_auth_code, info_only, iOutputStream);
			backtopool_(otab);
		} else {
			MSRepoFile	*repo_file;

			if (!(otab = MSTableList::getOpenTableForDB(blob.bu_db_id))) {
				char buffer[CS_EXC_MESSAGE_SIZE];
				char id_str[12];
				
				snprintf(id_str, 12, "%"PRIu32"", blob.bu_db_id);

				cs_strcpy(CS_EXC_MESSAGE_SIZE, buffer, "Unknown database ID # ");
				cs_strcat(CS_EXC_MESSAGE_SIZE, buffer, id_str);
				CSException::throwException(CS_CONTEXT, MS_ERR_UNKNOWN_DB, buffer);
			}
			frompool_(otab);
			repo_file = otab->getDB()->getRepoFileFromPool(blob.bu_tab_id, false);
			frompool_(repo_file);
			repo_file->sendBlob(otab, blob.bu_blob_id, offset, size, blob.bu_auth_code, true, info_only, iOutputStream);
			backtopool_(repo_file);
			backtopool_(otab);
		}
	} 
	else { 
#ifdef HAVE_ALIAS_SUPPORT

		CSString	*db_name;
		CSString	*alias;
		MSDatabase * db;
		uint32_t repo_id;
		uint64_t blob_id;
		
		db_name = iTableURI->left("/");
		push_(db_name);
		alias = iTableURI->right("/");
		push_(alias);

		if (db_name->length() == 0 || alias->length() == 0 || alias->length() > BLOB_ALIAS_LENGTH) 
			goto bad_url;
		
		if (!(otab = MSTableList::getOpenTableForDB(MSDatabase::getDatabaseID(db_name->getCString(), true)))) {
			char buffer[CS_EXC_MESSAGE_SIZE];

			cs_strcpy(CS_EXC_MESSAGE_SIZE, buffer, "Unknown database: ");
			cs_strcat(CS_EXC_MESSAGE_SIZE, buffer, db_name->getCString());
			CSException::throwException(CS_CONTEXT, MS_ERR_UNKNOWN_DB, buffer);
		}
		frompool_(otab);

		db = otab->getDB();
		
		// lookup the blob alias in the database.
		if (!db->findBlobWithAlias(alias->getCString(), &repo_id, &blob_id)) {
			char buffer[CS_EXC_MESSAGE_SIZE];

			cs_strcpy(CS_EXC_MESSAGE_SIZE, buffer, "Unknown alias: ");
			cs_strcat(CS_EXC_MESSAGE_SIZE, buffer, alias->getCString());
			CSException::throwException(CS_CONTEXT, MS_ERR_NOT_FOUND, buffer);
		}
			
		MSRepoFile *repo_file = db->getRepoFileFromPool(repo_id, false);
		
		frompool_(repo_file);
		repo_file->sendBlob(otab, blob_id, 0, false, info_only, iOutputStream);
		backtopool_(repo_file);	
			
		backtopool_(otab);		
		
		release_(alias);
		release_(db_name);

#else
		char buffer[CS_EXC_MESSAGE_SIZE];

		cs_strcpy(CS_EXC_MESSAGE_SIZE, buffer, "Bad PBMS BLOB URL: ");
		cs_strcat(CS_EXC_MESSAGE_SIZE, buffer, iTableURI->getCString());
		CSException::throwException(CS_CONTEXT, MS_ERR_NOT_FOUND, buffer);
#endif
	}
	

	exit_();

	bad_url:
	char buffer[CS_EXC_MESSAGE_SIZE];

	cs_strcpy(CS_EXC_MESSAGE_SIZE, buffer, bad_url_comment);
	cs_strcat(CS_EXC_MESSAGE_SIZE, buffer, iInputStream->getRequestURI()->getCString());
	CSException::throwException(CS_CONTEXT, MS_ERR_INCORRECT_URL, buffer);
	exit_();
}

void MSConnectionHandler::handlePut()
{
	MSOpenTable *otab = NULL;
	uint32_t	db_id = 0, tab_id;

	enter_();
	self->myException.setErrorCode(0);

	iOutputStream->clearHeaders();
	iOutputStream->clearBody();
	iOutputStream->setStatus(200);
	
	parseRequestURI();
	if (iTableURI->length() != 0)
		MSDatabase::convertTablePathToIDs(iTableURI->getCString(), &db_id, &tab_id, true);


	if ((!db_id) || !(otab = MSTableList::getOpenTableByID(db_id, tab_id))) {
		char buffer[CS_EXC_MESSAGE_SIZE];

		cs_strcpy(CS_EXC_MESSAGE_SIZE, buffer, "Incorrect URL: ");
		cs_strcat(CS_EXC_MESSAGE_SIZE, buffer, iInputStream->getRequestURI()->getCString());
		CSException::throwException(CS_CONTEXT, MS_ERR_INCORRECT_URL, buffer);
	}
	frompool_(otab);
	
	uint64_t			blob_len, cloud_blob_len = 0;
	PBMSBlobURLRec	bh;
	size_t			handle_len;
	uint16_t			metadata_size = 0; 
	CSStringBuffer	*metadata;
	
	new_(metadata, CSStringBuffer(80));
	push_(metadata);
	
	 if (! iInputStream->getContentLength(&blob_len)) {
		CSException::throwException(CS_CONTEXT, CS_ERR_MISSING_HTTP_HEADER, "Missing content length header");
	 }
	 
	
	// Collect the meta data.
	for (uint32_t i = 0; i < iInputStream->numHeaders(); i++) {
		CSHeader *header = iInputStream->getHeader(i);
		const char *name = header->getNameCString();
		
		push_(header);
		
		if (!strcmp(name, MS_BLOB_SIZE)) { // The actual BLOB data size if it is being stored in a cloud.
			sscanf(header->getValueCString(), "%"PRIu64"", &cloud_blob_len);
		}
			
		if (name && otab->getDB()->isValidHeaderField(name)) {
			uint16_t rec_size, name_size, value_size;
			const char *value = header->getValueCString();
			char *buf;
			if (!value)
				value = "";
				
			name_size = strlen(name);
			value_size = strlen(value);
			
			rec_size = name_size + value_size + 2;
			metadata->setLength(metadata_size + rec_size);
			
			buf = metadata->getBuffer(metadata_size);
			metadata_size += rec_size;
			
			memcpy(buf, name, name_size);
			buf += name_size;
			*buf = 0; buf++;
			
			memcpy(buf, value, value_size);
			buf += value_size;
			*buf = 0;
		}
		
		release_(header);
	}
	
	if (blob_len) {
		char hex_checksum[33];
		Md5Digest checksum;
		
		otab->createBlob(&bh, blob_len, metadata->getBuffer(0), metadata_size, RETAIN(iInputStream), NULL, &checksum);

		cs_bin_to_hex(33, hex_checksum, 16, checksum.val);
		iOutputStream->addHeader(MS_CHECKSUM_TAG, hex_checksum);
	} else { // If there is no BLOB data then the client will send it to the cloud server themselves.
		if (!cloud_blob_len)
			CSException::throwException(CS_CONTEXT, CS_ERR_MISSING_HTTP_HEADER, "Missing BLOB length header for cloud BLOB.");
		if (otab->getDB()->myBlobType == MS_CLOUD_STORAGE) {
			CloudKeyRec cloud_key;
			uint32_t signature_time;
			char time_str[20];
			CloudDB *cloud = otab->getDB()->myBlobCloud;
			MSCloudInfo *info;
			
			
			cloud->cl_getNewKey(&cloud_key);
			otab->createBlob(&bh, cloud_blob_len, metadata->getBuffer(0), metadata_size, NULL, &cloud_key);
			
			CSString *signature;
			signature = cloud->cl_getSignature(&cloud_key, iInputStream->getHeaderValue("Content-Type"), &signature_time);
			push_(signature);
			
			info = cloud->cl_getCloudInfo(cloud_key.cloud_ref);
			push_(info);
			iOutputStream->addHeader(MS_CLOUD_SERVER, info->getServer());
			iOutputStream->addHeader(MS_CLOUD_BUCKET, info->getBucket());
			iOutputStream->addHeader(MS_CLOUD_KEY, info->getPublicKey());
			iOutputStream->addHeader(MS_CLOUD_OBJECT_KEY, cloud->cl_getObjectKey(&cloud_key));
			iOutputStream->addHeader(MS_BLOB_SIGNATURE, signature->getCString());
			release_(info);
			
			release_(signature);
			snprintf(time_str, 20, "%"PRIu32"", signature_time);
			iOutputStream->addHeader(MS_BLOB_DATE, time_str);
			
		} else {
			// If the database is not using cloud storage then the client will
			// resend the BLOB data as a normal BLOB when it fails to get the
			// expected cloud server infor headers back.
			bh.bu_data[0] = 0;
		}
	}
	handle_len = strlen(bh.bu_data);
	iOutputStream->setContentLength(handle_len);

	replyPending = false;
	iOutputStream->writeHead();
	iOutputStream->write(bh.bu_data, handle_len);
	iOutputStream->flush();

	release_(metadata);

	backtopool_(otab);

	exit_();
}

void MSConnectionHandler::serviceConnection()
{
	const char	*method;
	bool		threadStarted = false;

	for (;;) {
		iInputStream->readHead();
		if (iInputStream->expect100Continue()) {			
			iOutputStream->clearHeaders();
			iOutputStream->clearBody();
			iOutputStream->setStatus(100);
			iOutputStream->setContentLength(0);
			iOutputStream->writeHead();
			iOutputStream->flush();
		}
		
		if (!(method = iInputStream->getMethod()))
			break;
		if (!threadStarted /* && iInputStream->keepAlive() */) { // Ignore keepalive: Never trust the client!
			/* Start another service handler if no threads
			 * are waiting to listen!
			 */
			threadStarted = true;
			if (!MSNetwork::gWaitingToListen)
				MSNetwork::startConnectionHandler();
		}
		replyPending = true;
		if (strcmp(method, "GET") == 0)
			handleGet(false);
		else if (strcmp(method, "PUT") == 0 ||
			strcmp(method, "POST") == 0)
			handlePut();
		else if (strcmp(method, "HEAD"))
			handleGet(true);
		else
			CSException::throwCoreError(CS_CONTEXT, CS_ERR_UNKNOWN_METHOD, method);
	}
}

bool MSConnectionHandler::initializeWork()
{
	return true;
}

/*
 * Return false if no connection this thread should quit!
 */
bool MSConnectionHandler::doWork()
{
	enter_();

	/* Open a connection: */
	if (!openStream()) {
		myMustQuit = true;
		return_(false);
	}

	/* Do the work for the connection: */
	serviceConnection();

	/* Close the connection: */
	close();

	return_(false);
}

void *MSConnectionHandler::completeWork()
{
	shuttingDown = true;
	/* Close the stream, if it was openned. */
	close();

	return NULL;
}

bool MSConnectionHandler::handleException()
{
	if (!myMustQuit) {
		/* Start another handler if required: */
		if (!MSNetwork::gWaitingToListen)
			MSNetwork::startConnectionHandler();
	}
	close();
	if (!shuttingDown)
		CSDaemon::handleException();
	return false;
}


