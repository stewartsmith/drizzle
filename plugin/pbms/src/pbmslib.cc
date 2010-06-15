/* Copyright (c) 2008 PrimeBase Technologies GmbH, Germany
 *
 *  PrimeBase Media Stream (PBMS)
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
 * 2008-09-10	Barry Leslie
 *
 * H&G2JCtL
 */
#include "cslib/CSConfig.h"
#include <inttypes.h>

#include <curl/curl.h>
#include <string.h>

#ifdef DRIZZLED
#include <libdrizzle/drizzle_client.h>
#define MYSQL drizzle_con_st
#define MYSQL_RES drizzle_result_st
#define MYSQL_ROW DRIZZLE_ROW

#define mysql_query			drizzle_query
#define mysql_store_result	drizzle_store_result
#define mysql_errno			drizzle_errno
#define mysql_error			drizzle_error
#define mysql_num_rows		drizzle_num_rows
#define mysql_fetch_row		drizzle_fetch_row
#define mysql_free_result	drizzle_free_result

#else
#include "mysql.h"
#endif

#include "pbmslib.h"
#include "pbms.h"
#include "cslib/CSGlobal.h"
#include "cslib/CSThread.h"
#include "cslib/CSString.h"
#include "cslib/CSStrUtil.h"
//#include "cslib/CSSocket.h"
#include "cslib/CSHTTPStream.h"
#include "cslib/CSMd5.h"
#include "cslib/CSS3Protocol.h"
#include "util_ms.h"
#include "metadata_ms.h"

#define CLEAR_SELF()	CSThread::setSelf(NULL)
#define MAX_STMT_SIZE	1024

static int global_errno;
static char global_err_message[MS_RESULT_MESSAGE_SIZE];

static unsigned long init_count = 0;
static CSThreadList	*pbms_thread_list = NULL;
static	CSThread *mslib_global_thread = NULL;

static void report_global_mse_error(CSThread *thd)
{
	global_errno = thd->myException.getErrorCode();
	cs_strcpy(MS_RESULT_MESSAGE_SIZE, global_err_message,  thd->myException.getMessage());
}

#define DFLT_CONNECTION_TIMEOUT	10	// Changing this required a documentation update.

#define THROW_CURL_IF(v) { if (v) CSException::throwException(CS_CONTEXT, CS_ERR_GENERIC_ERROR, ms_curl_error);}
//======================================
static size_t receive_data(void *ptr, size_t size, size_t nmemb, void *stream);
static size_t receive_header(void *ptr, size_t size, size_t nmemb, void *stream);
static size_t send_callback(void *ptr, size_t size, size_t nmemb, void *stream);
//--------------------------------------------
class  PBMS_ConHandle:public CSThread {
	public:
	
	// Fields that must be freed when the object is destroyed.
	CSString			*ms_host;
	CSString			*ms_database;
	struct curl_slist	*ms_header_list;	// A curl list of headers to be sent with the next PUT.
	struct curl_slist	*ms_info_only;	// A curl list of headers to be sent with Get.
	struct curl_slist	*ms_ping_header;	// A curl list of headers to be sent with ping.
	CSStringBuffer		*ms_url_str;
	CURL				*ms_curl;	
	CSStringBuffer		*ms_buffer;
	CSStringBuffer		*ms_errorReply;
	CSS3Protocol		*ms_cloud;
	uint64_t			ms_range_start;
	uint64_t			ms_range_end;

	PBMS_ConHandle():
		CSThread(pbms_thread_list),
		ms_host(NULL),
		ms_database(NULL),
		ms_port(0),
		ms_transmition_timeout(0),
		ms_curl(NULL),
		ms_header_list(NULL),
		ms_info_only(NULL),
		ms_ping_header(NULL),
		ms_buffer(NULL),
		ms_errorReply(NULL),
		ms_url_str(NULL),
		ms_cloud(NULL),
		ms_errno(0),
		ms_throw_error(false),
		ms_range_start(1),
		ms_range_end(0)
	{
		}
		
	~PBMS_ConHandle() {
		if (ms_curl) 
			curl_easy_cleanup(ms_curl);
			
		if (ms_cloud) 
			ms_cloud->release();
			
		if (ms_database) 
			ms_database->release();
			
		if (ms_host) 
			ms_host->release();
			
		if (ms_buffer) 
			ms_buffer->release();
			
		if (ms_errorReply) 
			ms_errorReply->release();
			
		if (ms_url_str) 
			ms_url_str->release();
			
		if (ms_header_list) 
			curl_slist_free_all(ms_header_list);
			
		if (ms_info_only) 
			curl_slist_free_all(ms_info_only);
			
		if(ms_ping_header)
			curl_slist_free_all(ms_ping_header);
			
		ms_headers.clearHeaders();
		ms_metadata_out.clearHeaders();
			
	}
	unsigned int		ms_replyStatus;
	CSHTTPHeaders		ms_headers; 
	CSHTTPHeaders		ms_metadata_out;
	uint32_t				ms_next_header;
	uint32_t				ms_max_header;	
	unsigned int		ms_port;
	unsigned int		ms_transmition_timeout; // In the future this may have some effect but for now it is always be 0 (no timeout).
	unsigned int		ms_url_base_len;
	bool				ms_throw_error;	// Gets set if an exception occurs in a callback.

	int					ms_errno;
	char				ms_err_message[MS_RESULT_MESSAGE_SIZE];
	
	
	char ms_curl_error[CURL_ERROR_SIZE];
	
	size_t ms_DataToBeTransfered;
	// Get data caller parameters:
	u_char				*ms_getBuffer;
	size_t				ms_getBufferSize;
	PBMS_WRITE_CALLBACK_FUNC	ms_writeCB;
	void				*ms_getCBData;
	
	void set_downLoadUserData(u_char *buffer, size_t buffer_size, PBMS_WRITE_CALLBACK_FUNC cb = NULL, void *caller_data = NULL)	
	{
		ms_DataToBeTransfered = buffer_size;
		ms_getBuffer = buffer;
		ms_getBufferSize = buffer_size;
		ms_writeCB = cb;
		ms_getCBData = caller_data;		
	}
	
