#! /usr/bin/python
# -*- mode: c; c-basic-offset: 2; indent-tabs-mode: nil; -*-
# vim:expandtab:shiftwidth=2:tabstop=2:smarttab:
#
# Copyright (C) 2010 Patrick Crews
#
""" dtr_test_execution:
    code related to the execution of dtr test cases 
    
    We are provided access to a testManager with 
    dtr-specific testCases.  We contact teh executionManager
    to produce the system and server configurations we need
    to execute a test.

"""

# imports
import os
import sys
import subprocess
import commands

import lib.test_mgmt.test_execution as test_execution

class dtrTestExecutor(test_execution.testExecutor):
    """ dtr-specific testExecutor 
        We currently execute by sending test-case
        data to client/drizzletest...for now

    """
  

    def execute_testCase (self):
        """ Execute a dtr testCase via calls to drizzletest (boo)
            Eventually, we will replace drizzletest with pythonic
            goodness, but we have these classes stored here for the moment

        """
        test_execution.testExecutor.execute_testCase(self)
        self.status = 0
    
        # generate command line
        drizzletest_cmd = self.generate_drizzletest_call()
        if self.debug:
            self.logging.debug(drizzletest_cmd)

        # call drizzletest
        self.process_environment_reqs()
        (retcode, output) = self.execute_drizzletest(drizzletest_cmd)

        # analyze results
        self.current_test_status = self.process_drizzletest_output(retcode, output)
        self.set_server_status(self.current_test_status)

        # update the test_manager
        self.test_manager.record_test_result(self.current_testcase,self.current_test_status, output)

        
        return self.current_test_status

    def generate_drizzletest_call(self):
        """ Produce the command line we use to call drizzletest
            We have a healthy number of values, so we put this in a 
            nice function

        """

        drizzletest_arguments = [ '--no-defaults'
                                , '--silent'
                                , '--tmpdir=%s' %(self.master_server.tmpdir)
                                , '--logdir=%s' %(self.master_server.logdir)
                                , '--port=%d' %(self.master_server.master_port)
                                , '--database=test'
                                , '--user=root'
                                , '--password='
                                #, '--testdir=%s' %(self.test_manager.testdir)
                                , '--test-file=%s' %(self.current_testcase.testpath)
                                , '--tail-lines=20'
                                , '--timer-file=%s' %(self.master_server.timer_file)
                                , '--result-file=%s' %(self.current_testcase.resultpath)
                                ]
        if self.record_flag:
            # We want to record a new result
            drizzletest_arguments.append('--record')
        drizzletest_cmd = "%s %s" %(self.system_manager.code_tree.drizzletest, " ".join(drizzletest_arguments))
        return drizzletest_cmd

    def execute_drizzletest(self, drizzletest_cmd):
        """ Execute the commandline and return the result.
            We use subprocess as we can pass os.environ dicts and whatnot 

        """

        retcode, output = commands.getstatusoutput(drizzletest_cmd)
        if self.debug:
            self.logging.debug("drizzletest_retcode: %d" %(retcode))
            self.logging.debug("drizzletest_output: %s" %(output))
        return (retcode, output)

    def process_drizzletest_output(self, retcode, output):
        """ Drizzletest has run, we now check out what we have """
        if retcode == 0:
            return 'pass'
        elif retcode == 62:
            return 'skipped'
        elif retcode == 63 or retcode == 1:
            return 'fail'
        else:
            return 'fail'

    def process_environment_reqs(self):
        """ We generate the ENV vars we need set
            and then ask systemManager to do so

        """
        
        env_reqs = { 'DRIZZLETEST_VARDIR': (self.master_server.vardir,0,0)
                   ,  'DRIZZLE_TMP_DIR': (self.master_server.tmpdir,0,0)
                   ,  'MASTER_MYSOCK': (self.master_server.socket_file,0,0)
                   ,  'MASTER_MYPORT': (str(self.master_server.master_port),0,0)
                   ,  'MC_PORT': (str(self.master_server.mc_port),0,0)
                   ,  'PBMS_PORT': (str(self.master_server.pbms_port),0,0)
                   ,  'DRIZZLE_TCP_PORT': (str(self.master_server.drizzle_tcp_port),0,0)
                   ,  'EXE_DRIZZLE': (self.master_server.drizzle_client,0,0)
                   ,  'DRIZZLE_DUMP': ("%s --no-defaults -uroot -p%d" %( self.master_server.drizzledump
                                                        , self.master_server.master_port),0,0)
                   ,  'DRIZZLE_SLAP': ("%s -uroot -p%d" %( self.master_server.drizzleslap
                                                        , self.master_server.master_port),0,0)
                   ,  'DRIZZLE_IMPORT': ("%s -uroot -p%d" %( self.master_server.drizzleimport
                                                          , self.master_server.master_port),0,0)
                   ,  'DRIZZLE': ("%s -uroot -p%d" %( self.master_server.drizzle_client
                                                   , self.master_server.master_port),0,0)
                   ,  'DRIZZLE_ADMIN' : ("%s -uroot -p%d" %( self.master_server.drizzleadmin
                                                         , self.master_server.master_port),0,0)
                   }     

        self.system_manager.process_environment_reqs(env_reqs, quiet=1)

    def set_server_status(self, test_status):
        """ We update our server to indicate if a test passed or failed """


        
