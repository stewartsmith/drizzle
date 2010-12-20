/* Copyright (C) 2010 PrimeBase Technologies GmbH, Germany
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
 * Author: Barry Leslie
 *
 * 2010-02-05
 *
 * CORE SYSTEM:
 * Basic UNIX specific file I/O classes.
 *
 */

#include "CSConfig.h"

#include <io.h>

#include "CSGlobal.h"
#include "CSDefs.h"
#include "CSStrUtil.h"
#include "CSSys.h"

#define CS_MASK		((S_IRUSR | S_IWUSR) | (S_IRGRP | S_IWGRP) | (S_IROTH))
//=====================
// CSSysFile
//=====================
static int get_win_error()
{
	return (int) GetLastError();
}

bool CSSysFile::isDirNotFound(CSException *e) { return e->getErrorCode() == ERROR_PATH_NOT_FOUND; }
bool CSSysFile::isFileNotFound(CSException *e) { return e->getErrorCode() == ERROR_FILE_NOT_FOUND; }
bool CSSysFile::isDirExists(CSException *e) { return e->getErrorCode() == ERROR_ALREADY_EXISTS; }


//--------------
void CSSysFile::sf_open(const char *path, bool readonly, bool create)
{
	SECURITY_ATTRIBUTES	sa = { sizeof(SECURITY_ATTRIBUTES), 0, 0 };
	DWORD				flags;

	flags = (create)?OPEN_ALWAYS:OPEN_EXISTING;
	
	if (sf_fh != INVALID_HANDLE_VALUE)
		sf_close();
	
	sf_path = CSString::newString(path);
	
	sf_fh = CreateFileA(
		path,
		readonly ? GENERIC_READ : (GENERIC_READ | GENERIC_WRITE),
		FILE_SHARE_READ | FILE_SHARE_WRITE,
		&sa,
		flags,
		FILE_FLAG_RANDOM_ACCESS,
		NULL);
		
	if (sf_fh == INVALID_HANDLE_VALUE) {
		sf_path->release();
		sf_path = NULL;
		CSException::throwFileError(CS_CONTEXT, path, get_win_error());
	}
}

//--------------
void CSSysFile::sf_close()
{
	if (sf_fh != INVALID_HANDLE_VALUE) {
		CloseHandle(sf_fh);
		sf_fh = INVALID_HANDLE_VALUE;
		sf_path->release();
		sf_path = NULL;
	}	
}

//--------------
size_t CSSysFile::sf_pread(void *data, size_t size, off64_t offset)
{
	LARGE_INTEGER	liDistanceToMove;
	DWORD			result;

	liDistanceToMove.QuadPart = offset;
	if (!SetFilePointerEx(sf_fh, liDistanceToMove, NULL, FILE_BEGIN))
		CSException::throwFileError(CS_CONTEXT, sf_path->getCString(), get_win_error());

	if (!ReadFile(sf_fh, data, size, &result, NULL))
		CSException::throwFileError(CS_CONTEXT, sf_path->getCString(), get_win_error());

	return (size_t) result;
}

//--------------
void CSSysFile::sf_pwrite(const void *data, size_t size, off64_t offset)
{
	LARGE_INTEGER	liDistanceToMove;
	DWORD			result;
	
	liDistanceToMove.QuadPart = offset;
	if (!SetFilePointerEx(sf_fh, liDistanceToMove, NULL, FILE_BEGIN))
		CSException::throwFileError(CS_CONTEXT, sf_path->getCString(), get_win_error());

	if (!WriteFile(sf_fh, data, size, &result, NULL))
		CSException::throwFileError(CS_CONTEXT, sf_path->getCString(), get_win_error());

	if (result != size)
		CSException::throwFileError(CS_CONTEXT, sf_path->getCString(), ERROR_HANDLE_EOF);
	
}

//--------------
void CSSysFile::sf_setEOF(off64_t offset)
{
	LARGE_INTEGER liDistanceToMove;
	
	liDistanceToMove.QuadPart = offset;
	if (!SetFilePointerEx(sf_fh, liDistanceToMove, NULL, FILE_BEGIN)) 
		CSException::throwFileError(CS_CONTEXT, sf_path->getCString(), get_win_error());

	if (!SetEndOfFile(sf_fh)) 
		CSException::throwFileError(CS_CONTEXT, sf_path->getCString(), get_win_error());
}

//--------------
off64_t CSSysFile::sf_getEOF()
{
	DWORD			result;
	LARGE_INTEGER	lpFileSize;

	result = SetFilePointer(sf_fh, 0, NULL, FILE_END);
	if (result == 0xFFFFFFFF)  
		CSException::throwFileError(CS_CONTEXT, sf_path->getCString(), get_win_error());

	if (!GetFileSizeEx(sf_fh, &lpFileSize))  
		CSException::throwFileError(CS_CONTEXT, sf_path->getCString(), get_win_error());

	return lpFileSize.QuadPart;
}

