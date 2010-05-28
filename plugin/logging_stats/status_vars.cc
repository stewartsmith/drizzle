/*
 * Copyright (c) 2010, Joseph Daly <skinny.moey@gmail.com>
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

#include "config.h"
#include <drizzled/plugin.h>
#include <drizzled/statistics_variables.h>
#include <drizzled/session.h>

#include "status_vars.h"

using namespace drizzled;

drizzle_show_var StatusVars::status_vars_defs[]= 
{
  {"Bytes_received",           (char*) offsetof(system_status_var, bytes_received), SHOW_LONGLONG_STATUS},
  {"Bytes_sent",               (char*) offsetof(system_status_var, bytes_sent), SHOW_LONGLONG_STATUS},
  {NULL, NULL, SHOW_LONGLONG}
};

StatusVars::StatusVars()
{
  status_var_counters= (system_status_var*) malloc(sizeof(system_status_var));
  memset(status_var_counters, 0, sizeof(system_status_var));
}

StatusVars::StatusVars(const StatusVars &status_vars)
{
  status_var_counters= (system_status_var*) malloc(sizeof(system_status_var));
  memset(status_var_counters, 0, sizeof(system_status_var));
  copySystemStatusVar(status_var_counters, status_vars.status_var_counters); 
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
}

void StatusVars::reset()
{
  memset(status_var_counters, 0, sizeof(system_status_var));
}

void StatusVars::logStatusVar(Session *session)
{
  copySystemStatusVar(status_var_counters, &session->status_var);
}

//TMP
void StatusVars::test()
{
  drizzle_show_var show_var= status_vars_defs[0];
  show_var.value= (char*) "test";
}

