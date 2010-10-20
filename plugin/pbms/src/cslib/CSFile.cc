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

#include "CSConfig.h"

#ifndef XT_WIN
#include <unistd.h>
#include <dirent.h>
#endif
#include <stdio.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>

#include "CSFile.h"
#include "CSStream.h"
#include "CSGlobal.h"


static int unix_file_open(const char *path, int flags, mode_t mode)
{
	return open(path, flags, mode);
}

int unix_file_close(int fh)
{
	return close(fh);
}

/*
 * ---------------------------------------------------------------
 * CORE SYSTEM FILES
 */

CSFile::~CSFile()
{
	close();
	if (myFilePath)
		myFilePath->release();
}

CSOutputStream *CSFile::getOutputStream()
{
	return CSFileOutputStream::newStream(RETAIN(this));
}

CSOutputStream *CSFile::getOutputStream(off64_t offset)
{
	return CSFileOutputStream::newStream(RETAIN(this), offset);
}

CSInputStream *CSFile::getInputStream()
{
	return CSFileInputStream::newStream(RETAIN(this));
}

CSInputStream *CSFile::getInputStream(off64_t offset)
{
	return CSFileInputStream::newStream(RETAIN(this), offset);
}

void CSFile::open(int mode)
{
	CSPath	*dir;
	bool	retry = false;

	CLOBBER_PROTECT(retry);

	enter_();
	if (mode & CREATE) {
		/* Create and open the file: */
		do {
			try_(a) {
				openFile(mode);
				retry = false;
			}
			catch_(a) {
				if (retry || !isDirNotFound(&self->myException))
					throw_();

				/* Make sure the parent directory exists: */
				dir = CSPath::newPath(RETAIN(myFilePath), "..");
				push_(dir);
				try_(b) {
					dir->makePath();
				}
				catch_(b) { /* Two threads may try to create the directory at the same time. */
					if (!isDirExists(&self->myException))
						throw_();
				}
				cont_(b);

				release_(dir);
				retry = true;
			}
			cont_(a);
		}
		while (retry);
	}
	else
		openFile(mode);
	exit_();
}

bool CSFile::transfer(CSFile *dst_file, off64_t dst_offset, CSFile *src_file, off64_t src_offset, off64_t size, char *buffer, size_t buffer_size)
{
	size_t tfer;

	while (size > 0) {
		if (size > (off64_t) buffer_size)
			tfer = buffer_size;
		else
			tfer = (size_t) size;
		if (!(tfer = src_file->read(buffer, src_offset, tfer, 0)))
			return false;
		dst_file->write(buffer, dst_offset, tfer);
		dst_offset += tfer;
		src_offset += tfer;
		size -= tfer;
	}
	return true;
}

void CSFile::streamOut(CSOutputStream *dst_stream, off64_t src_offset, off64_t size, char *buffer, size_t buffer_size)
{
	size_t tfer;

	while (size > 0) {
		if (size > (off64_t) buffer_size)
			tfer = buffer_size;
		else
			tfer = (size_t) size;
      
		read(buffer, src_offset, tfer, tfer);
		dst_stream->write(buffer, tfer);
    
		src_offset += tfer;
		size -= tfer;
	}
}

#define CS_MASK				((S_IRUSR | S_IWUSR) | (S_IRGRP | S_IWGRP) | (S_IROTH))

void CSFile::close()
{
	if (iFH != -1) {
		unix_file_close(iFH);
		iFH = -1;
	}
}

off64_t CSFile::getEOF()
{
	off64_t eof;

	if ((eof = lseek(iFH, 0, SEEK_END)) == (off64_t) -1)
		CSException::throwFileError(CS_CONTEXT, myFilePath->getCString(), errno);

     return eof;
}

void CSFile::setEOF(off64_t offset)
{
	if (ftruncate(iFH, offset) == -1)
		CSException::throwFileError(CS_CONTEXT, myFilePath->getCString(), errno);
}

size_t CSFile::read(void *data, off64_t offset, size_t size, size_t min_size)
{
	ssize_t read_size;
	
	enter_();
	read_size = pread(iFH, data, size, offset);
	self->interrupted();
	if (read_size ==  -1)
		CSException::throwFileError(CS_CONTEXT, myFilePath->getCString(), errno);
	if ((size_t) read_size < min_size)
		CSException::throwEOFError(CS_CONTEXT, myFilePath->getCString());
	return_(read_size);
}

