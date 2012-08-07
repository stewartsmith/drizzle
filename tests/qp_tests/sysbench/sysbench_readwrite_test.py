#! /usr/bin/env python
# -*- mode: python; indent-tabs-mode: nil; -*-
# vim:expandtab:shiftwidth=2:tabstop=2:smarttab:
#
# Copyright (C) 2011 Patrick Crews
# Copyright (C) 2012 Sharan Kumar
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
import re

from lib.util.sysbench_methods import prepare_sysbench
from lib.util.sysbench_methods import execute_sysbench
from lib.util.sysbench_methods import process_sysbench_output
from lib.util.mysqlBaseTestCase import mysqlBaseTestCase
from lib.util.database_connect import results_db_connect
from lib.util.sysbench_report import getSysbenchReport
from lib.util.mailing_report import kewpieSendMail
from lib.opts.test_run_options import parse_qp_options

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
        
    def test_sysbench_readwrite(self):
        self.logging = test_executor.logging
        master_server = servers[0]
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
       
        # We sleep for a minute to wait
        time.sleep(10) 
        # how many times to run sysbench at each concurrency
        iterations = 3
        
        # various concurrencies to use with sysbench
        #concurrencies = [4,8,16, 32, 64, 128, 256, 512, 1024]
        concurrencies = [1]


        # we setup once.  This is a readwrite test and we don't
        # alter the test bed once it is created
        exec_cmd = " ".join(test_cmd)
        retcode, output = prepare_sysbench(test_executor, exec_cmd)
        err_msg = ("sysbench 'prepare' phase failed.\n"
                   "retcode:  %d"
                   "output:  %s" %(retcode,output))
        self.assertEqual(retcode, 0, msg = err_msg) 

        # start the test!
        for concurrency in concurrencies:

            # pre-preparation
            # queries = ["DROP SCHEMA test",
            #            "CREATE SCHEMA test"
            #           ]
            # retcode, result = self.execute_queries(queries, master_server, schema = "INFORMATION_SCHEMA")

            # preparing the test bed for each concurrency
            # retcode, output = prepare_sysbench(test_executor, exec_cmd)
            # err_msg = ("sysbench 'prepare' phase failed.\n"
            #            "retcode: %d"
            #            "output:  %s" %(retcode,output))
            # self.assertEqual(retcode, 0, msg=err_msg)
            
            
            exec_cmd = " ".join(test_cmd)
            exec_cmd += "--num-threads=%d" %concurrency
            for test_iteration in range(iterations): 

                # pre-preparation

                # queries = ["DROP SCHEMA test",
                #            "CREATE SCHEMA test"
                #           ]
                # retcode, result = self.execute_queries(queries, master_server, schema = "INFORMATION_SCHEMA")

                # preparing test bed for each iteration

                # retcode, output = prepare_sysbench(test_executor, exec_cmd)
                # err_msg = ("sysbench 'prepare' phase failed. \n"
                #            "retcode:  %d"
                #            "output:   %s" %(retcode,output))
                # self.assertEqual(retcode, 0, msg = err_msg)
          
                # executing sysbench tests
                retcode, output = execute_sysbench(test_executor, exec_cmd)
                self.assertEqual(retcode, 0, msg = output)
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
                run['mode']="readwrite"

                # fetching test results from results_db database
                sql_select="SELECT * FROM sysbench_run_iterations WHERE concurrency=%d AND iteration=%d" % (concurrency,test_iteration)
                self.logging.info("dsn_string:%s" % dsn_string)
                fetch=results_db_connect(dsn_string,"select",sql_select)
                fetch['concurrency']=concurrency
                fetch['iteration']=test_iteration

                # deleting record with current concurrency and iteration
                if fetch['concurrency']==concurrency and fetch['iteration']==test_iteration:
                    sql_delete="DELETE FROM sysbench_run_iterations WHERE concurrency=%d AND iteration=%d" % (concurrency,test_iteration)
                    results_db_connect(dsn_string,"delete",sql_delete)
            
                # updating the results_db database with test results
                # it for historical comparison over the life of the code...
                sql_insert="""INSERT INTO  sysbench_run_iterations VALUES (%d, %0.2f, %0.2f, %0.2f, %0.2f, %0.2f, %0.2f, %0.2f, %d)""" % ( 
                                                                           concurrency,
                                                                           run['tps'],
                                                                           run['min_req_lat_ms'],
                                                                           run['max_req_lat_ms'],
                                                                           run['avg_req_lat_ms'],
                                                                           run['95p_req_lat_ms'],
                                                                           run['rwreqps'],
                                                                           run['deadlocksps'],
                                                                           test_iteration  )
            
                results_db_connect(dsn_string,"insert",sql_insert)

            #report generation
                self.logging.info("Generating regression report...")
#            print """==========================================================================
#field		value in database	recorded value		regression
#==========================================================================
#                  """
#
#            for key in fetch.keys():
#                print key,"\t\t",fetch[key],"\t\t",run[key],"\t\t",run[key]-fetch[key]
#            print "=========================================================================="

            #getting test result as report
                sys_report=getSysbenchReport(run,fetch)
           
                #mailing sysbench report
                if mail_tgt:
                  #sysbenchSendMail(test_executor,'sharan.monikantan@gmail.com',sys_report)
                  kewpieSendMail(test_executor,mail_tgt,sys_report)


    def tearDown(self):
            server_manager.reset_servers(test_executor.name)