	// Put data caller parameters:
	const u_char		*ms_putData;
	size_t				ms_putDataLen;
	size_t				ms_putDataOffset;
	PBMS_READ_CALLBACK_FUNC	ms_readCB;
	void				*ms_putCBData;
	
	void set_upLoadUserData(const u_char *buffer, size_t size, PBMS_READ_CALLBACK_FUNC cb = NULL, void *caller_data = NULL)	
	{
		ms_DataToBeTransfered = size;
		ms_putData = buffer;
		ms_putDataLen = size;
		ms_putDataOffset =0;
		ms_readCB = cb;
		ms_putCBData = caller_data;
	}
	
	
	void ms_initConnection(const char* host, unsigned int port, const char* database)
	{
		ms_curl = curl_easy_init();
		if (!ms_curl)
			CSException::throwException(CS_CONTEXT, CS_ERR_GENERIC_ERROR, "curl_easy_init() failed.");
		
		if (curl_easy_setopt(ms_curl, CURLOPT_ERRORBUFFER, ms_curl_error))
			CSException::throwException(CS_CONTEXT, CS_ERR_GENERIC_ERROR, "curl_easy_setopt(CURLOPT_ERRORBUFFER) failed.");
		
		// Uncomment this line to trace network action during request. Very Usefull!!
		//curl_easy_setopt(ms_curl, CURLOPT_VERBOSE, 1L);
		
		THROW_CURL_IF(curl_easy_setopt(ms_curl, CURLOPT_TCP_NODELAY, 1L));

		THROW_CURL_IF(curl_easy_setopt(ms_curl, CURLOPT_NOPROGRESS, 1L));
		THROW_CURL_IF(curl_easy_setopt(ms_curl, CURLOPT_WRITEFUNCTION, receive_data));
		THROW_CURL_IF(curl_easy_setopt(ms_curl, CURLOPT_READFUNCTION, send_callback));	
		THROW_CURL_IF(curl_easy_setopt(ms_curl, CURLOPT_HEADERFUNCTION, receive_header));
		
		THROW_CURL_IF(curl_easy_setopt(ms_curl, CURLOPT_WRITEDATA, this));
		THROW_CURL_IF(curl_easy_setopt(ms_curl, CURLOPT_READDATA, this));
		THROW_CURL_IF(curl_easy_setopt(ms_curl, CURLOPT_WRITEHEADER, this));

		THROW_CURL_IF(curl_easy_setopt(ms_curl, CURLOPT_FOLLOWLOCATION, 1L)); // Follow redirects.

		ms_port = port;
		pbms_api_owner = true;
		ms_host = CSString::newString(host);
		ms_database = CSString::newString(database);
		
		ms_url_str = new CSStringBuffer(50);	
		ms_url_str->append("http://");
		ms_url_str->append(host);
		ms_url_str->append(":");
		ms_url_str->append(port);
		ms_url_str->append("/");
		ms_url_base_len	= 	ms_url_str->length();
			
		ms_buffer = new CSStringBuffer(50);	

		ms_info_only = curl_slist_append(ms_info_only, MS_BLOB_INFO_REQUEST ": yes");
		if (!ms_info_only) 
			CSException::throwException(CS_CONTEXT, CS_ERR_GENERIC_ERROR, "curl_slist_append() failed.");
			
		ms_buffer->append(MS_PING_REQUEST": ");
			ms_buffer->append(database);
		ms_ping_header = curl_slist_append(ms_ping_header, ms_buffer->getCString());
		if (!ms_ping_header) 
			CSException::throwException(CS_CONTEXT, CS_ERR_GENERIC_ERROR, "curl_slist_append() failed.");

		ms_buffer->setLength(0);

	}
	
	void ms_ping();
	
	void ms_setSelf()
	{
		setSelf(this);
	}

	void ms_init_put_blob(curl_off_t size, const char *table, const char *alias, const char *checksum, bool use_cloud);
	
	void ms_init_get_blob(const char *ref, bool is_alias, bool info_only);
	
	void ms_get_info(const char *ref, bool is_alias);
	void ms_sendCloudBLOB(size_t size)	;
	
	pbms_bool ms_downLoadData(const char *ref, u_char *buffer, size_t buffer_size, PBMS_WRITE_CALLBACK_FUNC cb = NULL, void *caller_data = NULL);

	pbms_bool ms_upLoadData(const char *table, const char *alias, const char *checksum, char *ref, size_t size, const u_char *data, PBMS_READ_CALLBACK_FUNC cb = NULL, void *caller_data = NULL);
	
	uint32_t ms_init_fetch() {ms_next_header =0; return ms_max_header = ms_headers.numHeaders();}
	
	bool ms_next(const char **name, const char **value) 
	{
		if (ms_next_header >= ms_max_header)
			return false;
			
		CSHeader *header = ms_headers.getHeader(ms_next_header++);
		*name = header->getNameCString();
		*value = header->getValueCString();
		header->release();
		return true;
	}
	
	void dump_headers() 
	{
		uint32_t i = 0;
		CSHeader *header;
		printf("Headers:\n");
		printf("---------------------------------------\n");
		while  ( (header = ms_headers.getHeader(i++)) ) {
			printf("%s : %s\n", header->getNameCString(), header->getValueCString());
			header->release();			
		}
		printf("---------------------------------------\n");
	}
					
	void ms_addS3HeadersHeaders(CSVector *s3Headers);

	CSString	*ms_get_metadata(const char *name) 
	{ 
		return ms_headers.getHeaderValue(name);
	}
					
	CSString	*ms_get_alias() {return ms_get_metadata(MS_ALIAS_TAG);}

	CSString	*ms_get_checksum() {return ms_get_metadata(MS_CHECKSUM_TAG);}

	void report_error(CSThread *self)
	{
		ms_errno = self->myException.getErrorCode();
		cs_strcpy(MS_RESULT_MESSAGE_SIZE, ms_err_message,  self->myException.getMessage());
	}

	void report_error(int err, const char *msg)
	{
		ms_errno = err;
		cs_strcpy(MS_RESULT_MESSAGE_SIZE, ms_err_message,  msg);
	}

	void throw_http_reply_exception();
	
	void check_reply_status() 
	{
		switch (ms_replyStatus) {
			case 200:
			//case 301: // Moved Permanently
			//case 307: // Temporary Redirect
				break;
			default:
				throw_http_reply_exception();
		}
		
	}
	
