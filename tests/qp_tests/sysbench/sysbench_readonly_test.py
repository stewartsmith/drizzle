#! /usr/bin/env python
# -*- mode: python; indent-tabs-mode: nil; -*-
# vim:expandtab:shiftwidth=2:tabstop=2:smarttab:
#
# Copyright (C) 2011 Patrick Crews
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

import unittest
import subprocess
import time

from lib.util.sysbench_methods import prepare_sysbench
from lib.util.sysbench_methods import execute_sysbench
from lib.util.mysqlBaseTestCase import mysqlBaseTestCase

# TODO:  make server_options vary depending on the type of server being used here
# drizzle options
#server_requirements = [['innodb.buffer-pool-size=256M innodb.log-file-size=64M innodb.log-buffer-size=8M innodb.thread-concurrency=0 innodb.additional-mem-pool-size=16M table-open-cache=4096 table-definition-cache=4096 mysql-protocol.max-connections=2048']]
# mysql options
#server_requirements = [['innodb_buffer_pool_size=256M innodb_log_file_size=64M innodb_log_buffer_size=8M innodb_thread_concurrency=0 innodb_additional_mem_pool_size=16M table_open_cache=4096 table_definition_cache=4096 max_connections=2048']]
server_requirements = [[]]
servers = []
server_manager = None
test_executor = None

class basicTest(mysqlBaseTestCase):
        
    def test_sysbench_readonly(self):
        self.logging = test_executor.logging
        master_server = servers[0]
        # our base test command
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
       
        # We sleep for a minute to wait
        time.sleep(10) 
        # how many times to run sysbench at each concurrency
        iterations = 1
        
        # various concurrencies to use with sysbench
        #concurrencies = [16, 32, 64, 128, 256, 512, 1024]
        concurrencies = [1, 4, 8 ]

        # start the test!
        for concurrency in concurrencies:
            test_cmd.append("--num-threads=%d" %concurrency)
            # we setup once per concurrency, copying drizzle-automation
            # this should likely change and if not for readonly, then definitely
            # for readwrite

            test_cmd = " ".join(test_cmd)
            retcode, output = prepare_sysbench(test_executor, test_cmd)
            err_msg = ("sysbench 'prepare' phase failed.\n"
                       "retcode:  %d"
                       "output:  %s" %(retcode,output))
            self.assertEqual(retcode, 0, msg = err_msg) 

            for test_iteration in range(iterations):
                retcode, output = execute_sysbench(test_executor, test_cmd)
                self.assertEqual(retcode, 0, msg = output)
                parsed_output = process_sysbench_output(output)
                for line in parsed_output:
                    self.logging.info(line)

    def tearDown(self):
            server_manager.reset_servers(test_executor.name)

