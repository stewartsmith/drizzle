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
 *  Created by Barry Leslie on 10/02/09.
 *
 */
#include "CSConfig.h"
#include <inttypes.h>
#include <stdlib.h>

#include <curl/curl.h>

#include "CSGlobal.h"
#include "CSString.h"
#include "CSStrUtil.h"
#include "CSEncode.h"
#include "CSS3Protocol.h"
#include "CSXML.h"

#ifdef S3_UNIT_TEST
//#define SHOW_SIGNING
// Uncomment this line to trace network action during request. Very Usefull!!
#define DEBUG_CURL
#define DUMP_ERRORS
#endif

//#define DUMP_ERRORS
//#define SHOW_SIGNING

#define HEX_CHECKSUM_VALUE_SIZE (2 *CHECKSUM_VALUE_SIZE)

#define THROW_CURL_IF(v) { if (v) CSException::throwException(CS_CONTEXT, CS_ERR_GENERIC_ERROR, ms_curl_error);}

//-------------------------------
static const char *retryCodes[] = {
	"ExpiredToken",
	"InternalError",
	"OperationAborted",
	"RequestTimeout",
	"SlowDown",
	NULL
};

//======================================
static size_t receive_data(void *ptr, size_t size, size_t nmemb, void *stream);
static size_t receive_header(void *ptr, size_t size, size_t nmemb, void *stream);
static size_t send_callback(void *ptr, size_t size, size_t nmemb, void *stream);

class S3ProtocolCon : CSXMLBuffer, public CSObject {

	private:
	
	virtual bool openNode(char *path, char *value) {
		if (value && *value && (strcmp(path,"/error/code/") == 0)) {
			printf("S3 ERROR Code: %s\n", value);
			for (int i = 0; retryCodes[i] && !ms_retry; i++)
				ms_retry = (strcmp(value, retryCodes[i]) == 0);
				
			if (ms_retry && !strcmp("slowdown", value)) 
				ms_slowDown = true;
		} else if (value && *value && (strcmp(path,"/error/message/") == 0)) {
			printf("S3 ERROR MESSAGE: %s\n", value);
		}
		return true;
	}

	virtual bool closeNode(char *path) {
		(void)path;
		return true;
	}

	virtual bool addAttribute(char *path, char *name, char *value) {
		(void)path;
		(void)name;
		(void)value;
		return true;
	}
	
	//-------------------------------
	void parse_s3_error()
	{
		enter_();

		if (!ms_errorReply)
			CSException::throwException(CS_CONTEXT, CS_ERR_GENERIC_ERROR, "Missing HTTP reply: possible S3 connection failure.");

	#ifdef DUMP_ERRORS
		printf("ms_errorReply:\n===========\n%s\n===========\n", ms_errorReply->getCString());
	#endif
		
		if (!parseData(ms_errorReply->getCString(), ms_errorReply->length(), 0)){
			int		err;
			char	*msg;

			getError(&err, &msg);
			CSException::throwException(CS_CONTEXT, CS_ERR_GENERIC_ERROR, msg);
		}
		
		exit_();
	}
	
	public:
	
	CSHTTPHeaders	ms_reply_headers;
	CSStringBuffer	ms_buffer; // A scratch buffer

	CURL			*ms_curl;	
	struct curl_slist	*ms_header_list;	// A curl list of headers to be sent with the next request.

	CSInputStream	*ms_inputStream;
	CSOutputStream	*ms_outputStream;
	
	CSMd5			ms_md5;
	char			ms_s3Checksum[HEX_CHECKSUM_VALUE_SIZE +1];
	bool			ms_calculate_md5;
	
	bool			ms_notFound; // True if the object could not be found
	bool			ms_retry; // True if the request failed with a retry error.
	bool			ms_slowDown;
	
	CSStringBuffer	*ms_errorReply;
	char			ms_curl_error[CURL_ERROR_SIZE];
	
	off64_t			ms_data_size;
	
	unsigned int	ms_replyStatus;
	bool			ms_throw_error;	// Gets set if an exception occurs in a callback.
	bool			ms_old_libcurl;
	char			*ms_safe_url;
	time_t			ms_last_modified;
	
	S3ProtocolCon():
		ms_curl(NULL),
		ms_header_list(NULL),
		ms_inputStream(NULL),
		ms_outputStream(NULL),
		ms_calculate_md5(false),
		ms_notFound(false),
		ms_retry(false),
		ms_slowDown(false),
		ms_errorReply(NULL),
		ms_data_size(0),
		ms_replyStatus(0),
		ms_throw_error(false),
		ms_old_libcurl(false),
		ms_safe_url(NULL),
		ms_last_modified(0)
	{
	
		ms_curl = curl_easy_init();
		if (!ms_curl)
			CSException::throwException(CS_CONTEXT, CS_ERR_GENERIC_ERROR, "curl_easy_init() failed.");

		curl_version_info_data *curl_ver = curl_version_info(CURLVERSION_NOW); 
		
		// libCurl versions prior to 7.17.0 did not make copies of strings passed into curl_easy_setopt()
		// If this version requirement is a problem I can do this myself, if I have to, I guess. :(
		if (curl_ver->version_num < 0X071700 ) {
			ms_old_libcurl = true;
			
			//char msg[200];
			//snprintf(msg, 200, "libcurl version %s is too old, require version 7.17.0 or newer.", curl_ver->version);
			//CSException::throwException(CS_CONTEXT, CS_ERR_GENERIC_ERROR, msg);
		}
		
		if (curl_easy_setopt(ms_curl, CURLOPT_ERRORBUFFER, ms_curl_error))
			CSException::throwException(CS_CONTEXT, CS_ERR_GENERIC_ERROR, "curl_easy_setopt(CURLOPT_ERRORBUFFER) failed.");
		
#ifdef DEBUG_CURL
		curl_easy_setopt(ms_curl, CURLOPT_VERBOSE, 1L);
#endif		
		//THROW_CURL_IF(curl_easy_setopt(ms_curl, CURLOPT_TCP_NODELAY, 1L));
	

		THROW_CURL_IF(curl_easy_setopt(ms_curl, CURLOPT_NOPROGRESS, 1L));
		THROW_CURL_IF(curl_easy_setopt(ms_curl, CURLOPT_WRITEFUNCTION, receive_data));
		THROW_CURL_IF(curl_easy_setopt(ms_curl, CURLOPT_READFUNCTION, send_callback));	
		THROW_CURL_IF(curl_easy_setopt(ms_curl, CURLOPT_HEADERFUNCTION, receive_header));
		THROW_CURL_IF(curl_easy_setopt(ms_curl, CURLOPT_WRITEDATA, this));
		THROW_CURL_IF(curl_easy_setopt(ms_curl, CURLOPT_READDATA, this));
		THROW_CURL_IF(curl_easy_setopt(ms_curl, CURLOPT_WRITEHEADER, this));
		

		THROW_CURL_IF(curl_easy_setopt(ms_curl, CURLOPT_FOLLOWLOCATION, 1L)); // Follow redirects.
	
	}
	