	void ms_getCloudHeaders() 
	{
		CSString *value = NULL;
		
		enter_();
		
		// Get the S3 server
		value = ms_headers.getHeaderValue(MS_CLOUD_SERVER);
		if (!value) {
			if (ms_cloud)
				ms_cloud->release();
			ms_cloud = NULL;
			exit_();
		}
		
		push_(value);
		if (!ms_cloud)
			new_(ms_cloud, CSS3Protocol());
			
		// Remove the cloud headers so they are not visable to the caller.
		ms_headers.removeHeader(MS_CLOUD_SERVER); 
		
		ms_cloud->s3_setServer(value->getCString());
		release_(value);		
			
		// Get the S3 public key
		value = ms_headers.getHeaderValue(MS_CLOUD_KEY);
		if (!value)
			CSException::throwException(CS_CONTEXT, CS_ERR_GENERIC_ERROR, "Missing S3 public key in reply.");
		
		push_(value);
		ms_headers.removeHeader(MS_CLOUD_KEY);
		ms_cloud->s3_setPublicKey(value->getCString());
		release_(value);		

		exit_();				
	}

};

// CURL callback functions:
////////////////////////////
static size_t receive_data(void *vptr, size_t objs, size_t obj_size, void *v_con)
{
	PBMS_ConHandle *con = (PBMS_ConHandle*) v_con;
	size_t data_len = objs * obj_size;
	char *ptr = (char*)vptr;

	if (con->ms_replyStatus >= 400) { // Collect the error reply.
		enter_();
		try_(a) {
			if (!con->ms_errorReply)
				con->ms_errorReply = new CSStringBuffer(50);		
			con->ms_errorReply->append(ptr, data_len);
		}
		catch_(a);
		con->ms_throw_error = true;
		data_len = -1;
		
		cont_(a);
		return_(data_len);
	}
	
	if (data_len > con->ms_DataToBeTransfered) { // This should never happen.
		CSException::RecordException(CS_CONTEXT, CS_ERR_GENERIC_ERROR, "Blob data overflow.");
		con->ms_throw_error = true;
		return -1;
	}
	
	con->ms_DataToBeTransfered -= data_len;
	if (con->ms_writeCB) 
		return (con->ms_writeCB)(con->ms_getCBData, ptr, data_len, false); 
				
	memcpy(con->ms_getBuffer, ptr, data_len);
	con->ms_getBuffer += data_len;
	
	return data_len;
}

#define IS_REDIRECT(s) ((s >= 300) && (s < 400))
//----------------------
static size_t receive_header(void *header, size_t objs, size_t obj_size, void *v_con)
{
	PBMS_ConHandle *con = (PBMS_ConHandle*) v_con;
	size_t size = objs * obj_size;
	char *end, *ptr = (char*) header, *value;
	const char *name;
	uint32_t name_len, value_len;
	
	end = ptr + size;
	if (*(end -2) == '\r' && *(end -1) == '\n')
		end -=2;
		
	while ((end != ptr) && (*ptr == ' ')) ptr++;
	if (end == ptr)
		return size;
	
	// Get the reply status.	
	if (((!con->ms_replyStatus) || (con->ms_replyStatus == 100) || IS_REDIRECT(con->ms_replyStatus) ) && !strncasecmp(ptr, "HTTP", 4)) {
		char status[4];
		while ((end != ptr) && (*ptr != ' ')) ptr++; // skip HTTP stuff
		while ((end != ptr) && (*ptr == ' ')) ptr++; // find the start of eh status code.
		if (end == ptr)
			return size;
			
		if (end < (ptr +3)) // expecting a 3 digit status code.
			return size;
			
		memcpy(status, ptr, 3);
		status[3] = 0;
		
		con->ms_replyStatus = atoi(status);
	}
	
	name = ptr;
	while ((end != ptr) && (*ptr != ':')) ptr++;
	if (end == ptr)
		return size;
	name_len = ptr - name;
	
	ptr++; 
	while ((end != ptr) && (*ptr == ' ')) ptr++;
	if (end == ptr)
		return size;
	
	value = ptr;
	value_len = end - value;
	
	while (name[name_len-1] == ' ') name_len--;
	while (value[value_len-1] == ' ') value_len--;
	
	if (!strncasecmp(name, "Content-Length", 14)) {
		size_t length;
		char len[32];
		memcpy(len, value, (value_len > 31)?31:value_len);
		len[(value_len > 31)?31:value_len] = 0;
		
		length = atoll(len);
		// If there is no callback then the data size is limited
		// to the GetBuffer size.
		if (con->ms_writeCB || (length < con->ms_getBufferSize))
			con->ms_DataToBeTransfered = length;
			
	}
	

	enter_();
	try_(a) {
		if (!strncasecmp(name, "ETag", 4)) { // S3 checksum
			name = MS_CHECKSUM_TAG;
			name_len = strlen(MS_CHECKSUM_TAG);
			// Strip any quotes
			if (*value == '"') {
				value++;value_len--;
			}
			if (value[value_len-1] == '"') {
				value_len--;
			}
			con->ms_headers.removeHeader(MS_CHECKSUM_TAG);
		}
		con->ms_headers.addHeader(name, name_len, value, value_len);
	}
	
	catch_(a);
	con->ms_throw_error = true;
	return_(-1);
		
	cont_(a);
	return_(size);
}

//----------------------
static size_t send_callback(void *ptr, size_t objs, size_t obj_size, void *v_con)
{
	PBMS_ConHandle *con = (PBMS_ConHandle*) v_con;
	char *buffer = (char*) ptr;
	size_t data_sent = 0, buffer_size = objs * obj_size;

	if (con->ms_putDataLen == 0)
		return 0;
		
	if (con->ms_readCB) 
		data_sent = (con->ms_readCB)(con->ms_putCBData, buffer , buffer_size, false);		
	else {
		data_sent = (buffer_size < con->ms_putDataLen)? buffer_size: con->ms_putDataLen;
		memcpy(buffer,con->ms_putData, data_sent);
		con->ms_putData += data_sent;
	}
	con->ms_putDataLen -= data_sent;
	
	return data_sent;
}

