/* 
 * Copyright (C) 2010 Djellel Eddine Difallah
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
 *   * Neither the name of Djellel Eddine Difallah nor the names of its contributors
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

#include <drizzled/replication_services.h>

#include <drizzled/plugin/transaction_replicator.h>
#include <drizzled/plugin/transaction_applier.h>
#include <drizzled/message/transaction.pb.h>

#include <string>

class Invalidator : public drizzled::plugin::TransactionApplier 
{
public:

  Invalidator(std::string name_arg);

  /** Destructor */
  ~Invalidator() {}

  /**
   * Does something with the transaction message
   *
   * @param transaction message to be invalidated if present in the cache
   */
  drizzled::plugin::ReplicationReturnCode
  apply(drizzled::Session &in_session,
        const drizzled::message::Transaction &to_apply);
  

private:

  /**
   * Given a supplied Statement message, parse out the table
   * and schema name from the various metadata and header 
   * pieces for the different Statement types.
   *
   * @param[in] Statement to parse
   * @param[out] Schema name
   * @param[out] Table name
   */
  void parseStatementTableMetadata(const drizzled::message::Statement &in_statement,
                                   std::string &in_schema_name,
                                   std::string &in_table_name) const;

  /**
   * Given a schema name and table name, delete all the entries
   * of the table from the cache.
   *
   * @param[in] stmt The statement message
   * @param[in] Schema name
   * @param[in] Table name
   */
  void invalidateByTableName(const std::string &in_schema_name,
                             const std::string &in_table_name) const;


};