	~S3ProtocolCon()
	{
		if (ms_curl) 
			curl_easy_cleanup(ms_curl);			
		if (ms_header_list) 
			curl_slist_free_all(ms_header_list);			
		if (ms_inputStream)
			ms_inputStream->release();
		if (ms_outputStream)
			ms_outputStream->release();
		if (ms_errorReply)
			ms_errorReply->release();
			
		ms_reply_headers.clearHeaders();
		
		if (ms_safe_url)
			cs_free(ms_safe_url);
	}

	inline void check_reply_status() 
	{
		if (ms_replyStatus > 199 && ms_replyStatus < 300)
			return;
		
		
		
		switch (ms_replyStatus) {
			case 200:
			case 204:	// No Content
			//case 301: // Moved Permanently
			//case 307: // Temporary Redirect
				break;
			case 404:	// Not Found
			case 403:	// Forbidden (S3 object not found)
				ms_notFound = true;
				break;
			case 500:	
				ms_retry = true;
				break;
			default: {
				parse_s3_error();
				
				
				
				if (!ms_retry) {
					enter_();
					CSException::throwException(CS_CONTEXT, CS_ERR_GENERIC_ERROR, ms_errorReply->getCString());
					outer_();
				} else if (ms_slowDown) {
					enter_();
					CSException::logException(CS_CONTEXT, CS_ERR_GENERIC_ERROR, "S3 slow down request.");
					self->sleep(10); // sleep for 1/100 second.
					outer_();
				}
			}
		}
		
	}

	
	inline void ms_reset()
	{
		// Remove any old headers
		if (ms_header_list) {
			curl_slist_free_all(ms_header_list);
			ms_header_list = NULL;
		}

		ms_reply_headers.clearHeaders();
		ms_replyStatus = 0;
		ms_throw_error = false;
		if (ms_errorReply)
			ms_errorReply->setLength(0);
			
		ms_s3Checksum[0] = 0;
		ms_notFound = false;
		ms_retry = false;
		
		if (ms_outputStream) {
			ms_outputStream->release();
			ms_outputStream = NULL;
		}
		if (ms_inputStream) {
			ms_inputStream->release();
			ms_inputStream = NULL;
		}
		
		if (ms_safe_url) {
			cs_free(ms_safe_url);
			ms_safe_url = NULL;
		}
	}
	
	inline void ms_setHeader(const char *header)
	{
		ms_header_list = curl_slist_append(ms_header_list, header);
		if (!ms_header_list) 
			CSException::throwException(CS_CONTEXT, CS_ERR_GENERIC_ERROR, "curl_slist_append() failed.");
	}


private:	
	inline const char *safe_url(const char *url)
	{
		if (ms_old_libcurl == false)
			return url;
			
		if (ms_safe_url) {
			cs_free(ms_safe_url);
			ms_safe_url = NULL;
		}
		ms_safe_url = cs_strdup(url);
		return ms_safe_url;
	}
	
public:	
	inline void ms_setURL(const char *url)
	{
		//printf("URL: \"%s\n", url);
		THROW_CURL_IF(curl_easy_setopt(ms_curl, CURLOPT_URL, safe_url(url)));
	}
	
	inline void ms_execute_delete_request()
	{
		CURLcode rtc;
		
		THROW_CURL_IF(curl_easy_setopt(ms_curl, CURLOPT_HTTPHEADER, ms_header_list));
		THROW_CURL_IF(curl_easy_setopt(ms_curl, CURLOPT_CUSTOMREQUEST, "DELETE"));

		rtc = curl_easy_perform(ms_curl);
		
		THROW_CURL_IF(curl_easy_setopt(ms_curl, CURLOPT_CUSTOMREQUEST, NULL)); // IMPORTANT: Reset this to it's default value

		if (rtc && !ms_throw_error)
			CSException::throwException(CS_CONTEXT, CS_ERR_GENERIC_ERROR, ms_curl_error);
			
		if (ms_throw_error) {
			enter_();
			throw_();
			outer_();
		}
		
		check_reply_status();
	}
	
	inline void ms_execute_copy_request()
	{
		CURLcode rtc;
		
		THROW_CURL_IF(curl_easy_setopt(ms_curl, CURLOPT_HTTPHEADER, ms_header_list));
		THROW_CURL_IF(curl_easy_setopt(ms_curl, CURLOPT_INFILESIZE_LARGE, 0));
		THROW_CURL_IF(curl_easy_setopt(ms_curl, CURLOPT_UPLOAD, 1L));
		
		rtc = curl_easy_perform(ms_curl);
		
		if (rtc && !ms_throw_error)
			CSException::throwException(CS_CONTEXT, CS_ERR_GENERIC_ERROR, ms_curl_error);
			
		if (ms_throw_error) {
			enter_();
			throw_();
			outer_();
		}
		
		check_reply_status();
	}
	