#define CONTENT_TYPE "Content-Type"
//------------------------------------------------
void PBMS_ConHandle::ms_init_put_blob(curl_off_t size, const char *table, const char *alias, const char *checksum, bool use_cloud)
{
	char buffer[MS_META_VALUE_SIZE + MS_META_NAME_SIZE +2];
	int buffer_size = MS_META_VALUE_SIZE + MS_META_NAME_SIZE +2;
	bool have_content_type = false;
	
	ms_url_str->setLength(ms_url_base_len);

	ms_url_str->append(ms_database->getCString());
	if (table) {
		ms_url_str->append("/");
		ms_url_str->append(table);
	}
	
	// Remove any old headers
	if (ms_header_list) {
		curl_slist_free_all(ms_header_list);
		ms_header_list = NULL;
	}
	
	// Add metadata headers.
	uint32_t i = 0;
	CSHeader *header;
	while  ( (header = ms_metadata_out.getHeader(i++)) ) {
		cs_strcpy(buffer_size, buffer, header->getNameCString());
		if (!strcasecmp(  buffer, CONTENT_TYPE))
			have_content_type = true;
		cs_strcat(buffer_size, buffer, ':');
		cs_strcat(buffer_size, buffer, header->getValueCString());
		header->release();			
		ms_header_list = curl_slist_append(ms_header_list, buffer);
		if (!ms_header_list) 
			CSException::throwException(CS_CONTEXT, CS_ERR_GENERIC_ERROR, "curl_slist_append() failed.");
	}
	
	if (!have_content_type) {
		// Prevent CURLOPT_POST from adding a content type. 
		ms_header_list = curl_slist_append(ms_header_list, CONTENT_TYPE ":");
		if (!ms_header_list) 
			CSException::throwException(CS_CONTEXT, CS_ERR_GENERIC_ERROR, "curl_slist_append() failed.");
	}
		
	if (checksum) {	
		cs_strcpy(buffer_size, buffer, MS_CHECKSUM_TAG ":");
		cs_strcat(buffer_size, buffer, checksum);
		
		ms_header_list = curl_slist_append(ms_header_list, buffer);
		if (!ms_header_list) 
			CSException::throwException(CS_CONTEXT, CS_ERR_GENERIC_ERROR, "curl_slist_append() failed.");
	}
		
	
#ifdef HAVE_ALIAS_SUPPORT
	if (alias) {
		if (strlen(alias) > MS_META_VALUE_SIZE) 
			CSException::throwException(CS_CONTEXT, CS_ERR_GENERIC_ERROR, "BLOB alias name too long.");

		cs_strcpy(buffer_size, buffer, MS_ALIAS_TAG ":");
		cs_strcat(buffer_size, buffer, alias);
		
		ms_header_list = curl_slist_append(ms_header_list, buffer);
		if (!ms_header_list) 
			CSException::throwException(CS_CONTEXT, CS_ERR_GENERIC_ERROR, "curl_slist_append() failed.");
	}
#endif
	
	if (use_cloud) {
		cs_strcpy(buffer_size, buffer, MS_BLOB_SIZE ":");
		snprintf(buffer + strlen(buffer), buffer_size - strlen(buffer), "%"PRIu64"", size);
		ms_header_list = curl_slist_append(ms_header_list, buffer);
		if (!ms_header_list) 
			CSException::throwException(CS_CONTEXT, CS_ERR_GENERIC_ERROR, "curl_slist_append() failed.");
			
		size = 0; // The BLOB data is not being sent to the PBMS server
	}
	
	// Using CURLOPT_UPLOAD is VERY slow!
	//THROW_CURL_IF(curl_easy_setopt(ms_curl, CURLOPT_INFILESIZE_LARGE, size));
	//THROW_CURL_IF(curl_easy_setopt(ms_curl, CURLOPT_UPLOAD, 1L));
	THROW_CURL_IF(curl_easy_setopt(ms_curl, CURLOPT_POSTFIELDSIZE_LARGE, size));
	THROW_CURL_IF(curl_easy_setopt(ms_curl, CURLOPT_POST, 1L));

	THROW_CURL_IF(curl_easy_setopt(ms_curl, CURLOPT_HTTPHEADER, ms_header_list));
	THROW_CURL_IF(curl_easy_setopt(ms_curl, CURLOPT_URL, ms_url_str->getCString() ));	
		
	ms_replyStatus = 0;
	ms_headers.clearHeaders();
	if (ms_errorReply)
		ms_errorReply->setLength(0);
}
	

//------------------------------------------------
void PBMS_ConHandle::ms_init_get_blob(const char *ref, bool is_alias, bool info_only)
{
	MSBlobURLRec blob;
	
	ms_url_str->setLength(ms_url_base_len);
	
#ifdef HAVE_ALIAS_SUPPORT
	if (is_alias || !ms_parse_blob_url(&blob, ref)) {
		ms_url_str->append(ms_database->getCString());
		ms_url_str->append("/");
	}
#endif
	ms_url_str->append((char *) ref);
	
	//THROW_CURL_IF(curl_easy_setopt(ms_curl, CURLOPT_UPLOAD, 0L));
	THROW_CURL_IF(curl_easy_setopt(ms_curl, CURLOPT_POST, 0L));
	THROW_CURL_IF(curl_easy_setopt(ms_curl, CURLOPT_HTTPHEADER, (info_only)?ms_info_only: NULL));
	THROW_CURL_IF(curl_easy_setopt(ms_curl, CURLOPT_URL, ms_url_str->getCString()));
	
	// NOTE: range 0-0 is valid, it returns the first byte.
	if (ms_range_start <= ms_range_end) {
		char range[80];
		snprintf(range, 80, "%"PRIu64"-%"PRIu64"", ms_range_start, ms_range_end);
		THROW_CURL_IF(curl_easy_setopt(ms_curl, CURLOPT_RANGE, range));
		ms_range_start = 1;
		ms_range_end = 0;
	} else {
		THROW_CURL_IF(curl_easy_setopt(ms_curl, CURLOPT_RANGE, NULL));
	}
	
	ms_replyStatus = 0;
	ms_headers.clearHeaders();
	if (ms_errorReply)
		ms_errorReply->setLength(0);
}
	
