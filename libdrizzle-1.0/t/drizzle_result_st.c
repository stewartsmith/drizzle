/*
 * Drizzle Client & Protocol Library
 *
 * Copyright (C) 2008 Eric Day (eday@oddments.org)
 * All rights reserved.
 *
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
 */

/**
 * @file
 * @brief drizzle_result_st tests
 */

#include <libdrizzle-1.0/t/common.h>

#include <unistd.h>

int main(void)
{
  close(STDOUT_FILENO);

  drizzle_st *drizzle;
  drizzle_con_st *con;
  drizzle_result_st *result;
  drizzle_result_st result_buffer;
  drizzle_result_st *clone;

  printf("sizeof(drizzle_result_st) = %zu\n", sizeof(drizzle_result_st));

  if ((drizzle= drizzle_create(NULL)) == NULL)
    drizzle_test_error("drizzle_create");

  if ((con= drizzle_con_create(drizzle, NULL)) == NULL)
    drizzle_test_error("drizzle_con_create");

  if ((result= drizzle_result_create(con, &result_buffer)) == NULL)
    drizzle_test_error("drizzle_result_create");
  drizzle_result_free(result);

  if ((result= drizzle_result_create(con, NULL)) == NULL)
    drizzle_test_error("drizzle_result_create");

  if ((clone= drizzle_result_clone(con, NULL, result)) == NULL)
    drizzle_test_error("drizzle_result_clone");
  drizzle_result_free(clone);

  drizzle_result_set_info(result, "simple test");

  if (strcmp(drizzle_result_info(result), "simple test"))
    drizzle_test_error("drizzle_result_info");

  drizzle_result_free(result);
  drizzle_con_free(con);
  drizzle_free(drizzle);

  return 0;
}
