#! /usr/bin/python
# -*- mode: c; c-basic-offset: 2; indent-tabs-mode: nil; -*-
# vim:expandtab:shiftwidth=2:tabstop=2:smarttab:
#
# Copyright (C) 2010 Patrick Crews
#


"""Processes command line options for Drizzle test-runner"""

import sys
import os
import exceptions
import optparse

# functions
def comma_list_split(option, opt, value, parser):
    """Callback for splitting input expected in list form"""
    setattr(parser.values, option.dest, value.split(','))

def organize_options(args, test_cases):
    """Put our arguments in a nice dictionary
       We use option.dest as dictionary key
       item = supplied input
 
    """
    variables = {}
    variables = vars(args)
    variables['test_cases']= test_cases
    return variables

# Create the CLI option parser
parser= optparse.OptionParser()

# find some default values
# assume we are in-tree testing in general and operating from root/test(?)
testdir_default = os.path.abspath(os.getcwd())
server_default = os.path.abspath(os.path.join(testdir_default,
                                       '../drizzled/drizzled'))
workdir_default = os.path.join(testdir_default,'dtr_work')
clientbindir_default = os.path.abspath(os.path.join(testdir_default,
                                       '../client'))
basedir_default = os.path.split(testdir_default)[0]

# system_control_group - things like verbose, debug, etc
# test-runner affecting options
system_control_group = optparse.OptionGroup(parser, 
                         "Options for the test-runner itself")

system_control_group.add_option(
   "--force"
 , dest="force"
 , action="store_true"
 , default=False
 , help="Set this to continue test execution beyond the first failed test"
 )

system_control_group.add_option(
    "--verbose"
   , dest="verbose"
   , action="store_true"
   , default = False
   , help="Produces extensive output about test-runner state.  Distinct from --debug"
   )

system_control_group.add_option(
    "--debug"
   , dest="debug"
   , action="store_true"
   , default = False
   , help="Provide internal-level debugging output.  Distinct from --verbose"
   )

system_control_group.add_option(
    "--mode"
  , dest="mode"
  , default="dtr"
  , help="Testing mode.  We only support dtr...for now >;) [%default]"
  )

system_control_group.add_option(
    "--record"
  , dest="record"
  , action="store_true"
  , default=False
  , help="Record a testcase result (if the testing mode supports it) [%default]"
  )

parser.add_option_group(system_control_group)

# end system_control_group

# test_control_group - things like suite, do-test, skip-test
# Affect which tests are run
test_control_group = optparse.OptionGroup(parser, 
                         "Options for controlling which tests are executed")


test_control_group.add_option(
    "--suite"
  , dest="suitelist"
  , type='string'
  , action="append"
  , help="The name of the suite containing tests we want. Use one --suite arg for each suite you want to use. [default=autosearch]"
  )

test_control_group.add_option(
    "--suitepath"
  , dest="suitepaths"
  , type='string'
  , action="append"
  , default = []
  , help="The path containing the suite(s) you wish to execute.  Use on --suitepath for each suite you want to use."
  )
test_control_group.add_option(
    "--do-test"
  , dest="dotest"
  , type='string'
  , default = None
  , help="input can either be a prefix or a regex. Will only execute tests that match the provided pattern"
  )

test_control_group.add_option(
    "--skip-test"
  , dest="skiptest"
  , type='string'
  , default = None
  , help = "input can either be a prefix or a regex.  Will exclude tests that match the provided pattern"
  )

parser.add_option_group(test_control_group)
# end test_control_group

# environment options
# define where to find our drizzled, client dirs, working dirs, etc
environment_control_group = optparse.OptionGroup(parser, 
                            "Options for defining the testing environment")

environment_control_group.add_option(
    "--basedir"
  , dest="basedir"
  , type='string'
  , default = basedir_default
  , help = "Pass this argument to signal to the test-runner that this is an in-tree test.  We automatically set a number of variables relative to the argument (client-bindir, serverdir, testdir) [%default]"
  )

environment_control_group.add_option(
    "--serverdir"
  , dest="serverpath"
  , type='string'
  , default = "auto-search"
  , help = "Path to the server executable.  [%default]"
  )

environment_control_group.add_option(
    "--client-bindir"
  , dest="clientbindir"
  , type = 'string'
  , default = "auto-search"
  , help = "Path to the directory containing client program binaries for use in testing [%default]"
  )

environment_control_group.add_option(
    "--testdir"
  , dest="testdir"
  , type = 'string'
  , default = testdir_default
  , help = "Path to the test dir, containing additional files for test execution. [%default]"
  )

environment_control_group.add_option(
    "--workdir"
  , dest="workdir"
  , type='string'
  , default = workdir_default
  , help = "Path to the directory test-run will use to store generated files and directories. [%default]"
  )

environment_control_group.add_option(
    "--no-shm"
  , dest="noshm"
  , action='store_true'
  , default=False
  , help = "By default, we symlink workdir to a location in shm.  Use this flag to not symlink [%default]"
  )

environment_control_group.add_option(
    "--libeatmydata"
  , dest="libeatmydata"
  , action='store_true'
  , default=False
  , help = "Use libeatmydata to disable fsync calls.  This greatly speeds up test execution, but data durability is nil.  Sets LD_PRELOAD to include the libeatmydata library [%default]"
  )

environment_control_group.add_option(
    "--start-dirty"
  , dest="startdirty"
  , action='store_true'
  , default=False
  , help = "Don't try to clean up working directories before test execution [%default]"
  )

environment_control_group.add_option(
    "--no-secure-file-priv"
  , dest = "nosecurefilepriv"
  , action='store_true'
  , default=False
  , help = "Turn off the use of --secure-file-priv=vardir for started servers"
  )

 
parser.add_option_group(environment_control_group)
# end environment control group

parser.add_option(
    "--engine"
   , dest="engine"
   , default = 'innodb'
   , help="Start drizzled using the specified engine [%default]"
   )    



# supplied will be those arguments matching an option, 
# and test_cases will be everything else
(args, test_cases)= parser.parse_args()

variables = {}
variables = organize_options(args, test_cases)

