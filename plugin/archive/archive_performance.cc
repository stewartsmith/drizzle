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

#include "config.h"

#include "azio.h"
#include <string.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <memory>

#if TIME_WITH_SYS_TIME
# include <sys/time.h>
# include <time.h>
#else
# if HAVE_SYS_TIME_H
#  include <sys/time.h>
# else
#  include <time.h>
# endif
#endif

#include <boost/scoped_ptr.hpp>

#define TEST_FILENAME "performance_test.az"

#define BUFFER_LEN 1024

char test_string[BUFFER_LEN];

#define ROWS_TO_TEST 2000000LL

/* prototypes */
static long int timedif(struct timeval a, struct timeval b);
static int generate_data(uint64_t length);
static int read_test(azio_stream *reader_handle, uint64_t rows_to_test_for);

int main(int argc, char *argv[])
{
  unsigned int method;
  struct timeval start_time, end_time;
  long int timing;

  drizzled::internal::my_init();
  MY_INIT(argv[0]);

  if (argc != 1)
    return 1;

  printf("Performing write() test\n");
  generate_data(ROWS_TO_TEST);

  for (method= AZ_METHOD_BLOCK; method < AZ_METHOD_MAX; method++)
  {
    unsigned int ret;
    boost::scoped_ptr<azio_stream> reader_handle_ap(new azio_stream);
    azio_stream &reader_handle= *reader_handle_ap.get();

    if (method)
      printf("Performing azio_read() test\n");
    else
      printf("Performing read() test\n");

    if (!(ret= azopen(&reader_handle, TEST_FILENAME, O_RDONLY,
                      (az_method)method)))
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

  drizzled::internal::my_end();

  return 0;
}

static int generate_data(uint64_t rows_to_test)
{
  boost::scoped_ptr<azio_stream> writer_handle_ap(new azio_stream);
  azio_stream &writer_handle= *writer_handle_ap.get();
  uint64_t x;
  unsigned int ret;
  struct timeval start_time, end_time;
  long int timing;
  struct stat buf;

  if ((stat(TEST_FILENAME, &buf)) == 0)
  {
    printf("Writer file already available\n");
    return 0;
  }

  if (!(ret= azopen(&writer_handle, TEST_FILENAME, O_CREAT|O_RDWR|O_TRUNC,
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

static int read_test(azio_stream *reader_handle, uint64_t rows_to_test_for)
{
  uint64_t read_length= 0;
  uint64_t count= 0;
  unsigned int ret;
  int error;

  azread_init(reader_handle);
  while ((ret= azread_row(reader_handle, &error)))
  {
    if (error)
    {
      fprintf(stderr, "Got an error while reading at row %"PRIu64"\n", count);
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

static long int timedif(struct timeval a, struct timeval b)
{
    register int us, s;

    us = a.tv_usec - b.tv_usec;
    us /= 1000;
    s = a.tv_sec - b.tv_sec;
    s *= 1000;
    return s + us;
}
