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

#ifndef PLUGIN_LOGGING_STATS_STATS_SCHEMA_H
#define PLUGIN_LOGGING_STATS_STATS_SCHEMA_H

#include <drizzled/plugin/table_function.h>
#include <drizzled/field.h>

#include "logging_stats.h"

#include <vector>

class CurrentCommandsTool : public drizzled::plugin::TableFunction
{
private:
  LoggingStats *logging_stats;

public:

  CurrentCommandsTool(LoggingStats *logging_stats);

  class Generator : public drizzled::plugin::TableFunction::Generator
  {
  public:
    Generator(drizzled::Field **arg, LoggingStats *logging_stats);

    ~Generator();

    bool populate();
  private:
    void setVectorIteratorsAndLock(uint32_t bucket_number);
   
    Scoreboard *current_scoreboard; 
    uint32_t current_bucket;
    uint32_t number_buckets;
    std::vector<ScoreboardSlot *>::iterator it;
    std::vector<ScoreboardSlot *>::iterator end;
    pthread_rwlock_t* current_lock;
  };

  Generator *generator(drizzled::Field **arg)
  {
    return new Generator(arg, logging_stats);
  }
};

class CumulativeCommandsTool : public drizzled::plugin::TableFunction
{
private:
  LoggingStats *outer_logging_stats;

public:

  CumulativeCommandsTool(LoggingStats *logging_stats);

  class Generator : public drizzled::plugin::TableFunction::Generator
  {
  public:
    Generator(drizzled::Field **arg, LoggingStats *logging_stats);

    bool populate();
  private:
    LoggingStats *logging_stats;
    uint32_t record_number;
    uint32_t total_records;
  };

  Generator *generator(drizzled::Field **arg)
  {
    return new Generator(arg, outer_logging_stats);
  }
};

#endif /* PLUGIN_LOGGING_STATS_STATS_SCHEMA_H */