	inline void ms_execute_get_request(CSOutputStream *output)
	{
		enter_();
		
		if (output) {
			push_(output);
			THROW_CURL_IF(curl_easy_setopt(ms_curl, CURLOPT_HTTPGET, 1L));
		} else {
			THROW_CURL_IF(curl_easy_setopt(ms_curl, CURLOPT_NOBODY, 1L));
		}
		
		// 
		THROW_CURL_IF(curl_easy_setopt(ms_curl, CURLOPT_FILETIME, 1L));
		THROW_CURL_IF(curl_easy_setopt(ms_curl, CURLOPT_HTTPHEADER, ms_header_list));
    // Ask curl to parse the Last-Modified header.  This is easier than
    // parsing it ourselves.

		ms_outputStream = output;	
		if (curl_easy_perform(ms_curl) && !ms_throw_error) {
			ms_outputStream = NULL;	
			CSException::throwException(CS_CONTEXT, CS_ERR_GENERIC_ERROR, ms_curl_error);
		}
		ms_outputStream = NULL;	
		if (output){
			release_(output);
		}
		
		if (ms_throw_error) 
			throw_();
		
		check_reply_status();
		curl_easy_getinfo(ms_curl, CURLINFO_FILETIME, &ms_last_modified);
		exit_();		
	}
	inline void ms_execute_put_request(CSInputStream *input, off64_t size)
	{
		enter_();
		
		push_(input);	
		THROW_CURL_IF(curl_easy_setopt(ms_curl, CURLOPT_HTTPHEADER, ms_header_list));
		THROW_CURL_IF(curl_easy_setopt(ms_curl, CURLOPT_INFILESIZE_LARGE, size));
		THROW_CURL_IF(curl_easy_setopt(ms_curl, CURLOPT_UPLOAD, 1L));
		//THROW_CURL_IF(curl_easy_setopt(ms_curl, CURLOPT_POSTFIELDSIZE_LARGE, size));
		//THROW_CURL_IF(curl_easy_setopt(ms_curl, CURLOPT_POST, 1L));

		ms_md5.md5_init();
		
		ms_data_size = size;
		ms_inputStream = input;	
		if (curl_easy_perform(ms_curl) && !ms_throw_error) {
			ms_inputStream = NULL;	
			CSException::throwException(CS_CONTEXT, CS_ERR_GENERIC_ERROR, ms_curl_error);
		}
		ms_inputStream = NULL;
		release_(input);	

			
		if (ms_throw_error)
			throw_();
		
		check_reply_status();
		
		if (ms_calculate_md5) {
			// If the data was not sent with an md5 checksum then verify
			// the server's md5 value with the one calculated during the send.
			char checksum[HEX_CHECKSUM_VALUE_SIZE +1];
			Md5Digest digest;
			
			ms_md5.md5_get_digest(&digest);
			cs_bin_to_hex(HEX_CHECKSUM_VALUE_SIZE +1, checksum, CHECKSUM_VALUE_SIZE, digest.val);
			
			cs_strToUpper(ms_s3Checksum);
			if (strcmp(checksum, ms_s3Checksum)) {
				// The request should be restarted in this case.
				ms_retry = true;
				CSException::logException(CS_CONTEXT, CS_ERR_CHECKSUM_ERROR, "Calculated checksum did not match S3 checksum");
			}
		}

		exit_();		
	}
	
};

//======================================




//======================================
CSS3Protocol::CSS3Protocol():
	s3_server(NULL),
	s3_public_key(NULL),
	s3_private_key(NULL),
	s3_maxRetries(5),
	s3_sleepTime(0)
{
	new_(s3_server, CSStringBuffer());
	s3_server->append("s3.amazonaws.com/");

	s3_public_key = CSString::newString("");
	s3_private_key = CSString::newString("");
	
}

//------------------
CSS3Protocol::~CSS3Protocol()
{
	if (s3_server)
		s3_server->release();
	
	if (s3_public_key)
		s3_public_key->release();
	
	if (s3_private_key)
		s3_private_key->release();
}
	
//------------------
CSString *CSS3Protocol::s3_getSignature(const char *verb, 
										const char *md5, 
										const char *content_type, 
										const char *date, 
										const char *bucket, 
										const char *key,
										CSString *headers 
									)
{
	CSStringBuffer *s3_buffer;
	enter_();
	if (headers)
		push_(headers);
	
	new_(s3_buffer, CSStringBuffer());
	push_(s3_buffer);
	
	s3_buffer->setLength(0);
	s3_buffer->append(verb);	
	s3_buffer->append("\n");	
	if (md5) s3_buffer->append(md5);	
	s3_buffer->append("\n");	
	if (content_type) s3_buffer->append(content_type);	
	s3_buffer->append("\n");	
	s3_buffer->append(date);
	if (headers) { 
		// Note: headers are assumed to be in lower case, sorted, and containing no white space.
		s3_buffer->append("\n");	
		s3_buffer->append(headers->getCString());
	}
	s3_buffer->append("\n/");
	s3_buffer->append(bucket);
	s3_buffer->append("/");
	s3_buffer->append(key);

#ifdef SHOW_SIGNING
printf("signing:\n=================\n%s\n=================\n", 	s3_buffer->getCString());
printf("Public Key:\"%s\"\n", 	s3_public_key->getCString());
printf("Private Key:\"%s\"\n", 	s3_private_key->getCString());
if(0){
	const char *ptr = s3_buffer->getCString();
	while (*ptr) {
		printf("%x ", *ptr); ptr++;
	}
	printf("\n");
}
#endif

	CSString *sig = signature(s3_buffer->getCString(), s3_private_key->getCString());
	release_(s3_buffer);
	if (headers) 
		release_(headers);

	return_(sig);
}
//----------------------
// CURL callback functions:
////////////////////////////
//----------------------
//-----------------
static bool try_ReadStream(CSThread *self, S3ProtocolCon *con, unsigned char *ptr, size_t buffer_size, size_t *data_sent)
{
	volatile bool rtc = true;
	try_(a) {
		*data_sent = con->ms_inputStream->read((char*)ptr, buffer_size);
		if (*data_sent <= con->ms_data_size) {
			con->ms_data_size -= *data_sent;
			if (*data_sent)
				con->ms_md5.md5_append(ptr, *data_sent); // Calculating the checksum for the data sent.
		} else if (*data_sent > con->ms_data_size) 
			CSException::RecordException(CS_CONTEXT, CS_ERR_GENERIC_ERROR, "Blob larger than expected.");
		else if (con->ms_data_size && !*data_sent)
			CSException::RecordException(CS_CONTEXT, CS_ERR_GENERIC_ERROR, "Blob smaller than expected.");
		rtc = false;
	}
	
	catch_(a)
	cont_(a);
	return rtc;
}

