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
#include <cstring>
#include <cstdlib>

#include <libdrizzle-2.0/drizzle_client.h>

#define SIMPLE_MULTI_COUNT 10

int main(int argc, char *argv[])
{
  const char *query= "SELECT table_schema,table_name FROM tables";
  drizzle_st *drizzle;

  std::vector<drizzle_con_st *> cons;
  std::vector<drizzle_result_st *> result;
  std::vector<drizzle_query_st *> ql;

  if ((drizzle= drizzle_create()) == NULL)
  {
    fprintf(stderr, "drizzle_create:NULL\n");
    return 1;
  }

  /* Create SIMPLE_MULTI_COUNT connections and initialize query list. */
  for (int x= 0; x < SIMPLE_MULTI_COUNT; x++)
  {
    drizzle_con_st *con;
    if (x == 0)
    {
      if ((con= drizzle_con_create(drizzle)) == NULL)
      {
        printf("drizzle_con_create:%s\n", drizzle_error(drizzle));
        return 1;
      }

      if (argc == 2 && !strcmp(argv[1], "-m"))
      {
        drizzle_con_add_options(con, DRIZZLE_CON_MYSQL);
      }
      else if (argc != 1)
      {
        printf("usage: %s [-m]\n", argv[0]);
        return 1;
      }

      drizzle_con_set_db(con, "information_schema");
      cons.push_back(con);
    }
    else
    {
      if ((con= drizzle_con_clone(drizzle, cons[0])) == NULL)
      {
        fprintf(stderr, "drizzle_con_clone:%s\n", drizzle_error(drizzle));
        return 1;
      }
    }

    cons.push_back(con);
    drizzle_result_st *res= drizzle_result_create(con);
    result.push_back(res);

    drizzle_query_st *exec_query;
    if ((exec_query= drizzle_query_add(drizzle, NULL, con, res, query, strlen(query), DRIZZLE_QUERY_NONE, NULL)) == NULL)
    {
      fprintf(stderr, "drizzle_query_add:%s\n", drizzle_error(drizzle));
      return 1;
    }
    ql.push_back(exec_query);
  }

  drizzle_return_t ret= drizzle_query_run_all(drizzle);
  if (ret != DRIZZLE_RETURN_OK)
  {
    printf("drizzle_query_run_all:%s\n", drizzle_error(drizzle));
    return 1;
  }

  uint32_t x= 0;
  for (std::vector<drizzle_result_st *>::iterator iter= result.begin(); iter != result.end(); iter++)
  {
    if (drizzle_result_error_code(*iter) != 0)
    {
      printf("%d:%s\n", drizzle_result_error_code(*iter),
             drizzle_result_error(*iter));
      continue;
    }

    drizzle_row_t row;
    while ((row= drizzle_row_next(*iter)) != NULL)
    {
      printf("%x %s:%s\n", x, row[0], row[1]);
    }
    x++;
  }

  drizzle_free(drizzle);

  return 0;
}
