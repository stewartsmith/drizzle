#! /usr/bin/python
# -*- mode: c; c-basic-offset: 2; indent-tabs-mode: nil; -*-
# vim:expandtab:shiftwidth=2:tabstop=2:smarttab:
#
# Copyright (C) 2010 Patrick Crews
#

""" dbqp.py

DataBase Quality Platform - system for executing various
testing systems and the helper code 

Currently only executing drizzle-test-run tests
But we can compose various combinations of servers, system,
and test definitions to produce various results

"""

# imports
import os
import sys
import lib.test_run_options as test_run_options
from lib.test_mode import handle_mode
from lib.server_mgmt.server_management import serverManager
from lib.sys_mgmt.system_management import systemManager
from lib.test_mgmt.execution_management import executionManager

# functions


# main
variables = test_run_options.variables
system_manager = None
server_manager = None
test_manager = None
test_executor = None
execution_manager = None

try:
    # Some system-level work is constant regardless
    # of the test to be run
    system_manager = systemManager(variables)

    # Create our server_manager
    server_manager = serverManager(system_manager, variables)

    # Get our mode-specific test_manager and test_executor
    (test_manager,test_executor) = handle_mode(variables, system_manager)

    # Gather our tests for execution
    test_manager.gather_tests()

    # Initialize test execution manager
    execution_manager = executionManager(server_manager, system_manager
                                    , test_manager, test_executor
                                    , variables)

    # Execute our tests!
    execution_manager.execute_tests()

except Exception, e:
   print Exception, e

except KeyboardInterrupt:
  print "\n\nDetected <Ctrl>+c, shutting down and cleaning up..."

finally:
# TODO - make a more robust cleanup
# At the moment, runaway servers are our biggest concern
    if server_manager:
        server_manager.cleanup()