//------------------------------------------------
void PBMS_ConHandle::ms_get_info(const char *ref, bool is_alias)
{
	enter_();
	ms_init_get_blob(ref, is_alias, true);
	
	if (curl_easy_perform(ms_curl) && !ms_throw_error)
		CSException::throwException(CS_CONTEXT, CS_ERR_GENERIC_ERROR, ms_curl_error);
	
	if (ms_throw_error)
		throw_();
		
	check_reply_status();
	
	exit_();
}

//------------------------------------------------
void PBMS_ConHandle::ms_ping()
{
	enter_();
	ms_url_str->setLength(ms_url_base_len);
	
//	THROW_CURL_IF(curl_easy_setopt(ms_curl, CURLOPT_UPLOAD, 0L));
	THROW_CURL_IF(curl_easy_setopt(ms_curl, CURLOPT_POST, 0L));
	THROW_CURL_IF(curl_easy_setopt(ms_curl, CURLOPT_HTTPHEADER, ms_ping_header));
	THROW_CURL_IF(curl_easy_setopt(ms_curl, CURLOPT_URL, ms_url_str->getCString()));
	
	ms_replyStatus = 0;
	ms_headers.clearHeaders();
	if (ms_errorReply)
		ms_errorReply->setLength(0);

	if (curl_easy_perform(ms_curl) && !ms_throw_error)
		CSException::throwException(CS_CONTEXT, CS_ERR_GENERIC_ERROR, ms_curl_error);
	
	if (ms_throw_error)
		throw_();
		
	check_reply_status();
	
	// Check to see if the BLOBs in the database are stored in a cloud: 
	CSString *cloud_server;
	cloud_server = ms_headers.getHeaderValue(MS_CLOUD_SERVER);
	if (!cloud_server) {
		if (ms_cloud)
			ms_cloud->release();
		ms_cloud = NULL;
	} else {
		cloud_server->release();
		if (!ms_cloud)
			new_(ms_cloud, CSS3Protocol());
	}
		
	exit_();
}

//------------------------------------------------
void PBMS_ConHandle::throw_http_reply_exception()
{
	CSString *reply = NULL, *error_text = NULL;
	
	enter_();
	
	try_(a) {
		size_t size = 0;
		 //dump_headers();
		 
		if (ms_errorReply)
			size = ms_errorReply->length();
		
		if (!size) {
			error_text = CSString::newString("Missing HTTP reply: possible Media Stream engine connection failure.");
		} else {
			uint32_t start, end;
		
			reply = CSString::newString(ms_errorReply);
			ms_errorReply = NULL;
			
			start = reply->locate(EXCEPTION_REPLY_MESSAGE_PREFIX_TAG, 1);
			start += strlen(EXCEPTION_REPLY_MESSAGE_PREFIX_TAG);
			end = reply->locate(EXCEPTION_REPLY_MESSAGE_SUFFIX_TAG, 1); 
			if (start < end)
				error_text = reply->substr(start, end - start);
			else {
				error_text = reply;
				reply->retain();
			}
		}
		CSException::throwException(CS_CONTEXT, CS_ERR_GENERIC_ERROR, error_text->getCString());
	}
	
	finally_(a) {
		if (reply) reply->release();
		if (error_text) error_text->release();
	}	
	cont_(a);
	
	exit_();
}

//------------------------------------------------
pbms_bool PBMS_ConHandle::ms_downLoadData(const char *ref, u_char *buffer, size_t buffer_size, PBMS_WRITE_CALLBACK_FUNC cb, void *caller_data)
{
	pbms_bool ok = true;
	ms_setSelf();	

	enter_();
	
	try_(a) {	
		set_downLoadUserData(buffer, buffer_size, cb, caller_data);
		ms_init_get_blob(ref, false, false);
		if (curl_easy_perform(ms_curl) && !ms_throw_error)
			CSException::throwException(CS_CONTEXT, CS_ERR_GENERIC_ERROR, ms_curl_error);
		
		if (ms_throw_error)
			throw_();
		
		check_reply_status();
			
	}
	
	catch_(a);
	report_error(self);
	ok = false;
	
	cont_(a);
	
	return_(ok);
}

//------------------------------------------------
void PBMS_ConHandle::ms_sendCloudBLOB(size_t size)
{
	CSInputStream *input;
	CSString *bucket, *object_key, *content_type, *signature, *signature_time;
	CSVector *s3Headers;
	
	enter_();
	
	// Get the S3 bucket
	bucket = ms_headers.getHeaderValue(MS_CLOUD_BUCKET);
	if (!bucket)
		CSException::throwException(CS_CONTEXT, CS_ERR_GENERIC_ERROR, "Missing S3 bucket header in reply.");
	push_(bucket);
	ms_headers.removeHeader(MS_CLOUD_BUCKET);

	// Get the S3 object key
	object_key = ms_headers.getHeaderValue(MS_CLOUD_OBJECT_KEY);
	if (!object_key)
		CSException::throwException(CS_CONTEXT, CS_ERR_GENERIC_ERROR, "Missing S3 object key header in reply.");
	push_(object_key);
	ms_headers.removeHeader(MS_CLOUD_OBJECT_KEY);

	// Get the S3 blob signature
	signature = ms_headers.getHeaderValue(MS_BLOB_SIGNATURE);
	if (!signature)
		CSException::throwException(CS_CONTEXT, CS_ERR_GENERIC_ERROR, "Missing S3 blob signature in reply.");
	
	push_(signature);
	ms_headers.removeHeader(MS_BLOB_SIGNATURE);

	// Get the S3 blob signature date
	signature_time = ms_headers.getHeaderValue(MS_BLOB_DATE);
	if (!signature_time)
		CSException::throwException(CS_CONTEXT, CS_ERR_GENERIC_ERROR, "Missing S3 blob signature date in reply.");
	push_(signature_time);
	ms_headers.removeHeader(MS_BLOB_DATE);

	content_type = ms_headers.getHeaderValue(CONTENT_TYPE);
	if (content_type)
		push_(content_type);

	if (ms_readCB)
		input = CSCallbackInputStream::newStream(ms_readCB, ms_putCBData);
	else
		input = CSMemoryInputStream::newStream(ms_putData, ms_putDataLen);
	
	// Send the BLOB to the cloud storage.
	s3Headers = ms_cloud->s3_send(input, bucket->getCString(), object_key->getCString(), size, (content_type)?content_type->getCString():NULL, NULL, signature->getCString(), atol(signature_time->getCString()));

	ms_addS3HeadersHeaders(s3Headers);
	
	if (content_type)
		release_(content_type);
		
	release_(signature_time);
	release_(signature);
	release_(object_key);
	release_(bucket);
	
	exit_();
}