void CSFile::write(const void *data, off64_t offset, size_t size)
{
	size_t write_size;

	enter_();
    write_size = pwrite(iFH, (void *) data, size, offset);
	self->interrupted();
	if (write_size != size)
		CSException::throwFileError(CS_CONTEXT, myFilePath->getCString(), errno);
	exit_();
}

void CSFile::flush()
{
}

void CSFile::sync()
{
	fsync(iFH);
}

CSFile *CSFile::newFile(CSPath *path)
{
	CSFile *f;

	if (!(f = new CSFile())) {
		path->release();
		CSException::throwOSError(CS_CONTEXT, ENOMEM);
	}
	f->myFilePath = path;
	return f;
}

CSFile *CSFile::newFile(const char *path_str)
{
	CSPath *path;

	path = CSPath::newPath(path_str);
	return newFile(path);
}

void CSFile::openFile(int mode)
{
	int flags = 0;

	if (mode & READONLY)
		flags = O_RDONLY;
	else
		flags = O_RDWR;

	if (mode & CREATE)
		flags |= O_CREAT;
	
	if ((iFH = unix_file_open(myFilePath->getCString(), flags, CS_MASK)) ==  -1)
		CSException::throwFileError(CS_CONTEXT, myFilePath->getCString(), errno);

	/* Does not make sense to truncate, and have READONLY! */
	if ((mode & TRUNCATE) && !(mode & READONLY))
		setEOF((off64_t) 0);
}

/*
 * ---------------------------------------------------------------
 * A READ BUFFERED FILE
 */

CSReadBufferedFile::CSReadBufferedFile():
myFile(NULL),
iFileBufferOffset(0),
iBufferDataLen(0)
{
}

CSReadBufferedFile::~CSReadBufferedFile()
{
	if (myFile) {
		close();
		myFile->release();
		myFile = NULL;
	}
}

void CSReadBufferedFile::close()
{
	flush();
	myFile->close();
	iFileBufferOffset = 0;
	iBufferDataLen = 0;
}

off64_t CSReadBufferedFile::getEOF()
{
	off64_t eof = myFile->getEOF();

	if (eof < iFileBufferOffset + iBufferDataLen)
		return iFileBufferOffset + iBufferDataLen;
	return eof;
}

void CSReadBufferedFile::setEOF(off64_t offset)
{
	myFile->setEOF(offset);
	if (offset < iFileBufferOffset) {
		iFileBufferOffset = 0;
		iBufferDataLen = 0;
	}
	else if (offset < iFileBufferOffset + iBufferDataLen)
		iBufferDataLen = offset - iFileBufferOffset;
}

size_t CSReadBufferedFile::read(void *data, off64_t offset, size_t size, size_t min_size)
{
	size_t result;
	size_t tfer = 0;

	if (iBufferDataLen > 0) {
		if (offset < iFileBufferOffset) {
			//  case 1, 2, 6
			if (offset + size > iFileBufferOffset + iBufferDataLen) {
				// 6 (would have to do 2 reads and a memcpy (better is just one read)
				flush();
				goto readit;
			}
			if (offset + size > iFileBufferOffset) {
				// 2
				tfer = offset + size - iFileBufferOffset;
				memcpy((char *) data + (iFileBufferOffset - offset), iFileBuffer, tfer);
				size -= tfer;
			}
			// We assume we are reading back to front: 
			if (size < SC_DEFAULT_FILE_BUFFER_SIZE) {
				size_t mins;

				if (offset + size >= SC_DEFAULT_FILE_BUFFER_SIZE) {
					iFileBufferOffset = offset + size - SC_DEFAULT_FILE_BUFFER_SIZE;
					mins = SC_DEFAULT_FILE_BUFFER_SIZE;
				}
				else {
					iFileBufferOffset = 0;
					mins = (size_t) offset + size;
				}
				result = myFile->read(iFileBuffer, iFileBufferOffset, SC_DEFAULT_FILE_BUFFER_SIZE, mins);
				iBufferDataLen = result;
				memcpy(data, iFileBuffer + (offset - iFileBufferOffset), size);
			}
			else
				result = myFile->read(data, offset, size, size);
			return size + tfer;
		}
		if (offset + size <= iFileBufferOffset + iBufferDataLen) {
			// 3
			memcpy(data, iFileBuffer + (offset - iFileBufferOffset), size);
			return size;
		}
		if (offset < iFileBufferOffset + iBufferDataLen) {
			// 4 We assume we are reading front to back
			tfer = iFileBufferOffset + iBufferDataLen - offset;
			memcpy(data, iFileBuffer + (offset - iFileBufferOffset), tfer);
			data = (char *) data + tfer;
			size -= tfer;
			offset += tfer;
			if (min_size >= tfer)
				min_size -= tfer;
			else
				min_size = 0;
		}
		// else 5
	}

	readit:
	if (size < SC_DEFAULT_FILE_BUFFER_SIZE) {
		result = myFile->read(iFileBuffer, offset, SC_DEFAULT_FILE_BUFFER_SIZE, min_size);
		iFileBufferOffset = offset;
		iBufferDataLen = result;
		if (result > size)
			result = size;
		memcpy(data, iFileBuffer, result);
	}
	else
		result = myFile->read(data, offset, size, min_size);
	return result + tfer;
}

