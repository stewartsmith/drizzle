#! /usr/bin/python
# -*- mode: c; c-basic-offset: 2; indent-tabs-mode: nil; -*-
# vim:expandtab:shiftwidth=2:tabstop=2:smarttab:
#
# Copyright (C) 2010 Patrick Crews
#
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
    # drizzle-test-run mode - the default
    if variables['mode'] == 'dtr':
        # DTR mode - this is what we are coding to initially
        # We are just setting the code up this way to hopefully make
        # other coolness easier in the future

        system_manager.logging.info("Using testing mode: %s" %variables['mode'])

        # Set up our testManager
        from drizzle_test_run.dtr_test_management import testManager
        test_manager = testManager( variables['verbose'], variables['debug'] 
                                  , variables['engine'], variables['dotest']
                                  , variables['skiptest'], variables['reorder']
                                  , variables['suitelist'], variables['suitepaths']
                                  , system_manager, variables['test_cases'])

        # get our mode-specific testExecutor
        from drizzle_test_run.dtr_test_execution import dtrTestExecutor
        
        return (test_manager, dtrTestExecutor)

    else:
        system_manager.logging.error("unknown mode argument: %s" %variables['mode'])
        sys.exit(1)
