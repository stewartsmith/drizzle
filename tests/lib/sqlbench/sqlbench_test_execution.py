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

""" sqlbench_test_execution:
    code related to the execution of sqlbench test cases 
    
    We are provided access to a testManager with 
    sqlbench-specific testCases.  

"""

# imports
import os
import re
import sys
import subprocess
import commands

import lib.test_mgmt.test_execution as test_execution

class sqlbenchTestExecutor(test_execution.testExecutor):
    """ sqlbench-specific testExecutor 
        
    """
  
    def execute_testCase (self):
        """ Execute a sqlbench testCase

        """
        test_execution.testExecutor.execute_testCase(self)
        self.status = 0

        # execute sqlbench
        self.execute_sqlbench()

        # analyze results
        self.current_test_status = self.process_sqlbench_output()
        self.set_server_status(self.current_test_status)
        self.server_manager.reset_servers(self.name)
 
    def execute_sqlbench(self):
        """ Execute the commandline and return the result.
            We use subprocess as we can pass os.environ dicts and whatnot 

        """
      
        testcase_name = self.current_testcase.fullname
        self.time_manager.start(testcase_name,'test')
        sqlbench_outfile = os.path.join(self.logdir,'sqlbench.out')
        sqlbench_output = open(sqlbench_outfile,'w')
        sqlbench_cmd = self.current_testcase.test_command
        self.logging.info("Executing sqlbench:  %s" %(sqlbench_cmd))
        
        sqlbench_subproc = subprocess.Popen( sqlbench_cmd
                                         , shell=True
                                         , cwd=os.path.join(self.system_manager.testdir, 'sql-bench')
                                         , env=self.working_environment
                                         , stdout = sqlbench_output
                                         , stderr = subprocess.STDOUT
                                         )
        sqlbench_subproc.wait()
        retcode = sqlbench_subproc.returncode     
        execution_time = int(self.time_manager.stop(testcase_name)*1000) # millisec

        sqlbench_output.close()
        sqlbench_file = open(sqlbench_outfile,'r')
        output = ''.join(sqlbench_file.readlines())
        if self.debug:
            self.logging.debug(output)
        sqlbench_file.close()

        if self.debug:
            self.logging.debug("sqlbench_retcode: %d" %(retcode))
        self.current_test_retcode = retcode
        self.current_test_output = output
        self.current_test_exec_time = execution_time

    def process_sqlbench_output(self):
        
                    
        if self.current_test_retcode == 0:
            return 'pass'
        else:
            return 'fail'

    def handle_system_reqs(self):
        """ We check our test case and see what we need to do
            system-wise to get ready.  This is likely to be 
            mode-dependent and this is just a placeholder
            method

        """

        self.process_environment_reqs()
        self.process_symlink_reqs()
        self.process_master_sh()  
        return

    def process_master_sh(self):
        """ We do what we need to if we have a master.sh file """
        if self.current_testcase.master_sh:
            retcode, output = self.system_manager.execute_cmd("/bin/sh %s" %(self.current_testcase.master_sh))
            if self.debug:
                self.logging.info("retcode: %retcode")
                self.logging.info("%output")

    def process_environment_reqs(self):
        """ We generate the ENV vars we need set
            and then ask systemManager to do so

        """
        env_reqs = {  'DRIZZLETEST_VARDIR': self.master_server.vardir
                   ,  'DRIZZLE_TMP_DIR': self.master_server.tmpdir
                   ,  'MASTER_MYSOCK': self.master_server.socket_file
                   ,  'MASTER_MYPORT': str(self.master_server.master_port)
                   ,  'MC_PORT': str(self.master_server.mc_port)
                   ,  'PBMS_PORT': str(self.master_server.pbms_port)
                   ,  'RABBITMQ_NODE_PORT': str(self.master_server.rabbitmq_node_port)
                   ,  'DRIZZLE_TCP_PORT': str(self.master_server.drizzle_tcp_port)
                   ,  'EXE_DRIZZLE': self.master_server.drizzle_client
                   ,  'MASTER_SERVER_SLAVE_CONFIG' : self.master_server.slave_config_file
                   ,  'DRIZZLE_DUMP': "%s --no-defaults -uroot -p%d" %( self.master_server.drizzledump
                                                        , self.master_server.master_port)
                   ,  'DRIZZLE_SLAP': "%s -uroot -p%d" %( self.master_server.drizzleslap
                                                        , self.master_server.master_port)
                   ,  'DRIZZLE_IMPORT': "%s -uroot -p%d" %( self.master_server.drizzleimport
                                                          , self.master_server.master_port)
                   ,  'DRIZZLE': "%s -uroot -p%d" %( self.master_server.drizzle_client
                                                   , self.master_server.master_port)
                   ,  'DRIZZLE_ADMIN' : "%s -uroot -p%d" %( self.master_server.drizzleadmin
                                                         , self.master_server.master_port)
                   ,  'DRIZZLE_BASEDIR' : self.system_manager.code_tree.basedir
                   ,  'DRIZZLE_TRX_READER' : self.system_manager.code_tree.drizzle_trx_reader
                   ,  'DRIZZLE_TEST_WORKDIR' : self.system_manager.workdir
                   }     


        self.working_environment = self.system_manager.create_working_environment(env_reqs)


    def process_symlink_reqs(self):
        """ Create any symlinks we may need """
        needed_symlinks = []

        self.system_manager.create_symlinks(needed_symlinks)

    
   

        
