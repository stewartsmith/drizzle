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

from lib.util.drizzleslap_methods import prepare_drizzleslap
from lib.util.drizzleslap_methods import execute_drizzleslap
from lib.util.drizzleslap_methods import process_drizzleslap_output
from lib.util.mysqlBaseTestCase import mysqlBaseTestCase
from lib.util.database_connect import results_db_connect
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
        
    def test_drizzleslap(self):
        self.logging = test_executor.logging
        master_server = servers[0]
        
        # test group
        test_groups = ['guid','guid-scale',
                      'key','key-scale',
                      'mixed','mixed-commit','mixed-commit-scale','mixed-scale',
                      'scan','scan-scale',
                      'update','update-commit','update-commit-scale','update-scale',
                      'write','write-commit','write-commit-scale','write-scale']

        # test options specific to each test group
        test_options = {'guid':" --auto-generate-sql-guid-primary --auto-generate-sql-load-type=write --number-of-queries=100000",
                        'guid-scale':" --auto-generate-sql-guid-primary --auto-generate-sql-load-type=write --auto-generate-sql-execute-number=1000",
                        'key':"--auto-generate-sql-add-autoincrement --auto-generate-sql-load-type=key --number-of-queries=100000",
                        'key-scale':"--auto-generate-sql-add-autoincrement --auto-generate-sql-load-type=key --auto-generate-sql-execute-number=1000",
                        'mixed':" --auto-generate-sql-add-autoincrement --auto-generate-sql-load-type=mixed --number-of-queries=100000",
                        'mixed-commit':" --auto-generate-sql-add-autoincrement --auto-generate-sql-load-type=mixed --number-of-queries=100000 --commit=8",
                        'mixed-commit-scale':" --auto-generate-sql-add-autoincrement --auto-generate-sql-load-type=mixed --auto-generate-sql-execute-number=1000 --commit=8",
                        'mixed-scale':" --auto-generate-sql-add-autoincrement --auto-generate-sql-load-type=mixed --auto-generate-sql-execute-number=1000",
                        'scan':" --auto-generate-sql-add-autoincrement --auto-generate-sql-load-type=read --number-of-queries=100000",
                        'scan-scale':" --auto-generate-sql-add-autoincrement --auto-generate-sql-load-type=read --auto-generate-sql-execute-number=1000",
                        'update':" --auto-generate-sql-add-autoincrement --auto-generate-sql-load-type=update --number-of-queries=100000 --auto-generate-sql-write-number=50000",
                        'update-commit':"  --auto-generate-sql-add-autoincrement --auto-generate-sql-load-type=update --number-of-queries=100000 --auto-generate-sql-write-number=50000 --commit=8 ",
                        'update-commit-scale':" --auto-generate-sql-add-autoincrement --auto-generate-sql-load-type=update --auto-generate-sql-execute-number=1000  --auto-generate-sql-write-number=50000 --commit=8 ",
                        'update-scale':" --auto-generate-sql-add-autoincrement --auto-generate-sql-load-type=update --auto-generate-sql-execute-number=1000 --auto-generate-sql-write-number=50000",
                        'write':"--auto-generate-sql-add-autoincrement --auto-generate-sql-load-type=write --number-of-queries=100000",
                        'write-commit':" --auto-generate-sql-add-autoincrement --auto-generate-sql-load-type=write --number-of-queries=100000 --commit=8",
                        'write-commit-scale':"--auto-generate-sql-add-autoincrement --auto-generate-sql-load-type=write --auto-generate-sql-execute-number=1000 --commit=8 ",
                        'write-scale':" --auto-generate-sql-add-autoincrement --auto-generate-sql-load-type=write --auto-generate-sql-execute-number=1000" 
                       } 

        # our base test command
        test_cmd = [ "drizzleslap_tests"
                   , "--%s-table-engine=innodb" %master_server.type
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
        # how many times to run drizzleslap at each concurrency
        iterations = 10
        
        # setting concurreny for drizzleslap. This concurrency is fixed
        concurrencies = [50]


        # we setup once.  This is a readwrite test and we don't
        # alter the test bed once it is created
        exec_cmd = " ".join(test_cmd)
        retcode, output = prepare_drizzleslap(test_executor, exec_cmd)
        err_msg = ("drizzleslap 'prepare' phase failed.\n"
                   "retcode:  %d"
                   "output:  %s" %(retcode,output))
        self.assertEqual(retcode, 0, msg = err_msg) 

        # start the test!
        for concurrency in concurrencies:
            
            for group in test_groups:
                exec_cmd = " ".join(test_cmd)
                exec_cmd = exec_cmd.join(test_options[group])
                exec_cmd += "--num-threads=%d" %concurrency

                for test_iteration in range(iterations): 

                    retcode, output = execute_drizzleslap(test_executor, exec_cmd)
                    self.assertEqual(retcode, 0, msg = output)
                    parsed_output = process_drizzleslap_output(output)
                    self.logging.info(parsed_output)

                    #gathering the data from the output
# TODO
                    regexes={
                      'run_id':re.compile()
                    , 'engine_name':re.compile()
                    , 'test_name':re.compile()
                    , 'queries_average':re.compile()
                    , 'queries_min':re.compile()
                    , 'queries_max':re.compile()
                    , 'total_time':re.compile()
                    , 'stddev':re.compile()
                    , 'iterations':re.compile()
                    , 'concurrency':re.compile()
                    , 'concurrency2':re.compile()
                    , 'queries_per_client':re.compile()
                    }
            
                    run={}
                    for line in output.split("\n"):
                        for key in regexes:
                            result=regexes[key].match(line)
                            if result:
                                run[key]=float(result.group(1))
          

                    # fetching test results from results_db database
                    sql_select="SELECT * FROM drizzleslap_run_iterations WHERE concurrency=%d AND iteration=%d" % (concurrency,test_iteration)
                    self.logging.info("dsn_string:%s" % dsn_string)
                    fetch=results_db_connect(dsn_string,"select",sql_select)
                    fetch['concurrency']=concurrency
                    fetch['iteration']=test_iteration

                    # deleting record with current concurrency and iteration
                    if fetch['concurrency']==concurrency and fetch['iteration']==test_iteration:
                        sql_delete="DELETE FROM drizzleslap_run_iterations WHERE concurrency=%d AND iteration=%d" % (concurrency,test_iteration)
                        results_db_connect(dsn_string,"delete",sql_delete)
            
                    # updating the results_db database with test results
                    # it for historical comparison over the life of the code...
                    sql_insert="""INSERT INTO  drizzleslap_run_iterations VALUES (%d,'%s','%s',%0.3f,%0.3f,%0.3f,%0.3f,%0.3f,%d,%d,%d,%d )""" % ( 
                                                                           run['run_id'],
                                                                           run['engine_name'],
                                                                           run['test_name'],
                                                                           float(run['queries_avg']),
                                                                           float(run['queries_min']),
                                                                           float(run['queries_max']),
                                                                           float(run['total_time']),
                                                                           float(run['stddev']),
                                                                           int(run['iterations']),
                                                                           int(run['concurrency']),
                                                                           int(run['concurrency2']),
                                                                           int(run['queries_per_client']) )
            
                    results_db_connect(dsn_string,"insert",sql_insert)

#TODO get drizzleslap test result ( should modify this and add util method too )
                #getting test result as report for sysbench
                sys_report=getSysbenchReport(run,fetch)
           
                #mailing sysbench report
                if mail_tgt:
                  kewpieSendMail(test_executor,mail_tgt,sys_report)


    def tearDown(self):
            server_manager.reset_servers(test_executor.name)

