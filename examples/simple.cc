/*
 * Drizzle Client & Protocol Library
 *
 * Copyright (C) 2008 Eric Day (eday@oddments.org)
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *
 *     * The names of its contributors may not be used to endorse or
 * promote products derived from this software without specific prior
 * written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <getopt.h>
#include <libdrizzle/drizzle_client.h>
#include <netdb.h>
#include <unistd.h>

int main(int argc, char *argv[])
{
  const char* db= "information_schema";
  const char* host= NULL;
  bool mysql= false;
  in_port_t port= 0;
  const char* query= "select table_schema, table_name from tables";
  drizzle_verbose_t verbose= DRIZZLE_VERBOSE_NEVER;

  for (int c; (c = getopt(argc, argv, "d:h:mp:q:v")) != -1; )
  {
    switch (c)
    {
    case 'd':
      db= optarg;
      break;

    case 'h':
      host= optarg;
      break;

    case 'm':
      mysql= true;
      break;

    case 'p':
      port= (in_port_t)atoi(optarg);
      break;

    case 'q':
      query= optarg;
      break;

    case 'v':
      switch (verbose)
      {
      case DRIZZLE_VERBOSE_NEVER:
        verbose= DRIZZLE_VERBOSE_FATAL;
        break;
      case DRIZZLE_VERBOSE_FATAL:
        verbose= DRIZZLE_VERBOSE_ERROR;
        break;
      case DRIZZLE_VERBOSE_ERROR:
        verbose= DRIZZLE_VERBOSE_INFO;
        break;
      case DRIZZLE_VERBOSE_INFO:
        verbose= DRIZZLE_VERBOSE_DEBUG;
        break;
      case DRIZZLE_VERBOSE_DEBUG:
        verbose= DRIZZLE_VERBOSE_CRAZY;
        break;
      case DRIZZLE_VERBOSE_CRAZY:
      case DRIZZLE_VERBOSE_MAX:
        break;
      }
      break;

    default:
      printf("usage: %s [-d <db>] [-h <host>] [-m] [-p <port>] [-q <query>] "
             "[-v]\n", argv[0]);
      printf("\t-d <db>    - Database to use for query\n");
      printf("\t-h <host>  - Host to listen on\n");
      printf("\t-m         - Use the MySQL protocol\n");
      printf("\t-p <port>  - Port to listen on\n");
      printf("\t-q <query> - Query to run\n");
      printf("\t-v         - Increase verbosity level\n");
      return 1;
    }
  }

  drizzle_st drizzle;
  if (drizzle_create(&drizzle) == NULL)
  {
    printf("drizzle_create:NULL\n");
    return 1;
  }

  drizzle_set_verbose(&drizzle, verbose);

  drizzle_con_st* con= new drizzle_con_st;
  if (drizzle_con_create(&drizzle, con) == NULL)
  {
    printf("drizzle_con_create:NULL\n");
    return 1;
  }

  if (mysql)
    drizzle_con_add_options(con, DRIZZLE_CON_MYSQL);

  drizzle_con_set_tcp(con, host, port);
  drizzle_con_set_db(con, db);

  drizzle_result_st result;
  drizzle_return_t ret;
  (void)drizzle_query_str(con, &result, query, &ret);
  if (ret != DRIZZLE_RETURN_OK)
  {
    printf("drizzle_query:%s\n", drizzle_con_error(con));
    return 1;
  }

  ret= drizzle_result_buffer(&result);
  if (ret != DRIZZLE_RETURN_OK)
  {
    printf("drizzle_result_buffer:%s\n", drizzle_con_error(con));
    return 1;
  }

  while (drizzle_row_t row= drizzle_row_next(&result))
  {
    for (int x= 0; x < drizzle_result_column_count(&result); x++)
      printf("%s%s", x == 0 ? "" : ":", row[x] == NULL ? "NULL" : row[x]);
    printf("\n");
  }

  drizzle_result_free(&result);
  drizzle_con_free(con);
  drizzle_free(&drizzle);
  delete con;
  return 0;
}
