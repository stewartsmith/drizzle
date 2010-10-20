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

#ifndef __CSFILE_H__
#define __CSFILE_H__

#include <stdio.h>

#include "CSDefs.h"
#include "CSPath.h"
#include "CSException.h"

using namespace std;

extern int unix_file_close(int fh);

class CSOutputStream;
class CSInputStream;

class CSFile : public CSRefObject {
public:
	CSPath *myFilePath;

	static const int DEFAULT = 0;	// Open for read/write, error if it does not exist.
	static const int READONLY = 1;	// Open for readonly
	static const int CREATE = 2;	// Create if it does not exist
	static const int TRUNCATE = 4;	// After open, set EOF to zero	

	CSFile(): myFilePath(NULL), iFH(-1) { }

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

	/*
	 * Close the file.
	 */
	virtual void close();

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

	/*
	 * Return a platform specific prefered 
	 * line ending for text files.
	 */
	virtual const char *getEOL() { return "\n"; };

	virtual const char *getPathString() { return myFilePath->getCString(); }

	friend class CSReadBufferedFile;
	friend class CSBufferedFile;

private:
	int		iFH;

	virtual void openFile(int mode);

public:
	void streamOut(CSOutputStream *dst_stream, off64_t src_offset, off64_t size, char *buffer, size_t buffer_size);

	static bool isDirNotFound(CSException *e) { return e->getErrorCode() == ENOENT; }
	static bool isDirExists(CSException *e) { return e->getErrorCode() == EEXIST; }

	static bool transfer(CSFile *dst_file, off64_t dst_offset, CSFile *src_file, off64_t src_offset, off64_t size, char *buffer, size_t buffer_size);

	static CSFile *newFile(CSPath *path);

	static CSFile *newFile(const char *path);
};

#ifdef DEBUG
#define SC_DEFAULT_FILE_BUFFER_SIZE			127
#else
#define SC_DEFAULT_FILE_BUFFER_SIZE			(64 * 1024)
#endif

class CSReadBufferedFile : public CSFile {
public:
	CSFile	*myFile;

	CSReadBufferedFile();

	virtual ~CSReadBufferedFile();

	virtual void close();

	virtual off64_t getEOF();

	virtual void setEOF(off64_t offset);

	virtual size_t read(void *data, off64_t offset, size_t size, size_t min_size);

	virtual void write(const void *data, off64_t offset, size_t size);

	virtual void flush();

	virtual void sync();

	virtual const char *getEOL();

	friend class CSBufferedFile;

private:
	char	iFileBuffer[SC_DEFAULT_FILE_BUFFER_SIZE];
	off64_t	iFileBufferOffset;
	size_t	iBufferDataLen;

	virtual void openFile(int mode);
public:
	static CSFile *newFile(CSFile *file);
};

class CSBufferedFile : public CSReadBufferedFile {
public:
	CSBufferedFile(): CSReadBufferedFile(), iBufferDirty(false) { }

	virtual ~CSBufferedFile() { };

	virtual void write(const void *data, off64_t offset, size_t size);

	virtual void flush();

private:
	bool	iBufferDirty;
};

#endif