void CSReadBufferedFile::write(const void *data, off64_t offset, size_t size)
{
	if (iBufferDataLen > 0) {
		size_t tfer;

		if (offset < iFileBufferOffset) {
			//  case 1, 2, 6
			if (offset + size > iFileBufferOffset + iBufferDataLen) {
				// 6 (would have to do 2 reads and a memcpy (better is just one read)
				memcpy((char *) data + (iFileBufferOffset - offset), iFileBuffer, iBufferDataLen);
			}
			else if (offset + size > iFileBufferOffset) {
				// 2
				tfer = offset + size - iFileBufferOffset;
				memcpy(iFileBuffer, (char *) data + (iFileBufferOffset - offset), tfer);
			}
		}
		else if (offset + size <= iFileBufferOffset + iBufferDataLen) {
			// 3
			memcpy(iFileBuffer + (offset - iFileBufferOffset), data, size);
		}
		else if (offset < iFileBufferOffset + iBufferDataLen) {
			// 4 We assume we are reading front to back
			tfer = iFileBufferOffset + iBufferDataLen - offset;
			memcpy(iFileBuffer + (offset - iFileBufferOffset), data, tfer);
		}
		// else 5
	}

	myFile->write(data, offset, size);
}

void CSReadBufferedFile::flush()
{
	myFile->flush();
}

void CSReadBufferedFile::sync()
{
	flush();
	myFile->sync();
}

const char *CSReadBufferedFile::getEOL()
{
	return myFile->getEOL();
}

void CSReadBufferedFile::openFile(int mode)
{
	myFile->openFile(mode);
}

CSFile *CSReadBufferedFile::newFile(CSFile *file)
{
	CSReadBufferedFile *f;

	if (!(f = new CSReadBufferedFile())) {
		file->release();
		CSException::throwOSError(CS_CONTEXT, ENOMEM);
	}
	f->myFile = file;
	f->myFilePath = file->myFilePath;
	f->myFilePath->retain();
	return (CSFile *) f;
}

/*
 * ---------------------------------------------------------------
 * A BUFFERED FILE
 */

void CSBufferedFile::write(const void *data, off64_t offset, size_t size)
{
	if (iBufferDataLen > 0) {
		if (offset < iFileBufferOffset && offset <= iFileBufferOffset + iBufferDataLen) {
			size_t tfer;

			tfer = iFileBufferOffset + SC_DEFAULT_FILE_BUFFER_SIZE - offset;
			if (tfer >= size) {
				memcpy(iFileBuffer + (offset - iFileBufferOffset), data, size);
			}
			size -= tfer;
			data = (char *) data + tfer;
		}
		flush();
	}

	if (size < SC_DEFAULT_FILE_BUFFER_SIZE) {
		iFileBufferOffset = offset;
		iBufferDataLen = size;
		memcpy(iFileBuffer, data, size);
		iBufferDirty = true;
	}
	else
		myFile->write(data, offset, size);
}

void CSBufferedFile::flush()
{
	if (iBufferDirty && iBufferDataLen) {
		myFile->write(iFileBuffer, iFileBufferOffset, iBufferDataLen);
		iBufferDirty = false;
		
	}
	myFile->flush();
}

/*
 * ---------------------------------------------------------------
 * FILE BASED ON THE STANDARD C FILE
 */
