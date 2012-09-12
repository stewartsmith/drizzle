#! /usr/bin/env python
# -*- mode: python; indent-tabs-mode: nil; -*-
# vim:expandtab:shiftwidth=2:tabstop=2:smarttab:
#
# Copyright (C) 2012 Patrick Crews, M.Sharan Kumar
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

servers = []
server_manager = None
test_executor = None

class sysbenchTestCase(mysqlBaseTestCase):

    # initializing test_data ( data for regression analysis )
    def initTestData(self,test_executor,servers):
        self.test_executor = test_executor
        self.logging = test_executor.logging
        self.servers = servers
        self.master_server = servers[0]
        self.test_data = {}
        self.test_cmd = []
        self.iterations = 0
        self.concurrencies = []
 
        # data for results database / regression analysis
        self.test_data['run_date']= datetime.datetime.now().isoformat()
        self.test_data['test_machine'] = socket.gethostname()[0:20]
        self.test_data['test_server_type'] = self.master_server.type
        self.test_data['test_server_revno'], self.test_data['test_server_comment'] = self.master_server.get_bzr_info()
        self.test_data['config_name'] = self.config_name
        
       
    # utility code for configuring and preparing sysbench test
    def prepareSysbench(self,test_cmd,test_executor,servers):

        # creating the initial test data
        self.initTestData(test_executor,servers)
        # creating the initial test command
        self.test_cmd = test_cmd
        # how many times to run sysbench at each concurrency
        self.iterations = 3 
        # various concurrencies to use with sysbench
        self.concurrencies = [16, 32, 64, 128, 256, 512 ] #, 1024 ]
        # self.concurrencies = [ 128, 256, 512 ]
        
        # we setup once.  This is a readonly test and we don't
        # alter the test bed once it is created
        exec_cmd = " ".join(self.test_cmd)
        retcode, output = prepare_sysbench(test_executor, exec_cmd)
        err_msg = ("sysbench 'prepare' phase failed.\n"
                   "retcode:  %d"
                   "output:  %s" %(retcode,output))
        self.assertEqual(retcode, 0, msg = err_msg) 


    # the __main__ code for executing sysbench oltp test
    def executeSysbench(self):

        # executing sysbench test 
        for concurrency in self.concurrencies:
            if concurrency not in self.test_data:
                self.test_data[concurrency] = []
            exec_cmd = " ".join(self.test_cmd)
            exec_cmd += " --num-threads=%d" % concurrency
            for test_iteration in range(self.iterations):
                self.logging.info("Concurrency: %d Iteration: %d" % (concurrency, test_iteration+1) )
                retcode, output = execute_sysbench(self.test_executor, exec_cmd)
                self.assertEqual(retcode, 0, msg = output)
                # This might be inefficient/redundant...perhaps remove later
                parsed_output = process_sysbench_output(output)
                self.logging.info(parsed_output)

                # gathering the data from the output
                self.saveTestData(test_iteration,concurrency,output)

    # utility code for saving test run information for given concurrency
    def saveTestData(self,test_iteration,concurrency,output):
        
        # creating regexes for test result
        regexes={ 'tps':re.compile(r".*transactions\:\s+\d+\D*(\d+\.\d+).*")
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
        self.test_data[concurrency].append(deepcopy(run))
        #return self.test_data

    # If provided with a results_db, we process our data
    # utility code for reporting the test data
    def reportTestData(self,dsn_string,mail_tgt):

        # Report data
        msg_data = []
        test_concurrencies = [i for i in self.test_data.keys() if type(i) is int]
        test_concurrencies.sort()
        for concurrency in test_concurrencies:
            msg_data.append('Concurrency: %s' %concurrency)
            for test_iteration in self.test_data[concurrency]:
                msg_data.append("Iteration: %s || TPS:  %s" %(test_iteration['iteration']+1, test_iteration['tps']))
        for line in msg_data:
            self.logging.info(line)

        # Store / analyze data in results db, if available
        if dsn_string:
            result, msg_data = sysbench_db_analysis(dsn_string, self.test_data)
        self.logging.info(msg_data)

        # mailing sysbench report
        if mail_tgt:
          sendMail(test_executor,mail_tgt,"\n".join(msg_data))

    def tearDown(self):
            server_manager.reset_servers(test_executor.name)