//------------------------------------------------
void PBMS_ConHandle::ms_addS3HeadersHeaders(CSVector *s3Headers)
{
	CSHTTPHeaders headers;
	enter_();
	
	try_(a) {
		headers.setHeaders(s3Headers);
		
		for (uint32_t i = 0; i < headers.numHeaders(); i++) {
			CSHeader *h = headers.getHeader(i);
			const char *name = h->getNameCString();
			
			if (strcasecmp(name, "ETag") == 0){
				const char *value = h->getValueCString();
				uint32_t value_len = strlen(value);
				
				// Strip any quotes
				if (*value == '"') {
					value++;value_len--;
				}
				if (value[value_len-1] == '"') {
					value_len--;
				}
				
				ms_headers.removeHeader(MS_CHECKSUM_TAG);

				ms_headers.addHeader(MS_CHECKSUM_TAG, CSString::newString(value, value_len));
				h->release();
			} else {
				ms_headers.removeHeader(name);
				ms_headers.addHeader(h);
			}
		}
	}
	
	catch_(a);	
	headers.clearHeaders();
	throw_();
	
	cont_(a);	
	headers.clearHeaders();
	
	exit_();
}
//------------------------------------------------
pbms_bool PBMS_ConHandle::ms_upLoadData(const char *table, const char *alias, const char *checksum, char *ref, size_t size, const u_char *data, PBMS_READ_CALLBACK_FUNC cb, void *caller_data)
{
	pbms_bool ok = true, use_cloud = (ms_cloud != NULL);
	
	ms_setSelf();	

	enter_();
	
	try_(a) {

resend:
		ms_init_put_blob(size, table, alias, checksum, use_cloud);
		set_upLoadUserData(data, size, cb, caller_data);
		set_downLoadUserData((u_char*) ref, PBMS_BLOB_URL_SIZE -1); // Set things up to receive the BLOB ref back.

		if (curl_easy_perform(ms_curl) && !ms_throw_error)
			CSException::throwException(CS_CONTEXT, CS_ERR_GENERIC_ERROR, ms_curl_error);
		
		if (ms_throw_error)
			throw_();
			
		check_reply_status();
	
		*ms_getBuffer =0; // null terminate the blob reference string.
		ms_getCloudHeaders();
		if (ms_cloud && use_cloud) 
			ms_sendCloudBLOB(size);
		else if (use_cloud && !ms_cloud) {
			// We were expecting to send the BLOB to the cloud but 
			// the server did not respond with cloud data. The retry will
			// send the data to the PBMS server.
			use_cloud = false;
			goto resend;
		}
		
	}
	
	catch_(a);
	report_error(self);
	ok = false;
	cont_(a);
	
	return_(ok);
}

//------------------------------------------------
static void report_global_error(int err, const char *message, int line)
{
	char line_no[32];
	
	if (global_errno)
		return;
	global_errno = err;
	cs_strcpy(MS_RESULT_MESSAGE_SIZE, global_err_message,  message);
	snprintf(line_no, 32, ": line %d", line);
	cs_strcat(MS_RESULT_MESSAGE_SIZE, global_err_message,  line_no);
}

//------------------------------------------------
static void clear_global_error()
{
	global_errno = 0;
	*global_err_message = 0;
}

//------------------------------------------------
pbms_bool pbms_library_init()
{
	clear_global_error();
	init_count++;
	
	if (init_count == 1 ) {
		CLEAR_SELF();

		CURLcode curl_code = curl_global_init(CURL_GLOBAL_ALL);
		if (curl_code) {
			report_global_error(curl_code, curl_easy_strerror(curl_code) , __LINE__);
			init_count  = 0;
			return 0;
		}
		
		if (! CSThread::startUp()) {
			report_global_error(ENOMEM, "CSThread::startUp() failed.", __LINE__);
			init_count  = 0;
			return 0;
		}
		
		cs_init_memory();
		
		mslib_global_thread = new CSThread( NULL);
		CSThread::setSelf(mslib_global_thread);
		
		CSThread *self = mslib_global_thread;
		if (!mslib_global_thread) {
			report_global_error(ENOMEM, "new CSThread( NULL) failed.", __LINE__);
			init_count  = 0;
			 CSThread::shutDown();
			return 0;
		}
	
		try_(a) {
			pbms_thread_list = new CSThreadList();
		}
		catch_(a);
		report_global_mse_error(self);
		init_count  = 0;
		mslib_global_thread->release();
		mslib_global_thread = NULL;
		cs_exit_memory();
		CSThread::shutDown();
			
		cont_(a);
	}
	
	return(init_count > 0);
}

//------------------------------------------------
void pbms_library_end()
{
	 
	if (init_count == 1 ) {
		init_count  = 0; 
			

		if (pbms_thread_list) {
			PBMS_ConHandle *con;
			while (con = (PBMS_ConHandle *) pbms_thread_list->getFront()) {
				con->ms_setSelf();
				CSThread::detach(con);
			}
			CSThread::setSelf(mslib_global_thread);
			pbms_thread_list->release();
		} else
			CSThread::setSelf(mslib_global_thread);

		
		mslib_global_thread->release();
		mslib_global_thread = NULL;
		cs_exit_memory();
		CSThread::shutDown();
		
		curl_global_cleanup();
	}
	
	if (init_count > 0)
		init_count--;
}

//------------------------------------------------
int pbms_errno(PBMS myhndl)
{
	PBMS_ConHandle *con = (PBMS_ConHandle*) myhndl;

	if (con) {	
		return con->ms_errno;
	}
		
	return global_errno;
}

//------------------------------------------------
const char *pbms_error(PBMS myhndl)
{
	PBMS_ConHandle *con = (PBMS_ConHandle*) myhndl;

	if (con) {	
		return con->ms_err_message;
	}
		
	return global_err_message;
}