//----------------------
static size_t send_callback(void *ptr, size_t objs, size_t obj_size, void *v_con)
{
	S3ProtocolCon *con = (S3ProtocolCon*) v_con;
	size_t data_sent, buffer_size = objs * obj_size;

	if (!con->ms_data_size)
		return 0;
		
	enter_();
	if (try_ReadStream(self, con, (unsigned char*)ptr, buffer_size, &data_sent)) {
		con->ms_throw_error = true;
		data_sent = (size_t)-1;
	}
	
	return_(data_sent);
}

//-----------------
static bool try_WriteStream(CSThread *self, S3ProtocolCon *con, char *ptr, size_t data_len)
{
	volatile bool rtc = true;
	try_(a) {
		if (con->ms_replyStatus >= 400) { // Collect the error reply.
			if (!con->ms_errorReply)
				con->ms_errorReply = new CSStringBuffer(50);		
			con->ms_errorReply->append(ptr, data_len);
		} else if (	con->ms_outputStream)
			con->ms_outputStream->write(ptr, data_len);
		rtc = false;
	}
	
	catch_(a)
	cont_(a);
	return rtc;
}

//----------------------
static size_t receive_data(void *vptr, size_t objs, size_t obj_size, void *v_con)
{
	S3ProtocolCon *con = (S3ProtocolCon*) v_con;
	size_t data_len = objs * obj_size;

	enter_();
	if (try_WriteStream(self, con, (char*)vptr, data_len)) {
		con->ms_throw_error = true;
		data_len = (size_t)-1;
	}

	return_(data_len);	
}

#define IS_REDIRECT(s) ((s >= 300) && (s < 400))
//----------------------
static bool try_addHeader(CSThread *self, S3ProtocolCon *con, char *name, uint32_t name_len, char *value, uint32_t value_len)
{
	volatile bool rtc = true;
	
	try_(a) {
		con->ms_reply_headers.addHeader(name, name_len, value, value_len);
		rtc = false;
	}
	
	catch_(a);
	cont_(a);
	return rtc;
}

