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
 * 2007-06-07
 *
 * CORE SYSTEM:
 * Basic file I/O.
 *
 */

#pragma once
#ifndef __CSFILE_H__
#define __CSFILE_H__

#include <stdio.h>

#include "CSDefs.h"
#include "CSPath.h"
#include "CSException.h"
#include "CSSys.h"

class CSOutputStream;
class CSInputStream;

class CSFile : public CSSysFile, public CSRefObject {
public:
	CSPath *myFilePath;

	static const int DEFAULT = 0;	// Open for read/write, error if it does not exist.
	static const int READONLY = 1;	// Open for readonly
	static const int CREATE = 2;	// Create if it does not exist
	static const int TRUNCATE = 4;	// After open, set EOF to zero	

	CSFile(): myFilePath(NULL), iMode(-1), iLocked(0) { }

	virtual ~CSFile(); 

	CSOutputStream *getOutputStream();
	CSOutputStream *getOutputStream(off64_t offset);

	CSInputStream *getInputStream();
	CSInputStream *getInputStream(off64_t offset);

	/*
	 * Open the file in the specified
	 * mode.
	 */
	virtual void open(int mode);

	/* Lock the file. The file will be unlocked
	 * when closed.
	 */
	virtual void lock();

	virtual void unlock();

	/*
	 * Close the file.
	 */
	virtual void close();

	/*
	 * Calculate the Md5 digest for the file.
	 */
	void md5Digest(Md5Digest *digest);

	/*
	 * Move the current position to
	 * the end of the file.
	 */
	virtual off64_t getEOF();

	virtual void setEOF(off64_t offset);

	/*
	 * Read a given number of bytes. This function
	 * throws an error if the number of bytes read
	 * ius less than 'min_size'.
	 */
	virtual size_t read(void *data, off64_t offset, size_t size, size_t min_size);

	/*
	 * Write the given number of bytes.
	 * Throws IOException if an error occurs.
	 */
	virtual void write(const void *data, off64_t offset, size_t size);

	/*
	 * Flush the data written.
	 */
	virtual void flush();

	/* Flush the OS buffers: */
	virtual void sync() ;

	/* Resets access and modification times of the file: */
	virtual void touch();

	/*
	 * Return a platform specific prefered 
	 * line ending for text files.
	 */
	virtual const char *getEOL() { return "\n"; };

	virtual const char *getPathString() { return myFilePath->getCString(); }

	bool exists() { return myFilePath->exists(); }

	friend class CSReadBufferedFile;

private:
	int		iMode;
	int		iLocked;

	virtual void openFile(int mode);
	bool try_CreateAndOpen(CSThread *self, int mode, bool retry);

public:
	void streamOut(CSOutputStream *dst_stream, off64_t src_offset, off64_t size, char *buffer, size_t buffer_size);
	void streamIn(CSInputStream *src_stream, off64_t dst_offset, off64_t size, char *buffer, size_t buffer_size);

	static bool isDirNotFound(CSException *e) { return e->getErrorCode() == ENOENT; }
	static bool isDirExists(CSException *e) { return e->getErrorCode() == EEXIST; }

	static bool transfer(CSFile *dst_file, off64_t dst_offset, CSFile *src_file, off64_t src_offset, off64_t size, char *buffer, size_t buffer_size);

	static CSFile *newFile(CSPath *path);

	static CSFile *newFile(const char *path);

	static CSFile *newFile(const char *dir_str, const char *path_str);
};


// This stuff needs to be retought.

#ifdef DEBUG
#define SC_DEFAULT_FILE_BUFFER_SIZE			127
#else
#define SC_DEFAULT_FILE_BUFFER_SIZE			(64 * 1024)
#endif

class CSReadBufferedFile : public CSRefObject {
public:

	CSReadBufferedFile();

	~CSReadBufferedFile();
	
	void setFile(CSFile	*file) {myFile = file;}

	const char *getPathString() { return myFile->getPathString(); }
	void open(int mode) {myFile->open(mode); }

	void close();

	off64_t getEOF();

	void setEOF(off64_t offset);

	size_t read(void *data, off64_t offset, size_t size, size_t min_size);

	void write(const void *data, off64_t offset, size_t size);

	void flush();

	void sync();

	const char *getEOL();

private:
	CSFile	*myFile;

	char	iFileBuffer[SC_DEFAULT_FILE_BUFFER_SIZE];
	off64_t	iFileBufferOffset;
	size_t	iBufferDataLen;

	virtual void openFile(int mode);
};


#endif