//------------------------------------------------
PBMS pbms_connect(const char* host, unsigned int port, const char *database)
{
	PBMS_ConHandle *con = NULL;
	CLEAR_SELF(); 
	
	clear_global_error();

	new_(con, PBMS_ConHandle());
	
	if ((!con) || !CSThread::attach(con)) {
		report_global_error(ENOMEM, "new PBMS_Ref() failed.", __LINE__);
		if (con) {
			con->release();
			con = NULL;
		}
	} else {
		CSThread *self = con;
		
		try_(a) {
				con->ms_initConnection(host, port, database);
				con->ms_ping();
			}
		catch_(a);
			report_global_mse_error(con);
			CSThread::detach(con);
			con = NULL;
			
		cont_(a);
	}
	
	return(con);
}

//------------------------------------------------
void pbms_close(PBMS myhndl)
{
	PBMS_ConHandle *con = (PBMS_ConHandle*) myhndl;
	
	//	This will kill the 'self' thread so do not try and do any exception handling.
	con->ms_setSelf();
	CSThread::detach(con); // This will also release the connection.
}

//------------------------------------------------
pbms_bool pbms_is_blob_reference(PBMS myhndl, const char *ref)
{
	PBMS_ConHandle *con = (PBMS_ConHandle*) myhndl;
	pbms_bool ok = false;
	
	con->ms_setSelf();	
	enter_();
	
	try_(a) {
		MSBlobURLRec blob;
		if (ms_parse_blob_url(&blob, (char *)ref)) 
			ok = true;
	}
	
	catch_(a);
	cont_(a);
	return_(ok);
}

//------------------------------------------------
pbms_bool pbms_get_blob_size(PBMS myhndl, const char *ref, size_t *size)
{
	PBMS_ConHandle *con = (PBMS_ConHandle*) myhndl;
	bool ok = false;
	
	con->ms_setSelf();	
	enter_();
	
	try_(a) {
		MSBlobURLRec blob;
		if (ms_parse_blob_url(&blob, (char *)ref)) {
			*size = blob.bu_blob_size;
			ok = true;
		} 
#ifdef HAVE_ALIAS_SUPPORT
		else { // Assume it is a BLOB alias
			CSVector *saved_metadata;
			CSString *data = NULL;
			
			saved_metadata = con->ms_headers.takeHeaders();
			try_(b) {
				con->ms_get_info(ref, true);
				data = con->ms_get_metadata(MS_BLOB_SIZE);
				*size = 	atol(data->getCString());	
				data->release();
				ok = true;
			}
			catch_(b);
			con->report_error(self);
			cont_(b);
			con->ms_headers.setHeaders(saved_metadata);
		}
#else
		con->report_error(MS_ERR_INCORRECT_URL, "Invalid BLOB URL");
#endif
	}
	
	catch_(a);
	cont_(a);
	return_(ok);
}
/*
 * pbms_add_metadata() and pbms_clear_metadata() deal with metadata for outgoing BLOBs only.
 */
//------------------------------------------------
pbms_bool pbms_add_metadata(PBMS myhndl, const char *name, const char *value)
{
	PBMS_ConHandle *con = (PBMS_ConHandle*) myhndl;
	bool ok = false;
	
	con->ms_setSelf();	
	enter_();
	
	try_(a) {
		if (strlen(name) > MS_META_NAME_SIZE)
			CSException::throwException(CS_CONTEXT, CS_ERR_GENERIC_ERROR, "Metadata name too long.");
		if (strlen(value) > MS_META_VALUE_SIZE)
			CSException::throwException(CS_CONTEXT, CS_ERR_GENERIC_ERROR, "Metadata value too long.");
		
		con->ms_metadata_out.addHeader(name, value);

		ok = true;
	}
	
	catch_(a) {
		con->report_error(self);
	}
	
	cont_(a);
	
	return_(ok);
}

//------------------------------------------------
void pbms_clear_metadata(PBMS myhndl, const char *name)
{
	PBMS_ConHandle *con = (PBMS_ConHandle*) myhndl;
	
	con->ms_setSelf();	
	enter_();
	
	try_(a) {
		if (name)
			con->ms_metadata_out.removeHeader(name);
		else
			con->ms_metadata_out.clearHeaders();
	}
	
	catch_(a) {
		con->report_error(self);
	}
	
	cont_(a);
	
}

/*
 * pbms_reset_metadata(), pbms_next_metadata() and pbms_get_metadata_value() deal with metadata for the last
 * BLOB received on the connection.
 */
//------------------------------------------------
unsigned int pbms_reset_metadata(PBMS myhndl)
{
	PBMS_ConHandle *con = (PBMS_ConHandle*) myhndl;
	unsigned int count = 0;
	
	con->ms_setSelf();	
	enter_();
	
	try_(a) {
		count = con->ms_init_fetch();
	}
	
	catch_(a) {
		con->report_error(self);
	}
	
	cont_(a);
	
	return_(count);
}

//------------------------------------------------
pbms_bool pbms_next_metadata(PBMS myhndl, char *name, char *value, size_t *v_size)
{
	PBMS_ConHandle *con = (PBMS_ConHandle*) myhndl;
	bool ok = false;
	size_t null_size = MS_META_VALUE_SIZE;
	
	con->ms_setSelf();	
	enter_();
	
	if (!v_size)
		v_size = &null_size;
		
	try_(a) {
		const char *m_name, *m_value;
		ok = con->ms_next(&m_name, &m_value);
		if (ok) {
			cs_strcpy(MS_META_NAME_SIZE, name, m_name);
			cs_strcpy(*v_size, value, m_value);

			if (*v_size <= strlen(m_value)) 
				*v_size = strlen(m_value) +1;

		}
	}
	
	catch_(a) {
		con->report_error(self);
	}
	
	cont_(a);
	
	return_(ok);
}

//------------------------------------------------
pbms_bool pbms_get_metadata_value(PBMS myhndl, const char *name, char *buffer, size_t *size)
{
	PBMS_ConHandle *con = (PBMS_ConHandle*) myhndl;
	bool ok = false;
	
	con->ms_setSelf();	
	enter_();
	
	try_(a) {
		CSString *data = NULL;
		data = con->ms_get_metadata(name);
		if (data) {
			ok = true;
			cs_strcpy(*size, buffer, data->getCString());
			if (data->length() >= *size) {
				*size = data->length() +1;				
			}
			data->release();
		}
	}
	
	catch_(a) {
		con->report_error(self);
	}
	
	cont_(a);
	
	return_(ok);
}

