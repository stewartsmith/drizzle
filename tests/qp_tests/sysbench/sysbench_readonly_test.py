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

from lib.util.sysbench_methods import prepare_sysbench
from lib.util.sysbench_methods import execute_sysbench
from lib.util.mysqlBaseTestCase import mysqlBaseTestCase

server_requirements = [['innodb.buffer-pool-size=256M innodb.log-file-size=64M innodb.log-buffer-size=8M innodb.thread-concurrency=0 innodb.additional-mem-pool-size=16M table-open-cache=4096 table-definition-cache=4096 mysql-protocol.max-connections=2048']]
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
                   , "--drizzle-table-engine=innodb" 
                   , "--oltp-read-only=on"
                   , "--oltp-table-size=1000000"
                   , "--drizzle-mysql=on"
                   , "--drizzle-user=root"
                   , "--drizzle-db=test"
                   , "--drizzle-port=%d" %master_server.master_port
                   , "--drizzle-host=localhost"
                   , "--db-driver=drizzle"
                   ]
        
        # how many times to run sysbench at each concurrency
        iterations = 1
        
        # various concurrencies to use with sysbench
        concurrencies = [16, 32, 64, 128, 256, 512, 1024]

        # start the test!
        for concurrency in concurrencies:
            test_cmd.append("--num-threads=%d" %concurrency)
            # we setup once per concurrency, copying drizzle-automation
            # this should likely change and if not for readonly, then definitely
            # for readwrite

            test_cmd = " ".join(test_cmd)
            print concurrency
            print test_cmd

            retcode, output = prepare_sysbench(test_executor, test_cmd)
            err_msg = ("sysbench 'prepare' phase failed.\n"
                       "retcode:  %d"
                       "output:  %s" %(retcode,output))

            self.assertEqual(retcode, 0, msg = err_msg) 
            for test_iteration in iterations:
                test_cmd = " ".join(test_cmd)            
                retcode, output = execute_sysbench(test_cmd, test_executor, servers)
                self.assertEqual(retcode, 0, msg = output)
                parsed_output = process_output(output)
                for line in parsed_output:
                    self.logging.info(line)

    def tearDown(self):
            server_manager.reset_servers(test_executor.name)

