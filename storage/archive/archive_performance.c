/* Copyright (C) 2006 MySQL AB

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA */

#include "azio.h"
#include <string.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <my_getopt.h>
#include <mysql_version.h>

#define ARCHIVE_ROW_HEADER_SIZE 4

#define COMMENT_STRING "Your bases"
#define FRM_STRING "My bases"
#define TEST_FILENAME "performance_test.az"
#define TEST_STRING_INIT "YOU don't know about me without you have read a book by the name of The Adventures of Tom Sawyer; but that ain't no matter.  That book was made by Mr. Mark Twain, and he told the truth, mainly.  There was things which he stretched, but mainly he told the truth.  That is nothing.  I never seen anybody but lied one time or another, without it was Aunt Polly, or the widow, or maybe Mary.  Aunt Polly--Tom's Aunt Polly, she is--and Mary, and the Widow Douglas is all told about in that book, which is mostly a true book, with some stretchers, as I said before.  Now the way that the book winds up is this:  Tom and me found the money that the robbers hid in the cave, and it made us rich.  We got six thousand dollars apiece--all gold.  It was an awful sight of money when it was piled up.  Well, Judge Thatcher he took it and put it out at interest, and it fetched us a dollar a day apiece all the year round --more than a body could tell what to do with.  The Widow Douglas she took me for her son, and allowed she would..."
#define TEST_LOOP_NUM 100


#define ARCHIVE_ROW_HEADER_SIZE 4

#define BUFFER_LEN 1024

char test_string[BUFFER_LEN];

#define ROWS_TO_TEST LL(2000000)

/* prototypes */
long int timedif(struct timeval a, struct timeval b);
int generate_data(unsigned long long length);
int read_test(azio_stream *reader_handle, unsigned long long rows_to_test_for);

int main(int argc, char *argv[])
{
  unsigned int method;
  struct timeval start_time, end_time;
  long int timing;

  my_init();
  MY_INIT(argv[0]);

  if (argc != 1)
    return 1;

  printf("Performing write() test\n");
  generate_data(ROWS_TO_TEST);

  for (method= AZ_METHOD_BLOCK; method < AZ_METHOD_MAX; method++)
  {
    unsigned int ret;
    azio_stream reader_handle;

    if (method)
      printf("Performing aio_read() test\n");
    else
      printf("Performing read() test\n");

    if (!(ret= azopen(&reader_handle, TEST_FILENAME, O_RDONLY|O_BINARY,
                    method)))
    {
      printf("Could not open test file\n");
      return 0;
    }

    gettimeofday(&start_time, NULL);
    read_test(&reader_handle, 1044496L);
    gettimeofday(&end_time, NULL);
    timing= timedif(end_time, start_time);
    printf("Time took to read was %ld.%03ld seconds\n", timing / 1000, timing % 1000);

    azclose(&reader_handle);
  }

  my_end(0);

  return 0;
}

int generate_data(unsigned long long rows_to_test)
{
  azio_stream writer_handle;
  unsigned long long x;
  unsigned int ret;
  struct timeval start_time, end_time;
  long int timing;
  struct stat buf;

  if ((stat(TEST_FILENAME, &buf)) == 0)
  {
    printf("Writer file already available\n");
    return 0;
  }

  if (!(ret= azopen(&writer_handle, TEST_FILENAME, O_CREAT|O_RDWR|O_TRUNC|O_BINARY,
                    AZ_METHOD_BLOCK)))
  {
    printf("Could not create test file\n");
    exit(1);
  }

  gettimeofday(&start_time, NULL);
  for (x= 0; x < rows_to_test; x++)
  {
    ret= azwrite_row(&writer_handle, test_string, BUFFER_LEN);
    if (ret != BUFFER_LEN)
    {
      printf("Size %u\n", ret);
      assert(ret != BUFFER_LEN);
    }
    if ((x % 14031) == 0)
    {
      azflush(&writer_handle,  Z_SYNC_FLUSH);
    }
  }
  /* 
    We put the flush in just to be honest with write speed, normally azclose 
    would be fine.
  */
  azflush(&writer_handle,  Z_SYNC_FLUSH);
  gettimeofday(&end_time, NULL);
  timing= timedif(end_time, start_time);

  azclose(&writer_handle);

  printf("Time took to write was %ld.%03ld seconds\n", timing / 1000, timing % 1000);

  return 0;
}

int read_test(azio_stream *reader_handle, unsigned long long rows_to_test_for)
{
  unsigned long long read_length= 0;
  unsigned long long count= 0;
  unsigned int ret;
  int error;

  azread_init(reader_handle);
  while ((ret= azread_row(reader_handle, &error)))
  {
    if (error)
    {
      fprintf(stderr, "Got an error while reading at row %llu\n", count);
      exit(1);
    }

    read_length+= ret;
    assert(!memcmp(reader_handle->row_ptr, test_string, ret));
    if (ret != BUFFER_LEN)
    {
      printf("Size %u\n", ret);
      assert(ret != BUFFER_LEN);
    }
    count++;
  }
  assert(rows_to_test_for == rows_to_test_for);

  return 0;
}

long int timedif(struct timeval a, struct timeval b)
{
    register int us, s;
 
    us = a.tv_usec - b.tv_usec;
    us /= 1000;
    s = a.tv_sec - b.tv_sec;
    s *= 1000;
    return s + us;
}
