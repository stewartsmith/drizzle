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
from lib.util.mysqlBaseTestCase import mysqlBaseTestCase
from lib.util.database_connect import results_db_connect
from lib.util.mailing_report import sendMail


# TODO:  make server_options vary depending on the type of server being used here
# drizzle options
server_requirements = [['innodb.buffer-pool-size=256M innodb.log-file-size=64M innodb.log-buffer-size=8M innodb.thread-concurrency=0 innodb.additional-mem-pool-size=16M table-open-cache=4096 table-definition-cache=4096 mysql-protocol.max-connections=2048']]

# mysql options
#server_requirements = [['innodb_buffer_pool_size=256M innodb_log_file_size=64M innodb_log_buffer_size=8M innodb_thread_concurrency=0 innodb_additional_mem_pool_size=16M table_open_cache=4096 table_definition_cache=4096 max_connections=2048']]

servers = []
server_manager = None
test_executor = None

class basicTest(mysqlBaseTestCase):
    def test_sysbench_readonly(self):
        self.logging = test_executor.logging
        master_server = servers[0]

        # data for results database / regression analysis
        test_data = {}
        test_data['run_date']= datetime.datetime.now().isoformat()
        test_data['test_machine'] = socket.gethostname()
        test_data['test_server_type'] = master_server.type
        test_data['test_server_revno'], test_data['test_server_comment'] = master_server.get_bzr_info()
        test_data['config_name'] = 'sysbench_readwrite1000k'
            
        # our base test command
        test_cmd = [ "sysbench"
                   , "--max-time=240"
                   , "--max-requests=0"
                   , "--test=oltp"
                   , "--db-ps-mode=disable"
                   , "--%s-table-engine=innodb" %master_server.type
                   , "--oltp-read-only=off"
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
       
        # how many times to run sysbench at each concurrency
        iterations = 3 
        
        # various concurrencies to use with sysbench
        concurrencies = [16, 32, 64, 128, 256, 512, 1024 ]
        # concurrencies = [ 128, 256, 512 ]

        # we setup once.  This is a readonly test and we don't
        # alter the test bed once it is created
        exec_cmd = " ".join(test_cmd)
        retcode, output = prepare_sysbench(test_executor, exec_cmd)
        err_msg = ("sysbench 'prepare' phase failed.\n"
                   "retcode:  %d"
                   "output:  %s" %(retcode,output))
        self.assertEqual(retcode, 0, msg = err_msg) 

        # start the test!
        for concurrency in concurrencies:
            if concurrency not in test_data:
                test_data[concurrency] = []
            exec_cmd = " ".join(test_cmd)
            exec_cmd += "--num-threads=%d" %concurrency
            for test_iteration in range(iterations):
                self.logging.info("Iteration: %d" %test_iteration)
                retcode, output = execute_sysbench(test_executor, exec_cmd)
                self.assertEqual(retcode, 0, msg = output)
                # This might be inefficient/redundant...perhaps remove later
                parsed_output = process_sysbench_output(output)
                self.logging.info(parsed_output)

                #gathering the data from the output
                regexes={
                  'tps':re.compile(r".*transactions\:\s+\d+\D*(\d+\.\d+).*")
                , 'min_req_lat_ms':re.compile(r".*min\:\s+(\d*\.\d+)ms.*")
                , 'max_req_lat_ms':re.compile(r".*max\:\s+(\d*\.\d+)ms.*")
                , 'avg_req_lat_ms':re.compile(r".*avg\:\s+(\d*\.\d+)ms.*")
                , '95p_req_lat_ms':re.compile(r".*approx.\s+95\s+percentile\:\s+(\d+\.\d+)ms.*")
                , 'rwreqps':re.compile(r".*read\/write\s+requests\:\s+\d+\D*(\d+\.\d+).*")
                , 'deadlocksps':re.compile(r".*deadlocks\:\s+\d+\D*(\d+\.\d+).*")
                }
            
                run={}
                for line in output.split("\n"):
                    for key in regexes:
                        result=regexes[key].match(line)
                        if result:
                            run[key]=float(result.group(1))
                run['mode']="readonly"
                run['iteration'] = test_iteration
                test_data[concurrency].append(deepcopy(run))

        # If provided with a results_db, we process our data

        # Report data
        msg_data = []
        test_concurrencies = [i for i in test_data.keys() if type(i) is int]
        test_concurrencies.sort()
        for concurrency in test_concurrencies:
            msg_data.append('Concurrency: %s' %concurrency)
            for test_iteration in test_data[concurrency]:
                msg_data.append("Iteration: %s || TPS:  %s" %(test_iteration['iteration'], test_iteration['tps']))
        for line in msg_data:
            self.logging.info(line)

        # Store / analyze data in results db, if available
        if dsn_string:
            result, msg_data = sysbench_db_analysis(dsn_string, test_data)
        print result
        print msg_data

        # mailing sysbench report
        if mail_tgt:
          sendMail(test_executor,mail_tgt,"\n".join(msg_data))

    def tearDown(self):
            server_manager.reset_servers(test_executor.name)

