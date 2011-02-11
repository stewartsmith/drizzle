#! /usr/bin/env python
# -*- mode: c; c-basic-offset: 2; indent-tabs-mode: nil; -*-
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
    # drizzle-test-run mode - the default
    if variables['mode'] == 'dtr':
        # DTR mode - this is what we are coding to initially
        # We are just setting the code up this way to hopefully make
        # other coolness easier in the future

        system_manager.logging.info("Using testing mode: %s" %variables['mode'])

        # Set up our testManager
        from drizzle_test_run.dtr_test_management import testManager
        test_manager = testManager( variables['verbose'], variables['debug'] 
                                  , variables['defaultengine'], variables['dotest']
                                  , variables['skiptest'], variables['reorder']
                                  , variables['suitelist'], variables['suitepaths']
                                  , system_manager, variables['test_cases'])

        # get our mode-specific testExecutor
        from drizzle_test_run.dtr_test_execution import dtrTestExecutor
        
        return (test_manager, dtrTestExecutor)

    else:
        system_manager.logging.error("unknown mode argument: %s" %variables['mode'])
        sys.exit(1)
