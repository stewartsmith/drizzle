#! /usr/bin/env python
# -*- mode: python; indent-tabs-mode: nil; -*-
# vim:expandtab:shiftwidth=2:tabstop=2:smarttab:
#
# Copyright (C) 2010 Patrick Crews
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

"""test_mode.py
   code for dealing with testing modes
   A given mode should have a systemInitializer, testManager, and testExecutor
   that define how to setup, manage, and execute test cases

"""

# imports
import sys

def handle_mode(variables, system_manager):
    """ Deals with the 'mode' option and returns
        the appropriate code objects for the test-runner to play with

    """

    test_mode = variables['mode']
    system_manager.logging.info("Using testing mode: %s" %test_mode)

    # drizzle-test-run mode - the default
    if test_mode == 'dtr':
        # DTR mode - this is what we are coding to initially
        # We are just setting the code up this way to hopefully make
        # other coolness easier in the future

        # get our mode-specific testManager and Executor
        from drizzle_test_run.dtr_test_management import testManager
        from drizzle_test_run.dtr_test_execution import testExecutor as testExecutor

    elif test_mode == 'randgen':
        # randgen mode - we run the randgen grammar against
        # the specified server configs and report the randgen error code

        # get manager and executor
        from randgen.randgen_test_management import testManager
        from randgen.randgen_test_execution import randgenTestExecutor as testExecutor

    elif test_mode == 'sysbench':
        # sysbench mode - we have a standard server setup 
        # and a variety of concurrencies we want to run

        # get manager and executor
        from sysbench.sysbench_test_management import testManager
        from sysbench.sysbench_test_execution import sysbenchTestExecutor as testExecutor

    elif test_mode == 'sqlbench':
        # sqlbench mode - we execute all test sql-bench tests
        # - run-all-tests

        # get manager and executor
        from sqlbench.sqlbench_test_management import testManager
        from sqlbench.sqlbench_test_execution import sqlbenchTestExecutor as testExecutor

    elif test_mode == 'crashme':
        # crashme mode - see if the server lives : )
        # get manager and executor
        from sqlbench.sqlbench_test_management import crashmeTestManager as testManager
        from sqlbench.sqlbench_test_execution import crashmeTestExecutor as testExecutor

    elif test_mode == 'cleanup':
        # cleanup mode - we try to kill any servers whose pid's we detect
        # in our workdir.  Might extend to other things (file cleanup, etc)
        # at some later point
        system_manager.cleanup(exit=True)
 
    else:
        system_manager.logging.error("unknown mode argument: %s" %variables['mode'])
        sys.exit(1)

    test_manager = testManager( variables['verbose'], variables['debug'] 
                              , variables['defaultengine'], variables['dotest']
                              , variables['skiptest'], variables['reorder']
                              , variables['suitelist'], variables['suitepaths']
                              , system_manager, variables['test_cases']
                              , variables['mode'] )

    return (test_manager, testExecutor)

