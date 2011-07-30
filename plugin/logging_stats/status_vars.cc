/*
 * Copyright (C) 2010 Joseph Daly <skinny.moey@gmail.com>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 *   * Redistributions of source code must retain the above copyright notice,
 *     this list of conditions and the following disclaimer.
 *   * Redistributions in binary form must reproduce the above copyright notice,
 *     this list of conditions and the following disclaimer in the documentation
 *     and/or other materials provided with the distribution.
 *   * Neither the name of Joseph Daly nor the names of its contributors
 *     may be used to endorse or promote products derived from this software
 *     without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <config.h>
#include <drizzled/plugin.h>
#include <drizzled/statistics_variables.h>
#include <drizzled/session.h>
#include <drizzled/drizzled.h>
#include "status_vars.h"

using namespace drizzled;

StatusVars::StatusVars()
{
  status_var_counters= (system_status_var*) malloc(sizeof(system_status_var));
  memset(status_var_counters, 0, sizeof(system_status_var));
  sent_row_count= 0;
}

StatusVars::StatusVars(const StatusVars &status_vars)
{
  status_var_counters= (system_status_var*) malloc(sizeof(system_status_var));
  memset(status_var_counters, 0, sizeof(system_status_var));
  copySystemStatusVar(status_var_counters, status_vars.status_var_counters); 
  sent_row_count= 0;
}

StatusVars::~StatusVars()
{
  free(status_var_counters); 
}

void StatusVars::copySystemStatusVar(system_status_var *to_var, 
                                     system_status_var *from_var)
{
  uint64_t *end= (uint64_t*) ((unsigned char*) to_var + offsetof(system_status_var,
                 last_system_status_var) + sizeof(uint64_t));

  uint64_t *to= (uint64_t*) to_var;
  uint64_t *from= (uint64_t*) from_var;

  while (to != end)
  {
    *(to++)= *(from++);
  }
  to_var->last_query_cost= from_var->last_query_cost;
}

void StatusVars::merge(StatusVars *status_vars)
{
  system_status_var* from_var= status_vars->getStatusVarCounters(); 

  uint64_t *end= (uint64_t*) ((unsigned char*) status_var_counters + offsetof(system_status_var,
                 last_system_status_var) + sizeof(uint64_t));

  uint64_t *to= (uint64_t*) status_var_counters;
  uint64_t *from= (uint64_t*) from_var;

  while (to != end)
  {
    *(to++)+= *(from++);
  }

  sent_row_count+= status_vars->sent_row_count;
}

void StatusVars::reset()
{
  memset(status_var_counters, 0, sizeof(system_status_var));
  sent_row_count= 0;
}

void StatusVars::logStatusVar(Session *session)
{
  copySystemStatusVar(status_var_counters, &session->status_var);
  sent_row_count+= session->sent_row_count;
}

bool StatusVars::hasBeenFlushed(Session *session)
{
  system_status_var *current_status_var= &session->status_var;

  /* check bytes received if its lower then a flush has occurred */
  uint64_t current_bytes_received= current_status_var->bytes_received;
  uint64_t my_bytes_received= status_var_counters->bytes_received;
  return current_bytes_received < my_bytes_received;
}

void StatusVars::copyGlobalVariables(StatusVars *global_status_vars)
{
  system_status_var* from_var= global_status_vars->getStatusVarCounters();
  status_var_counters->aborted_connects= from_var->aborted_connects;  
  status_var_counters->aborted_threads= from_var->aborted_threads;
  status_var_counters->connection_time= from_var->connection_time;
}
