/* - mode: c; c-basic-offset: 2; indent-tabs-mode: nil; -*-
 *  vim:expandtab:shiftwidth=2:tabstop=2:smarttab:
 *
 *  Copyright (C) 2011 David Shrewsbury
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 */

#include <config.h>
#include <plugin/slave/replication_schema.h>
#include <drizzled/execute.h>
#include <drizzled/sql/result_set.h>
#include <string>
#include <vector>
#include <boost/lexical_cast.hpp>

using namespace std;
using namespace drizzled;
using namespace boost;

namespace slave
{

bool ReplicationSchema::create()
{
  vector<string> sql;

  sql.push_back("COMMIT");
  sql.push_back("CREATE SCHEMA IF NOT EXISTS `sys_replication` REPLICATE=FALSE");

  if (not executeSQL(sql))
    return false;

  /*
   * Create our IO thread state information table if we need to.
   */

  sql.clear();
  sql.push_back("COMMIT");
  sql.push_back("CREATE TABLE IF NOT EXISTS `sys_replication`.`io_state` ("
                " `status` VARCHAR(20) NOT NULL,"
                " `error_msg` VARCHAR(250))"
                " COMMENT = 'VERSION 1.0'");

  if (not executeSQL(sql))
    return false;

  sql.clear();
  sql.push_back("SELECT COUNT(*) FROM `sys_replication`.`io_state`");

  {
    sql::ResultSet result_set(1);
    Execute execute(*(_session.get()), true);
    execute.run(sql[0], result_set);
    result_set.next();
    string count= result_set.getString(0);

    /* Must always be at least one row in the table */
    if (count == "0")
    {
      sql.clear();
      sql.push_back("INSERT INTO `sys_replication`.`io_state` (`status`)"
                    " VALUES ('STOPPED')");
      if (not executeSQL(sql))
        return false;
    }
  }

  /*
   * Create our applier thread state information table if we need to.
   */

  /*
   * Table: applier_state
   * Version 1.0: Initial definition
   * Version 1.1: Added originating_server_uuid and originating_commit_id
   */

  sql.clear();
  sql.push_back("COMMIT");
  sql.push_back("CREATE TABLE IF NOT EXISTS `sys_replication`.`applier_state`"
                " (`last_applied_commit_id` BIGINT NOT NULL PRIMARY KEY,"
                " `originating_server_uuid` VARCHAR(36) NOT NULL,"
                " `originating_commit_id` BIGINT NOT NULL,"
                " `status` VARCHAR(20) NOT NULL,"
                " `error_msg` VARCHAR(250))"
                " COMMENT = 'VERSION 1.1'");

  if (not executeSQL(sql))
    return false;

  sql.clear();
  sql.push_back("SELECT COUNT(*) FROM `sys_replication`.`applier_state`");

  {
    sql::ResultSet result_set(1);
    Execute execute(*(_session.get()), true);
    execute.run(sql[0], result_set);
    result_set.next();
    string count= result_set.getString(0);

    /* Must always be at least one row in the table */
    if (count == "0")
    {
      sql.clear();
      sql.push_back("INSERT INTO `sys_replication`.`applier_state`"
                    " (`last_applied_commit_id`, `originating_server_uuid`,"
                    "  `originating_commit_id`, `status`)"
                    " VALUES (0, '', 0, 'STOPPED')");
      if (not executeSQL(sql))
        return false;
    }
  }

  /*
   * Create our message queue table if we need to.
   * Version 1.0: Initial definition
   * Version 1.1: Added originating_server_uuid and originating_commit_id
   */

  sql.clear();
  sql.push_back("COMMIT");
  sql.push_back("CREATE TABLE IF NOT EXISTS `sys_replication`.`queue`"
                " (`trx_id` BIGINT NOT NULL, `seg_id` INT NOT NULL,"
                " `commit_order` BIGINT,"
                " `originating_server_uuid` VARCHAR(36) NOT NULL,"
                " `originating_commit_id` BIGINT NOT NULL,"
                " `msg` BLOB,"
                " PRIMARY KEY(`trx_id`, `seg_id`))"
                " COMMENT = 'VERSION 1.1'");
  if (not executeSQL(sql))
    return false;

  return true;
}

bool ReplicationSchema::setInitialMaxCommitId(uint64_t value)
{
  vector<string> sql;

  sql.push_back("UPDATE `sys_replication`.`applier_state`"
                " SET `last_applied_commit_id` = "
                + lexical_cast<string>(value));

  return executeSQL(sql);
}

} /* namespace slave */
