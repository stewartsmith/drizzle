/* Copyright (C) 2000 MySQL AB

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA */

/* readline for batch mode */

#include <drizzled/global.h>
#include <mysys/my_sys.h>
#include <mystrings/m_string.h>
#include "client/linebuffer.h"

#include <stdexcept>

using namespace std;

LineBuffer::LineBuffer(uint32_t my_max_size,FILE *my_file)
{
  if (file)
    file= fileno(my_file);
  else
    file= NULL;
  bufread= IO_SIZE;
  max_size= my_max_size;
  buffer= (char *) malloc(bufread+1);
  if (buffer == NULL)
    throw runtime_error("malloc failed");
  start_of_line= end_of_line= end= buffer;
  buffer[0]= 0;

  eof= read_length= 0;
}

LineBuffer::~LineBuffer()
{
  free(buffer);
}

void LineBuffer::add_string(const char *str)
{
  uint32_t old_length= (uint32_t) (end - buffer);
  uint32_t length= (uint32_t) strlen(str);
  char *tmpptr= (char *) realloc(buffer,old_length+length+2);
  if (tmpptr == NULL)
    throw runtime_error("malloc failed");
  
  buffer= start_of_line= end_of_line= tmpptr;
  end= buffer + old_length;
  if (old_length)
    end[-1]= ' ';
  memcpy(end, str, length);
  end[length]= '\n';
  end[length+1]= 0;
  end+= length+1;
  eof= 1;
  max_size= 1;
}

char *LineBuffer::readline()
{
  char *pos;
  uint32_t out_length;

  if (!(pos=internal_readline(&out_length)))
    return 0;
  if (out_length && pos[out_length-1] == '\n')
    if (--out_length && pos[out_length-1] == '\r')  /* Remove '\n' */
      out_length--;                                 /* Remove '\r' */
  read_length=out_length;
  pos[out_length]=0;
  return pos;
}

char *LineBuffer::internal_readline(uint32_t *out_length)
{
  char *pos;
  size_t length;

  start_of_line=end_of_line;
  for (;;)
  {
    pos=end_of_line;
    while (*pos != '\n' && *pos)
      pos++;
    if (pos == end)
    {
      if ((uint32_t) (pos - start_of_line) < max_size)
      {
	if (!(length=fill_buffer()) || length == (size_t) -1)
	  return(0);
	continue;
      }
      pos--; /* break line here */
    }
    end_of_line= pos+1;
    *out_length= (uint32_t) (pos + 1 - eof - start_of_line);
    return(start_of_line);
  }
}

size_t LineBuffer::fill_buffer()
{
  size_t read_count;
  uint32_t bufbytes= (uint32_t) (end - start_of_line);

  if (eof)
    return 0;					/* Everything read */

  /* See if we need to grow the buffer. */

  for (;;)
  {
    uint32_t start_offset=(uint32_t) (start_of_line - buffer);
    read_count=(bufread - bufbytes)/IO_SIZE;
    if ((read_count*=IO_SIZE))
      break;
    bufread *= 2;
    if (!(buffer = (char*) realloc(buffer, bufread+1)))
      return (uint32_t) -1;
    start_of_line= buffer+start_offset;
    end= buffer+bufbytes;
  }

  /* Shift stuff down. */
  if (start_of_line != buffer)
  {
    memmove(buffer, start_of_line, (uint32_t) bufbytes);
    end= buffer+bufbytes;
  }

  /* Read in new stuff. */
  if ((read_count= my_read(file, (unsigned char*) end, read_count,
			   MYF(MY_WME))) == MY_FILE_ERROR)
    return (size_t) -1;

  /* Kludge to pretend every nonempty file ends with a newline. */
  if (!read_count && bufbytes && end[-1] != '\n')
  {
    eof= read_count = 1;
    *end= '\n';
  }
  end_of_line= (start_of_line=buffer)+bufbytes;
  end+= read_count;
  *end= 0;				/* Sentinel */
  return read_count;
}