//--------------
void CSSysFile::sf_sync()
{
	if (!FlushFileBuffers(sf_fh))  
		CSException::throwFileError(CS_CONTEXT, sf_path->getCString(), get_win_error());
}

//--------------
void CSSysFile::sf_lock(bool shared)
{
	OVERLAPPED overlap = {0};
	
	if (!LockFileEx(sf_fh, (shared)? 0: LOCKFILE_EXCLUSIVE_LOCK, 0, 512, 0, &overlap))  
		CSException::throwFileError(CS_CONTEXT, sf_path->getCString(), get_win_error());
}

//--------------
void CSSysFile::sf_unlock()
{
	OVERLAPPED overlap = {0};

	if (!UnlockFileEx(sf_fh, 0, 512, 0, &overlap))  
		CSException::throwFileError(CS_CONTEXT, sf_path->getCString(), get_win_error());
}

//=====================
// CSSys
//=====================
//--------------
bool CSSys::sys_exists(const char *path)
{
	 return (access(path, 0) != -1);
}

//--------------
void CSSys::sys_getcwd(char *path, size_t size)
{
	DWORD	len;

	len = GetCurrentDirectoryA(size, path);
	if (len == 0)
		CSException::throwFileError(CS_CONTEXT, "GetCurrentDirectory()" , get_win_error());
	else if (len > (size -1))
		CSException::throwFileError(CS_CONTEXT, "GetCurrentDirectory()overflow " , len);

}

//--------------
void CSSys::sys_setcwd(const char *path)
{
	if (!SetCurrentDirectoryA(path))
		CSException::throwFileError(CS_CONTEXT, "SetCurrentDirectory()" , get_win_error());
}

//--------------
void CSSys::sys_makeDir(const char *path)
{
	SECURITY_ATTRIBUTES	sa = { sizeof(SECURITY_ATTRIBUTES), 0, 0 };

	if (!CreateDirectoryA(path, &sa))
		CSException::throwFileError(CS_CONTEXT, path, get_win_error());

}

#define FILE_NOT_FOUND(x)		((x) == ERROR_FILE_NOT_FOUND || (x) == ERROR_PATH_NOT_FOUND)
//--------------
void CSSys::sys_removeDir(const char *path)
{
	if (!RemoveDirectoryA(path)) {
		int err = get_win_error();

		if (!FILE_NOT_FOUND(err)) 
			CSException::throwFileError(CS_CONTEXT, path, err);
	}
}

//--------------
void CSSys::sys_removeFile(const char *path)
{
	if (!DeleteFileA(path)) {
		int err = get_win_error();

		if (!FILE_NOT_FOUND(err)) 
			CSException::throwFileError(CS_CONTEXT, path, err);
	}
}


//--------------

void CSSys::sys_stat(const char *path, bool *is_dir, off64_t *size, CSTime *mod_time)
{
	HANDLE						fh;
	BY_HANDLE_FILE_INFORMATION	info;
	SECURITY_ATTRIBUTES			sa = { sizeof(SECURITY_ATTRIBUTES), 0, 0 };

	fh = CreateFileA(
		path,
		0,
		FILE_SHARE_READ,
		&sa,
		OPEN_EXISTING,
		FILE_ATTRIBUTE_NORMAL | FILE_FLAG_BACKUP_SEMANTICS, // FILE_FLAG_BACKUP_SEMANTICS allows you to open directories.
		NULL);
		
	if (fh == INVALID_HANDLE_VALUE) {
		CSException::throwFileError(CS_CONTEXT, path, get_win_error());
	}

	if (!GetFileInformationByHandle(fh, &info)) {
		CloseHandle(fh);
		CSException::throwFileError(CS_CONTEXT, path, get_win_error());
	}

	CloseHandle(fh);
	if (is_dir)
		*is_dir = ((info.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) == FILE_ATTRIBUTE_DIRECTORY);

	if (size)
		*size = (off64_t) info.nFileSizeLow | (((off64_t) info.nFileSizeHigh) << 32);
		
	if (mod_time) {
		SYSTEMTIME st;
		FileTimeToSystemTime(&info.ftLastWriteTime, &st);
		mod_time->setUTC(st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond, st.wMilliseconds * 1000000);
	}

}

//--------------
bool CSSys::sys_isLink(const char *path)
{
	CSException::throwFileError(CS_CONTEXT, "CSSys::sys_isLink() not implimented on windows.", -1);
	return false;	
}

//--------------
void CSSys::sys_rename(const char *old_path, const char *new_path)
{
	 if (rename(old_path, new_path) == -1)
		CSException::throwFileError(CS_CONTEXT, old_path, errno);
}

//--------------
uint32_t CSSys::sys_getpid()
{
	return GetCurrentProcessId();
}

//--------------
bool CSSys::sys_isAlive(uint32_t pid)
{
	HANDLE h;
	bool isAlive = false;
	DWORD code;

	h = OpenProcess(PROCESS_QUERY_INFORMATION, FALSE, pid);
	if (h) {
		if (GetExitCodeProcess(h, &code) && (code == STILL_ACTIVE))
			isAlive = true;
			
		CloseHandle(h);
	} else {
		int err;

		err = HRESULT_CODE(GetLastError());
		if (err != ERROR_INVALID_PARAMETER)
			isAlive = true;
		else
			fprintf(stderr, "ERROR CSSys::sys_isAlive(%d):OpenProcess %d\n", pid, err);
	}

	return(isAlive);	
}