//------------------------------------------------
pbms_bool pbms_get_md5_digest(PBMS myhndl, char *md5_digest)
{
	PBMS_ConHandle *con = (PBMS_ConHandle*) myhndl;
	bool ok = false;
	
	con->ms_setSelf();	
	enter_();
	
	try_(a) {		
		CSString *data = NULL;
		data = con->ms_get_checksum();
		if (!data)
			CSException::throwException(CS_CONTEXT, CS_ERR_GENERIC_ERROR, "No checksum found.");
		push_(data);
		if (cs_hex_to_bin(16, md5_digest, data->length(), data->getCString()) != 16) {
			CSException::throwException(CS_CONTEXT, CS_ERR_GENERIC_ERROR, "Invalid MD5 Digest.");
		} 
		release_(data);
		ok = true;
	}
	
	catch_(a) {
		con->report_error(self);
	}
	
	cont_(a);
	
	return_(ok);
}

//------------------------------------------------
//------------------------------------------------
pbms_bool pbms_put_data(PBMS myhndl, const char *table, const char *checksum, char *ref, size_t size, const unsigned char *data)
{
	PBMS_ConHandle *con = (PBMS_ConHandle*) myhndl;
	
	return con->ms_upLoadData(table, NULL, checksum, ref, size, data);
}
//------------------------------------------------
pbms_bool pbms_put_data_cb(PBMS myhndl, const char *table, const char *checksum, char *ref, size_t size, PBMS_READ_CALLBACK_FUNC cb, void *caller_data)
{
	PBMS_ConHandle *con = (PBMS_ConHandle*) myhndl;
	
	return con->ms_upLoadData(table, NULL, checksum, ref, size, NULL, cb, caller_data);
}

//------------------------------------------------
pbms_bool pbms_get_data(PBMS myhndl, const char *ref, unsigned char *buffer, size_t buffer_size)
{
	PBMS_ConHandle *con = (PBMS_ConHandle*) myhndl;
	
	return con->ms_downLoadData(ref, buffer, buffer_size);
}

//------------------------------------------------
pbms_bool pbms_get_data_range(PBMS myhndl, const char *ref, size_t start_offset, size_t end_offset, unsigned char *buffer, size_t buffer_size, size_t *data_size)
{
	PBMS_ConHandle *con = (PBMS_ConHandle*) myhndl;
	
	con->ms_range_start = start_offset;
	con->ms_range_end = end_offset;
	
	if (con->ms_downLoadData(ref, buffer, buffer_size)) {
		if (data_size)
			*data_size = con->ms_getBuffer - buffer;
		return true;
	}
  
	return false;
}

//------------------------------------------------
pbms_bool pbms_get_data_cb(PBMS myhndl, const char *ref, PBMS_WRITE_CALLBACK_FUNC cb, void *caller_data)
{
	PBMS_ConHandle *con = (PBMS_ConHandle*) myhndl;
	
	return con->ms_downLoadData(ref, NULL, 0, cb, caller_data);
}

//------------------------------------------------
pbms_bool pbms_get_data_range_cb(PBMS myhndl, const char *ref, size_t start_offset, size_t end_offset, PBMS_WRITE_CALLBACK_FUNC cb, void *caller_data)
{
	PBMS_ConHandle *con = (PBMS_ConHandle*) myhndl;
	
	con->ms_range_start = start_offset;
	con->ms_range_end = end_offset;

	return con->ms_downLoadData(ref, NULL, 0, cb, caller_data);
}

//------------------------------------------------
pbms_bool pbms_get_info(PBMS myhndl, const char *ref)
{
	PBMS_ConHandle *con = (PBMS_ConHandle*) myhndl;
	pbms_bool ok = true;
	
	con->ms_setSelf();	
	enter_();
	
	try_(a) {	
		con->ms_get_info(ref, false);			
	}
	
	catch_(a);
	con->report_error(self);
	ok = false;
	
	cont_(a);
	
	return_(ok);
}

//------------------------------------------------
pbms_bool pbms_set_option(PBMS myhndl, enum pbms_option option, const void *in_value)
{
	PBMS_ConHandle *con = (PBMS_ConHandle*) myhndl;
	pbms_bool ok = true;
	
	con->ms_setSelf();	
	enter_();
	
	try_(a) {

		switch (option) {
			case PBMS_OPTION_DATABASE: {
				CSString *database = CSString::newString((char *)in_value);
				con->ms_database->release();
				con->ms_database = database;
				break;
			}
				
			case PBMS_OPTION_TRANSMITION_TIMEOUT:
				con->ms_transmition_timeout = *((unsigned int*)in_value);
				break; 
			
			case PBMS_OPTION_HOST:
			case PBMS_OPTION_PORT:
				CSException::throwException(CS_CONTEXT, CS_ERR_GENERIC_ERROR, "Option is ReadOnly.");
				break;

			default:
				CSException::throwException(CS_CONTEXT, CS_ERR_GENERIC_ERROR, "Unknown Option.");
		}
	
	}
	
	catch_(a);
	con->report_error(self);
	ok = false;
	cont_(a);
	return_(ok);
}

//------------------------------------------------
pbms_bool pbms_get_option(PBMS myhndl, enum pbms_option option, void *out_value)
{
	PBMS_ConHandle *con = (PBMS_ConHandle*) myhndl;
	pbms_bool ok = true;
	
	con->ms_setSelf();	
	enter_();
	
	try_(a) {

		switch (option) {
			case PBMS_OPTION_DATABASE:
				*((const char**)out_value) = con->ms_database->getCString();
				break;
			
			case PBMS_OPTION_TRANSMITION_TIMEOUT:
				*((unsigned int*)out_value) = con->ms_transmition_timeout;
				break; 
			
			case PBMS_OPTION_HOST:
				*((const char**)out_value) = con->ms_host->getCString();
				break;
				
			case PBMS_OPTION_PORT:
				*((unsigned int*)out_value) = con->ms_port;
				break;

			default:
				CSException::throwException(CS_CONTEXT, CS_ERR_GENERIC_ERROR, "Unknown Option.");
		}
	
	}
	
	catch_(a);
	con->report_error(self);
	ok = false;
	cont_(a);
	return_(ok);
}



