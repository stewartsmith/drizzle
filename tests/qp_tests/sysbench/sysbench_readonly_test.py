#! /usr/bin/env python
# -*- mode: python; indent-tabs-mode: nil; -*-
# vim:expandtab:shiftwidth=2:tabstop=2:smarttab:
#
# Copyright (C) 2011 Patrick Crews
# Copyright (C) 2012 M.Sharan Kumar
#
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA

import re
import time
import socket
import subprocess
import datetime
from copy import deepcopy

from lib.util.sysbench_methods import prepare_sysbench
from lib.util.sysbench_methods import execute_sysbench
from lib.util.sysbench_methods import process_sysbench_output
from lib.util.sysbench_methods import sysbench_db_analysis
from lib.util.sysbench_methods import getSysbenchReport
from lib.util.sysbenchTestCase import sysbenchTestCase
from lib.util.database_connect import results_db_connect
from lib.util.mailing_report   import sendMail

# TODO:  make server_options vary depending on the type of server being used here
# drizzle options
server_requirements = [['innodb.buffer-pool-size=256M innodb.log-file-size=64M innodb.log-buffer-size=8M innodb.thread-concurrency=0 innodb.additional-mem-pool-size=16M table-open-cache=4096 table-definition-cache=4096 mysql-protocol.max-connections=2048']]

# mysql options
#server_requirements = [['innodb_buffer_pool_size=256M innodb_log_file_size=64M innodb_log_buffer_size=8M innodb_thread_concurrency=0 innodb_additional_mem_pool_size=16M table_open_cache=4096 table_definition_cache=4096 max_connections=2048']]

servers = []
server_manager = None
test_executor = None

class basicTest(sysbenchTestCase):

    def test_sysbench_readonly(self):

        # defining the test command
        master_server = servers[0]
        self.config_name = 'innodb_1000K_readonly'
        test_cmd = [ "sysbench"
                   , "--max-time=240"
                   , "--max-requests=0"
                   , "--test=oltp"
                   , "--db-ps-mode=disable"
                   , "--%s-table-engine=innodb" %master_server.type
                   , "--oltp-read-only=on"
                   , "--oltp-table-size=1000000"
                   , "--%s-user=root" %master_server.type
                   , "--%s-db=test" %master_server.type
                   , "--%s-port=%d" %(master_server.type, master_server.master_port)
                   , "--%s-host=localhost" %master_server.type
                   , "--db-driver=%s" %master_server.type
                   ]
        if master_server.type == 'drizzle':
            test_cmd.append("--drizzle-mysql=on")
        if master_server.type == 'mysql':
            test_cmd.append("--mysql-socket=%s" %master_server.socket_file)

        # preparing sysbench_readonly test
        time.sleep(20)
        self.prepareSysbench(test_cmd,test_executor,servers)

        # start the test!
        # this method takes care of *running* the test and *saving* the test results
        self.executeSysbench()

        # reporting the test result
        # this method handles *dsn_string* and *mail_tgt*
        self.reportTestData(dsn_string,mail_tgt)

    def tearDown(self):
            server_manager.reset_servers(test_executor.name)