//----------------------
static size_t receive_header(void *header, size_t objs, size_t obj_size, void *v_con)
{
	S3ProtocolCon *con = (S3ProtocolCon*) v_con;
	size_t size = objs * obj_size;
	char *end, *ptr = (char*) header, *name, *value = NULL;
	uint32_t name_len =0, value_len = 0;
	
//printf(	"receive_header: %s\n", ptr);
	end = ptr + size;
	if (*(end -2) == '\r' && *(end -1) == '\n')
		end -=2;
		
	while ((end != ptr) && (*ptr == ' ')) ptr++;
	if (end == ptr)
		return size;
	
	// Get the reply status.
	// Status 100 = Continue
	if (((!con->ms_replyStatus) || (con->ms_replyStatus == 100) || IS_REDIRECT(con->ms_replyStatus) ) 
			&& !strncasecmp(ptr, "HTTP", 4)
		) {
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
	
	if (!strncasecmp(name, "ETag", 4)) {
		if (*value == '"') {
			value++; value_len -=2; // Strip quotation marks from checksum string.
		}
		if (value_len == HEX_CHECKSUM_VALUE_SIZE) {
			memcpy(con->ms_s3Checksum, value, value_len);
			con->ms_s3Checksum[value_len] = 0;
		}
	}
	
	enter_();
	if (try_addHeader(self, con, name, name_len, value, value_len)) {
		con->ms_throw_error = true;
		size = (size_t)-1;
	}
	return_(size);
}

//----------------------

#define SET_DATE_FROM_TIME(t, d) {strftime(d, sizeof(d), "%a, %d %b %Y %H:%M:%S GMT", gmtime(&t));}
#define SET_DATE(d) {time_t t = time(NULL); SET_DATE_FROM_TIME(t, d);}

bool CSS3Protocol::s3_delete(const char *bucket, const char *key)
{
 	CSStringBuffer *s3_buffer;
	char date[64];
	CSString *signed_str;
	uint32_t retry_count = 0;
	S3ProtocolCon *con_data;

	enter_();

	new_(s3_buffer, CSStringBuffer());
	push_(s3_buffer);

	new_(con_data, S3ProtocolCon());
	push_(con_data);

retry:
	// Clear old settings. 
	con_data->ms_reset();	
	
	SET_DATE(date);
 
	// Build the URL
	s3_buffer->setLength(0);
	s3_buffer->append("http://");
	s3_buffer->append(bucket);
	s3_buffer->append(".");	
	s3_buffer->append(s3_server->getCString());
	s3_buffer->append(key);

	con_data->ms_setURL(s3_buffer->getCString());
	
	// Add the 'DATE' header
	s3_buffer->setLength(0);
	s3_buffer->append("Date: ");	
	s3_buffer->append(date);
	con_data->ms_setHeader(s3_buffer->getCString());

	// Create the authentication signature and add the 'Authorization' header
	signed_str = s3_getSignature("DELETE", NULL, NULL, date, bucket, key);
	push_(signed_str);
	s3_buffer->setLength(0);
	s3_buffer->append("Authorization: AWS ");	
	s3_buffer->append(s3_public_key->getCString());
	s3_buffer->append(":");	
	s3_buffer->append(signed_str->getCString());	
	release_(signed_str); signed_str = NULL;
	
	con_data->ms_setHeader(s3_buffer->getCString());
	
	con_data->ms_execute_delete_request();
	
	if (con_data->ms_retry) {
		if (retry_count == s3_maxRetries) {
			CSException::throwException(CS_CONTEXT, CS_ERR_GENERIC_ERROR, "S3 operation aborted after max retries.");
		}
	//printf("RETRY: s3_delete()\n");
		retry_count++;
		self->sleep(s3_sleepTime);
		goto retry;
	}
	
	bool notFound = con_data->ms_notFound;
	release_(con_data);
	release_(s3_buffer);
	
	return_(!notFound);
}

//-------------------------------
void CSS3Protocol::s3_copy(const char *dest_server, const char *dest_bucket, const char *dest_key, const char *src_bucket, const char *src_key)
{
  	CSStringBuffer *s3_buffer;
	char date[64];
	CSString *signed_str;
	uint32_t retry_count = 0;
	S3ProtocolCon *con_data;

	enter_();

	new_(s3_buffer, CSStringBuffer());
	push_(s3_buffer);

	new_(con_data, S3ProtocolCon());
	push_(con_data);
	
	if (!dest_server)
		dest_server = s3_server->getCString();

retry:
	// Clear old settings. 
	con_data->ms_reset();	
	
	SET_DATE(date);
 
	// Build the URL
	s3_buffer->setLength(0);
	s3_buffer->append("http://");
	s3_buffer->append(dest_bucket);
	s3_buffer->append(".");	
	s3_buffer->append(s3_server->getCString());
	s3_buffer->append(dest_key);

	con_data->ms_setURL(s3_buffer->getCString());
	
	// Add the destination location
	s3_buffer->setLength(0);
	s3_buffer->append("Host: ");	
	s3_buffer->append(dest_bucket);
	s3_buffer->append(".");	
	s3_buffer->append(dest_server);
	s3_buffer->setLength(s3_buffer->length() -1); // trim the '/'
	con_data->ms_setHeader(s3_buffer->getCString());
	
	// Add the source location
	s3_buffer->setLength(0);
	s3_buffer->append("x-amz-copy-source:");	
	s3_buffer->append(src_bucket);
	s3_buffer->append("/");
	s3_buffer->append(src_key);
	con_data->ms_setHeader(s3_buffer->getCString());
	
	// Create the authentication signature and add the 'Authorization' header
	signed_str = s3_getSignature("PUT", NULL, NULL, date, dest_bucket, dest_key, CSString::newString(s3_buffer->getCString()));
	push_(signed_str);

	// Add the 'DATE' header
	s3_buffer->setLength(0);
	s3_buffer->append("Date: ");	
	s3_buffer->append(date);
	con_data->ms_setHeader(s3_buffer->getCString());

	// Add the signature
	s3_buffer->setLength(0);
	s3_buffer->append("Authorization: AWS ");	
	s3_buffer->append(s3_public_key->getCString());
	s3_buffer->append(":");	
	s3_buffer->append(signed_str->getCString());	
	release_(signed_str); signed_str = NULL;
	con_data->ms_setHeader(s3_buffer->getCString());
	
	con_data->ms_execute_copy_request();
	
	if (con_data->ms_notFound) {
		s3_buffer->setLength(0);
		s3_buffer->append("Cloud copy failed, object not found: ");
		s3_buffer->append(src_bucket);
		s3_buffer->append(" ");
		s3_buffer->append(src_key);
		CSException::throwException(CS_CONTEXT, CS_ERR_GENERIC_ERROR, s3_buffer->getCString());
	}
	
	if (con_data->ms_retry) {
		if (retry_count == s3_maxRetries) {
			CSException::throwException(CS_CONTEXT, CS_ERR_GENERIC_ERROR, "S3 operation aborted after max retries.");
		}
	//printf("RETRY: s3_copy()\n");
		retry_count++;
		self->sleep(s3_sleepTime);
		goto retry;
	}
	
	release_(con_data);
	release_(s3_buffer);
	
	exit_();
}


//-------------------------------
CSVector *CSS3Protocol::s3_receive(CSOutputStream *output, const char *bucket, const char *key, bool *found, S3RangePtr range, time_t *last_modified)
{
 	CSStringBuffer *s3_buffer;
    char date[64];
	CSString *signed_str;
	uint32_t retry_count = 0;
	S3ProtocolCon *con_data;
	CSVector *replyHeaders;
	CSString *range_header = NULL;
	const char *http_op;

	enter_();

	if (output) {
		push_(output);
		http_op = "GET";
	} else
		http_op = "HEAD";

	new_(s3_buffer, CSStringBuffer());
	push_(s3_buffer);

	new_(con_data, S3ProtocolCon());
	push_(con_data);

retry:
	// Clear old settings. 
	con_data->ms_reset();	
	
	SET_DATE(date);
 
	// Build the URL
	s3_buffer->setLength(0);
	s3_buffer->append("http://");
	s3_buffer->append(bucket);
	s3_buffer->append(".");	
	s3_buffer->append(s3_server->getCString());
	s3_buffer->append(key);

	con_data->ms_setURL(s3_buffer->getCString());
	
	// Add the 'DATE' header
	s3_buffer->setLength(0);
	s3_buffer->append("Date: ");	
	s3_buffer->append(date);
	con_data->ms_setHeader(s3_buffer->getCString());

	if (range) {
		char buffer[80];
		snprintf(buffer, 80,"Range: bytes=%"PRIu64"-%"PRIu64, range->startByte, range->endByte);

		range_header = CSString::newString(buffer);
	}
	// Create the authentication signature and add the 'Authorization' header
	if (range_header)
		con_data->ms_setHeader(range_header->getCString());
	signed_str = s3_getSignature(http_op, NULL, NULL, date, bucket, key, NULL);
	push_(signed_str);
	s3_buffer->setLength(0);
	s3_buffer->append("Authorization: AWS ");	
	s3_buffer->append(s3_public_key->getCString());
	s3_buffer->append(":");	
	s3_buffer->append(signed_str->getCString());	
	release_(signed_str); signed_str = NULL;
	con_data->ms_setHeader(s3_buffer->getCString());
	
	if (output) output->retain();
	con_data->ms_execute_get_request(output);
	
	if (con_data->ms_retry) {
		if (retry_count == s3_maxRetries) {
			CSException::throwException(CS_CONTEXT, CS_ERR_GENERIC_ERROR, "S3 operation aborted after max retries.");
		}
	//printf("RETRY: s3_receive()\n");
		retry_count++;
		output->reset();
		self->sleep(s3_sleepTime);
		goto retry;
	}
	
	if (last_modified)
		*last_modified = con_data->ms_last_modified;
	*found = !con_data->ms_notFound;
	replyHeaders = con_data->ms_reply_headers.takeHeaders();
	release_(con_data);
	release_(s3_buffer);
	if (output)
		release_(output);
	
	return_(replyHeaders);
}

class S3ListParser : public CSXMLBuffer {

	CSVector *list;
	public:


	bool parseListData(const char *data, size_t len, CSVector *keys)
	{
		list = keys;
		return parseData(data, len, 0);
	}

	private:
	virtual bool openNode(char *path, char *value) {
		if (value && *value && (strcmp(path,"/listbucketresult/contents/key/") == 0))
			list->add(CSString::newString(value));
		return true;
	}

	virtual bool closeNode(char *path) {
		(void)path;
		return true;
	}

	virtual bool addAttribute(char *path, char *name, char *value) {
		(void)path;
		(void)name;
		(void)value;
		return true;
	}

};

//-------------------------------
static CSVector *parse_s3_list(CSMemoryOutputStream *output)
{
	S3ListParser s3ListParser;
	const char *data;
	CSVector *vector;
	size_t len;
	
	enter_();

	push_(output);
	
	new_(vector, CSVector(10));	
	push_(vector);	

	data = (const char *) output->getMemory(&len);
	if (!s3ListParser.parseListData(data, len, vector)) {
		int		err;
		char	*msg;

		s3ListParser.getError(&err, &msg);
		CSException::throwException(CS_CONTEXT, CS_ERR_GENERIC_ERROR, msg);
	}

	pop_(vector);
	release_(output);
	return_(vector);
}


//-------------------------------
CSVector *CSS3Protocol::s3_list(const char *bucket, const char *key_prefix, uint32_t max)
{
 	CSStringBuffer *s3_buffer;
    char date[64];
	CSString *signed_str;
	CSMemoryOutputStream *output;
	uint32_t retry_count = 0;
	S3ProtocolCon *con_data;
	enter_();

	new_(s3_buffer, CSStringBuffer());
	push_(s3_buffer);

	output = CSMemoryOutputStream::newStream(1024, 1024);
	push_(output);
	
	new_(con_data, S3ProtocolCon());
	push_(con_data);

retry:

	// Clear old settings. 
	con_data->ms_reset();	
	
	SET_DATE(date);
 
	// Build the URL
	s3_buffer->setLength(0);
	s3_buffer->append("http://");
	s3_buffer->append(bucket);
	s3_buffer->append(".");	
	s3_buffer->append(s3_server->getCString());
//s3_buffer->append("/");	
//s3_buffer->append(bucket);
	if (key_prefix) {
		s3_buffer->append("?prefix=");
		s3_buffer->append(key_prefix);
	}
	
	if (max) {
		if (key_prefix)
			s3_buffer->append("&max-keys=");
		else
			s3_buffer->append("?max-keys=");
		s3_buffer->append(max);
	}

	con_data->ms_setURL(s3_buffer->getCString());
	
	// Add the 'DATE' header
	s3_buffer->setLength(0);
	s3_buffer->append("Date: ");	
	s3_buffer->append(date);
	con_data->ms_setHeader(s3_buffer->getCString());

	// Create the authentication signature and add the 'Authorization' header
	signed_str = s3_getSignature("GET", NULL, NULL, date, bucket, "");
	push_(signed_str);
	s3_buffer->setLength(0);
	s3_buffer->append("Authorization: AWS ");	
	s3_buffer->append(s3_public_key->getCString());
	s3_buffer->append(":");	
	s3_buffer->append(signed_str->getCString());	
	release_(signed_str); signed_str = NULL;
	con_data->ms_setHeader(s3_buffer->getCString());
	
	con_data->ms_execute_get_request(RETAIN(output));
	
	if (con_data->ms_retry) {
		if (retry_count == s3_maxRetries) {
			CSException::throwException(CS_CONTEXT, CS_ERR_GENERIC_ERROR, "S3 operation aborted after max retries.");
		}
	//printf("RETRY: s3_list()\n");
		retry_count++;
		output->reset();
		self->sleep(s3_sleepTime);
		goto retry;
	}
	
	release_(con_data);
	pop_(output);
	release_(s3_buffer);
	return_(parse_s3_list(output));
}

//-------------------------------
CSString *CSS3Protocol::s3_getAuthorization(const char *bucket, const char *key, const char *content_type, uint32_t *s3AuthorizationTime)
{
    char date[64];
	CSString *signed_str;
	time_t sys_time;

	enter_();

	if (!content_type)
		content_type = "binary/octet-stream";
		
	sys_time = time(NULL);
	
	*s3AuthorizationTime = (uint32_t)sys_time;
	
	SET_DATE_FROM_TIME(sys_time, date);
	signed_str = s3_getSignature("PUT", NULL, content_type, date, bucket, key);
	return_(signed_str);
}

//-------------------------------
CSVector *CSS3Protocol::s3_send(CSInputStream *input, const char *bucket, const char *key, off64_t size, const char *content_type, Md5Digest *digest, const char *s3Authorization, time_t s3AuthorizationTime)
{
 	CSStringBuffer *s3_buffer;
    char date[64];
	CSString *signed_str;
	uint32_t retry_count = 0;
	S3ProtocolCon *con_data;
	CSVector *replyHeaders;
	char checksum[32], *md5 = NULL;

	enter_();
	push_(input);

	new_(s3_buffer, CSStringBuffer());
	push_(s3_buffer);

	new_(con_data, S3ProtocolCon());
	push_(con_data);
		
	if (!content_type)
		content_type = "binary/octet-stream";
		
retry:

	// Clear old settings. 
	con_data->ms_reset();	
	
	if (s3Authorization) {
		SET_DATE_FROM_TIME(s3AuthorizationTime, date);
	} else {
		SET_DATE(date);
	}
	
	// Build the URL
	s3_buffer->setLength(0);
	s3_buffer->append("http://");
	s3_buffer->append(bucket);
	s3_buffer->append(".");	
	s3_buffer->append(s3_server->getCString());
	s3_buffer->append(key);

	con_data->ms_setURL(s3_buffer->getCString());
	
	// Add the 'DATE' header
	s3_buffer->setLength(0);
	s3_buffer->append("Date: ");	
	s3_buffer->append(date);
	con_data->ms_setHeader(s3_buffer->getCString());
	
	// Add the 'Content-Type' header
	s3_buffer->setLength(0);
	s3_buffer->append("Content-Type: ");	
	s3_buffer->append(content_type);
	con_data->ms_setHeader(s3_buffer->getCString());
		
	if (digest) {
		// Add the Md5 checksum header
		md5 = checksum;
		memset(checksum, 0, 32);
		base64Encode(digest->val, 16, checksum, 32);
		
		s3_buffer->setLength(0);
		s3_buffer->append("Content-MD5: ");	
		s3_buffer->append(checksum);
		con_data->ms_setHeader(s3_buffer->getCString());		
		con_data->ms_calculate_md5 = false;
	} else 
		con_data->ms_calculate_md5 = true;
	

	// Create the authentication signature and add the 'Authorization' header
	if (!s3Authorization)
		signed_str = s3_getSignature("PUT", md5, content_type, date, bucket, key);
	else
		signed_str = CSString::newString(s3Authorization);
	push_(signed_str);
	s3_buffer->setLength(0);
	s3_buffer->append("Authorization: AWS ");	
	s3_buffer->append(s3_public_key->getCString());
	s3_buffer->append(":");	
	s3_buffer->append(signed_str->getCString());	
	release_(signed_str); signed_str = NULL;
	con_data->ms_setHeader(s3_buffer->getCString());
	
	con_data->ms_execute_put_request(RETAIN(input), size);
	
	if (con_data->ms_retry) {
		if (retry_count == s3_maxRetries) {
			CSException::throwException(CS_CONTEXT, CS_ERR_GENERIC_ERROR, "S3 operation aborted after max retries.");
		}
	//printf("RETRY: s3_send()\n");
		retry_count++;
		input->reset();
		self->sleep(s3_sleepTime);
		goto retry;
	}
	
	replyHeaders = con_data->ms_reply_headers.takeHeaders();

	release_(con_data);
	release_(s3_buffer);
	release_(input);
	return_(replyHeaders);
}

//-------------------------------
CSString *CSS3Protocol::s3_getDataURL(const char *bucket, const char *key, uint32_t keep_alive)
{
 	CSStringBuffer *s3_buffer;
	char timeout[32];
	CSString *signed_str;
	enter_();
	
	new_(s3_buffer, CSStringBuffer());
	push_(s3_buffer);

	snprintf(timeout, 32, "%"PRId32"", ((uint32_t)time(NULL)) + keep_alive);
	
	signed_str = s3_getSignature("GET", NULL, NULL, timeout, bucket, key);
//printf("Unsafe: \"%s\"\n", signed_str->getCString());
	signed_str = urlEncode(signed_str); // Because the signature is in the URL it must be URL encoded.
//printf("  Safe: \"%s\"\n", signed_str->getCString());
	push_(signed_str);
	
	s3_buffer->setLength(0);	
	s3_buffer->append("http://");
	s3_buffer->append(bucket);
	s3_buffer->append(".");	
	s3_buffer->append(s3_server->getCString());
	s3_buffer->append(key);

	s3_buffer->append("?AWSAccessKeyId=");
	s3_buffer->append(s3_public_key->getCString());
	s3_buffer->append("&Expires=");
	s3_buffer->append(timeout);
	s3_buffer->append("&Signature=");
	s3_buffer->append(signed_str->getCString());
	
	release_(signed_str);
	
	pop_(s3_buffer);
	CSString *str = CSString::newString(s3_buffer);
	return_(str);	
}

//#define S3_UNIT_TEST
#ifdef S3_UNIT_TEST
static void show_help_info(const char *cmd)
{
	printf("Get authenticated query string:\n\t%s q <bucket> <object_key> <timeout>\n", cmd);
	printf("Delete object:\n\t%s d <bucket> <object_key>\n", cmd);
	printf("Delete all object with a given prefix:\n\t%s D <bucket> <object_prefix>\n", cmd);
	printf("Get object, data will be written to 'prottest.out':\n\t%s g <bucket> <object_key> <timeout>\n", cmd);
	printf("Get object header only:\n\t%s h <bucket> <object_key> <timeout>\n", cmd);
	printf("Put (Upload) an object:\n\t%s p <bucket> <object_key> <file>\n", cmd);
	printf("List objects in the bucket:\n\t%s l <bucket> [<object_prefix> [max_list_size]]\n", cmd);
	printf("Copy object:\n\t%s c <src_bucket> <src_object_key> <dst_bucket> <dst_object_key> \n", cmd);
	printf("Copy all object with a given prefix:\n\t%s C <src_bucket> <object_key_prefix> <dst_bucket> \n", cmd);
}

void dump_headers(CSVector *header_array)
{
	CSHTTPHeaders headers;
	
	headers.setHeaders(header_array);
	printf("Reply Headers:\n");
	printf("--------------\n");
	
	for (uint32_t i = 0; i < headers.numHeaders(); i++) {
		CSHeader *h = headers.getHeader(i);
		
		printf("%s : %s\n", h->getNameCString(), h->getValueCString());
		h->release();
	}
	printf("--------------\n");
	headers.clearHeaders();
}

int main(int argc, char **argv)
{
	CSThread *main_thread;
	const char *pub_key;
	const char *priv_key;
	const char *server;
	CSS3Protocol *prot = NULL;
	
	if (argc < 3) {
		show_help_info(argv[0]);
		return 0;
	}
	
	if (! CSThread::startUp()) {
		CSException::throwException(CS_CONTEXT, ENOMEM, "CSThread::startUp() failed.");
		return 1;
	}
	
	cs_init_memory();
	
	main_thread = new CSThread( NULL);
	CSThread::setSelf(main_thread);
	
	enter_();
	try_(a) {
	
		pub_key = getenv("S3_ACCESS_KEY_ID");
		priv_key = getenv("S3_SECRET_ACCESS_KEY");
		new_(prot, CSS3Protocol());
		push_(prot);
		
		server = getenv("S3_SERVER");
		if ((server == NULL) || (*server == 0))
			server = "s3.amazonaws.com/";
		prot->s3_setServer(server);
		prot->s3_setPublicKey(pub_key);
		prot->s3_setPrivateKey(priv_key);
		prot->s3_setMaxRetries(0);
		
		switch (argv[1][0]) {
			case 'q': // Get the query string
				if (argc == 5) {
					CSString *qstr = prot->s3_getDataURL(argv[2], argv[3], atoi(argv[4]));
					printf("To test call:\ncurl -L -D - \"%s\"\n", qstr->getCString());
					qstr->release();
				} else
					printf("Bad command: q <bucket> <object_key> <timeout>\n");
				
				break;
			case 'd': // Delete the object
				if (argc == 4) {
					printf("delete %s %s\n", argv[2], argv[3]);
					if (!prot->s3_delete(argv[2], argv[3]))
						printf("%s/%s could not be found.\n", argv[2], argv[3]);

				} else
					printf("Bad command: d <bucket> <object_key>\n");
				
				break;
			case 'D': // Delete  objects like
				if (argc == 4) {
					CSVector *list;
					CSString *key;
					
					list = prot->s3_list(argv[2], argv[3]);
					push_(list);
					while (key = (CSString*) list->take(0)) {
						printf("Deleting %s\n", key->getCString());
						prot->s3_delete(argv[2], key->getCString());
						key->release();
					}
					release_(list);
					
				} else
					printf("Bad command: D <bucket> <object_key_prefix>\n");
				
				break;
			case 'g':  // Get the object
				if ((argc == 4) || (argc == 6)) {
					CSFile *output;	
					CSVector *headers;
					bool found;				
					S3RangeRec *range_ptr = NULL, range = 	{0,0};		
					
					if (argc == 6) {
						range.startByte = atoi(argv[4]);
						range.endByte = atoi(argv[5]);
						range_ptr = &range;
					}
					
					output = CSFile::newFile("prottest.out");
					push_(output);
					output->open(CSFile::CREATE | CSFile::TRUNCATE);
					headers = prot->s3_receive(output->getOutputStream(), argv[2], argv[3], &found, range_ptr);
					if (!found)
						printf("%s/%s could not be found.\n", argv[2], argv[3]);
						
					dump_headers(headers);
						
					release_(output);
				} else
					printf("Bad command: g <bucket> <object_key>\n");
				
				break;
				
			case 'h':  // Get the object header
				if (argc == 4) {
					CSVector *headers;
					bool found;	
					S3RangeRec range = 	{0,0};		
					
					headers = prot->s3_receive(NULL, argv[2], argv[3], &found);
					if (!found)
						printf("%s/%s could not be found.\n", argv[2], argv[3]);
						
					dump_headers(headers);
						
				} else
					printf("Bad command: h <bucket> <object_key>\n");
				
				break;
				
			case 'p':  // Put (Upload) the object
				if (argc == 5) {
					CSFile *input;
					Md5Digest digest;
					CSVector *headers;
					
					input = CSFile::newFile(argv[4]);
					push_(input);
					input->open(CSFile::READONLY);
					input->md5Digest(&digest);
					headers = prot->s3_send(input->getInputStream(), argv[2], argv[3], input->myFilePath->getSize(), NULL, &digest);
					dump_headers(headers);
					release_(input);
				} else
					printf("Bad command: p <bucket> <object_key> <file> \n");
				
				break;
				
			case 'c':  // Copy the object
				if (argc == 6) {
					prot->s3_copy(NULL, argv[4], argv[5], argv[2], argv[3]);
				} else
					printf("Bad command: c <src_bucket> <src_object_key> <dst_bucket> <dst_object_key>\n");
				
				break;
				
			case 'C':  // Copy  objects like
				if (argc == 5) {
					CSVector *list;
					CSString *key;
					
					list = prot->s3_list(argv[2], argv[3]);
					push_(list);
					while (key = (CSString*) list->take(0)) {
						printf("Copying %s\n", key->getCString());
						prot->s3_copy(NULL, argv[4], key->getCString(), argv[2], key->getCString());
						key->release();
					}
					release_(list);
					
				} else
					printf("Bad command: C <src_bucket> <object_key_prefix> <dst_bucket>\n");
				
				break;
			case 'l':  // List the object
				if ((argc == 3) || (argc == 4) || (argc == 5)) {
					uint32_t max = 0;
					char *prefix = NULL;
					CSVector *list;
					CSString *key;
					
					if (argc > 3) {
						prefix = argv[3];
						if (!strlen(prefix))
							prefix = NULL;
					}
					
					if (argc == 5) 
						max = atol(argv[4]);
						
					list = prot->s3_list(argv[2], prefix, max);
					push_(list);
					while (key = (CSString*) list->take(0)) {
						printf("%s\n", key->getCString());
						key->release();
					}
					release_(list);
					
				} else
					printf("Bad command: l <bucket> [<object_prefix> [max_list_size]] \n");
				
				break;
			default:
				printf("Unknown command.\n");
				show_help_info(argv[0]);
		}
		
		release_(prot);
	}
	
	catch_(a);		
	self->logException();
	
	cont_(a);
		
	outer_()
	main_thread->release();
	cs_exit_memory();
	CSThread::shutDown();
	return 0;
}

#endif