//=====================
// CSSysDir
//=====================
CSSysDir::~CSSysDir()
{
	close();
	if (sd_path)
		sd_path->release();
		
	if (sd_filter)
		sd_filter->release();
}

//--------------
void CSSysDir::open()
{
	enter_();

	if (! CSSys::sys_exists(sd_path->getCString()))
		CSException::throwFileError(CS_CONTEXT, sd_path->getCString(), ERROR_PATH_NOT_FOUND);

	sd_filter = new CSStringBuffer();
	sd_filter->append(sd_path->getCString());

	if (IS_DIR_CHAR(*(sd_filter->getBuffer(sd_filter->length()-1))))
		sd_filter->append("*");
	else
		sd_filter->append(CS_DIR_DELIM"*");

	exit_();
}

//--------------
void CSSysDir::close()
{
	if (sd_dir != INVALID_HANDLE_VALUE) {
		FindClose(sd_dir);
		sd_dir = INVALID_HANDLE_VALUE;
	}
}

//--------------
bool CSSysDir::next()
{
	int err = 0;

	while (true) {
		if (sd_dir == INVALID_HANDLE_VALUE) {
			sd_dir = FindFirstFileA(sd_filter->getCString(), &sd_entry);
			if (sd_dir == INVALID_HANDLE_VALUE)
				err = get_win_error();
		}
		else {
			if (!FindNextFileA(sd_dir, &sd_entry))
				err = get_win_error();
		}

		if (err) {
			if ((err != ERROR_NO_MORE_FILES) && (err != ERROR_FILE_NOT_FOUND)){
				CSException::throwFileError(CS_CONTEXT, sd_path->getCString(), err);
			}
			return false;
		}

		/* Filter out '.' and '..': */
		if (sd_entry.cFileName[0] == '.') {
			if (sd_entry.cFileName[1] == '.') {
				if (sd_entry.cFileName[2] == '\0')
					continue;
			}
			else {
				if (sd_entry.cFileName[1] == '\0')
					continue;
			}
		}
		break;
	}

	return true;
}


//--------------
void CSSysDir::getEntryPath(char *path, size_t size)
{
	cs_strcpy(size, path, sd_path->getCString());
	cs_add_dir_char(size, path);
	cs_strcat(size, path, entryName());
}

//--------------
const char *CSSysDir::entryName()
{
	return (const char*) sd_entry.cFileName;
}

//--------------
bool CSSysDir::entryIsFile()
{
	if (sd_entry.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
		return false;
	return true;
}

///////////////////////////////////////////
// A windows version of gettimeofday() as taken from:
// http://www.suacommunity.com/dictionary/gettimeofday-entry.php

#if defined(_MSC_VER) || defined(_MSC_EXTENSIONS)
  #define DELTA_EPOCH_IN_MICROSECS  11644473600000000Ui64
#else
  #define DELTA_EPOCH_IN_MICROSECS  11644473600000000ULL
#endif
 
struct timezone
{
  int  tz_minuteswest; /* minutes W of Greenwich */
  int  tz_dsttime;     /* type of dst correction */
};
 
// Definition of a gettimeofday function
 
int gettimeofday(struct timeval *tv, struct timezone *tz)
{
// Define a structure to receive the current Windows filetime
  FILETIME ft;
 
// Initialize the present time to 0 and the timezone to UTC
  unsigned __int64 tmpres = 0;
  static int tzflag = 0;
 
  if (NULL != tv)
  {
    GetSystemTimeAsFileTime(&ft);
 
// The GetSystemTimeAsFileTime returns the number of 100 nanosecond 
// intervals since Jan 1, 1601 in a structure. Copy the high bits to 
// the 64 bit tmpres, shift it left by 32 then or in the low 32 bits.
    tmpres |= ft.dwHighDateTime;
    tmpres <<= 32;
    tmpres |= ft.dwLowDateTime;
 
// Convert to microseconds by dividing by 10
    tmpres /= 10;
 
// The Unix epoch starts on Jan 1 1970.  Need to subtract the difference 
// in seconds from Jan 1 1601.
    tmpres -= DELTA_EPOCH_IN_MICROSECS;
 
// Finally change microseconds to seconds and place in the seconds value. 
// The modulus picks up the microseconds.
    tv->tv_sec = (long)(tmpres / 1000000UL);
    tv->tv_usec = (long)(tmpres % 1000000UL);
  }
 
  if (NULL != tz)
  {
    if (!tzflag)
    {
      _tzset();
      tzflag++;
    }
  
// Adjust for the timezone west of Greenwich
      tz->tz_minuteswest = _timezone / 60;
    tz->tz_dsttime = _daylight;
  }
 
  return 0;
}

