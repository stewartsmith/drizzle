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

#pragma once

#include "logging_stats.h"

#include <drizzled/plugin/table_function.h>
#include <drizzled/field.h>

class StatusTool : public drizzled::plugin::TableFunction
{            
public:

  StatusTool(LoggingStats *logging_stats, bool isLocal);
    
  class Generator : public drizzled::plugin::TableFunction::Generator
  { 
  public:
    Generator(drizzled::Field **arg, LoggingStats *logging_stats,
              std::vector<drizzled::drizzle_show_var *> *all_status_vars, 
              bool isLocal);

    ~Generator();

    bool populate();
  private:
    LoggingStats *logging_stats;
    bool isLocal;
    StatusVars *status_var_to_display;
    std::vector<drizzled::drizzle_show_var *>::iterator all_status_vars_it;
    std::vector<drizzled::drizzle_show_var *>::iterator all_status_vars_end;
    void fill(const std::string &name, char *value, drizzled::SHOW_TYPE show_type); 
  };

  Generator *generator(drizzled::Field **arg)
  {
    return new Generator(arg, outer_logging_stats, &all_status_vars, isLocal);
  }

private:
  LoggingStats *outer_logging_stats;

  bool isLocal;

  std::vector<drizzled::drizzle_show_var *> all_status_vars;
};

