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
 * @brief drizzle_con_st tests
 */

#include <libdrizzle-1.0/t/common.h>

#include <assert.h>
#include <unistd.h>

#define DRIZZLE_TEST_PORT 12399

int main(void)
{
  close(STDOUT_FILENO);

  printf("sizeof(drizzle_con_st) = %zu\n", sizeof(drizzle_con_st));
  printf("sizeof(drizzle_con_tcp_st) = %zu\n", sizeof(drizzle_con_tcp_st));
  printf("sizeof(drizzle_con_uds_st) = %zu\n", sizeof(drizzle_con_uds_st));
  printf("sizeof(drizzle_con_options_t) = %zu\n",
         sizeof(drizzle_con_options_t));
  printf("sizeof(drizzle_con_socket_t) = %zu\n", sizeof(drizzle_con_socket_t));
  printf("sizeof(drizzle_con_status_t) = %zu\n", sizeof(drizzle_con_status_t));
  printf("sizeof(drizzle_capabilities_t) = %zu\n",
         sizeof(drizzle_capabilities_t));

  drizzle_st *drizzle;
  drizzle_test("drizzle_create");
  if ((drizzle= drizzle_create(NULL)) == NULL)
  {
    drizzle_test_error("returned NULL");
  }

  drizzle_con_st con_buffer;
  drizzle_con_st *con;
  drizzle_test("drizzle_con_create buffer");
  if ((con= drizzle_con_create(drizzle, &con_buffer)) == NULL)
  {
    drizzle_test_error("returned NULL");
  }
  drizzle_con_free(con);

  drizzle_test("drizzle_con_create");
  if ((con= drizzle_con_create(drizzle, NULL)) == NULL)
  {
    drizzle_test_error("returned NULL");
  }

  drizzle_con_st *clone;
  drizzle_test("drizzle_con_clone");
  if ((clone= drizzle_con_clone(drizzle, NULL, con)) == NULL)
  {
    drizzle_test_error("returned NULL");
  }
  drizzle_con_free(clone);

  drizzle_test("drizzle_con_options");
  if (drizzle_con_options(con) != (DRIZZLE_CON_ALLOCATED | DRIZZLE_CON_MYSQL))
  {
    drizzle_test_error("drizzle_con_options");
  }

  drizzle_test("drizzle_con_add_options");
  drizzle_con_add_options(con, DRIZZLE_CON_EXPERIMENTAL);
  if (drizzle_con_options(con) != (DRIZZLE_CON_ALLOCATED | DRIZZLE_CON_EXPERIMENTAL))
  {
    drizzle_test_error("drizzle_con_options");
  }

  drizzle_test("drizzle_con_set_tcp");
  drizzle_con_set_tcp(con, "localhost", 1);

  drizzle_test("drizzle_con_host");
  if (strcmp(drizzle_con_host(con), "localhost"))
  {
    drizzle_test_error("expected host not set");
  }

  drizzle_test("drizzle_con_port");
  if (drizzle_con_port(con) != 1)
  {
    drizzle_test_error("expected port not set");
  }

  drizzle_test("drizzle_con_fd");
  if (drizzle_con_fd(con) != -1) 
  {
    drizzle_test_error("drizzle_con_fd != -1 for a new connection");
  }

  drizzle_test("drizzle_con_connect");
  drizzle_return_t ret= drizzle_con_connect(con);
  if (ret != DRIZZLE_RETURN_COULD_NOT_CONNECT) 
  {
    drizzle_test_error("expected COULD_NOT_CONNECT, got: %s\n", drizzle_error(drizzle));
  }

  if (drizzle_con_fd(con) != -1)
  {
    drizzle_test_error("drizzle_con_fd != -1 for unconnected connection");
  }

  drizzle_test("drizzle_con_listen");
  drizzle_con_set_tcp(con, "localhost", DRIZZLE_TEST_PORT);
  ret = drizzle_con_listen(con);
  if (ret != DRIZZLE_RETURN_OK) 
  {
    drizzle_test_error("drizzle_con_listen: %s", drizzle_error(drizzle));
  }

  if (drizzle_con_fd(con) <= 0)
  {
    drizzle_test_error("drizzle_con_fd <= 0 for a listening connection");
  }

  drizzle_test("drizzle_con_free");
  drizzle_con_free(con);

  drizzle_test("drizzle_con_clone with uds");
  con= drizzle_con_create(drizzle, NULL);
  assert(con != NULL);
  drizzle_con_set_uds(con, "/dev/null");
  clone= drizzle_con_clone(drizzle, NULL, con);
  if (clone == NULL) 
  {
    drizzle_test_error("drizzle_con_clone uds: %s", drizzle_error(drizzle));
  }
  drizzle_con_free(clone);
  drizzle_con_free(con);
  
  drizzle_test("drizzle_free");
  drizzle_free(drizzle);

  return 0;
}
