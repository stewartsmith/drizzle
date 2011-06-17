#! /usr/bin/env python
# -*- mode: python; indent-tabs-mode: nil; -*-
# vim:expandtab:shiftwidth=2:tabstop=2:smarttab:
#
# Copyright (C) 2010, 2011 Patrick Crews
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



"""Processes command line options for Drizzle test-runner"""

import os
import sys
import copy
import exceptions
import optparse

# functions
def comma_list_split(option, opt, value, parser):
    """Callback for splitting input expected in list form"""
    cur_list = getattr(parser.values, option.dest,[])
    input_list = value.split(',')
    # this is a hack to work with make target - we
    # don't deal with a dangling ',' in our list
    if '' in input_list:
        input_list.remove('')
    if cur_list:
        value_list = cur_list + input_list 
    else:
        value_list = input_list 
    setattr(parser.values, option.dest, value_list)

def organize_options(args, test_cases):
    """Put our arguments in a nice dictionary
       We use option.dest as dictionary key
       item = supplied input
 ['
    """
    variables = {}
    # we make a copy as the python manual on vars
    # says we shouldn't alter the dictionary returned
    # by vars() - could affect symbol table?
    variables = copy.copy(vars(args))
    variables['test_cases']= test_cases
    # This code should become a function once
    # enough thought has been given to it
    if variables['manualgdb']:
        variables['gdb']=True
    if variables['repeat'] <= 0:
        print "Setting --repeat=1.  You chose a silly value that I will ignore :P"
        variables['repeat'] = 1
    if variables['mode'] == 'randgen' or variables['gendatafile']:
        print "Setting --no-secure-file-priv=True for randgen usage..."
        variables['nosecurefilepriv']=True
    if variables['mode'] == 'cleanup':
        print "Setting --start-dirty=True for cleanup mode..."
        variables['startdirty']=True
    if variables['libeatmydata'] and os.path.exists(variables['libeatmydatapath']):
        # We are using libeatmydata vs. shared mem for server speedup
        print "Using libeatmydata at %s.  Setting --no-shm / not using shared memory for testing..." %(variables['libeatmydatapath'])
        variables['noshm']=True
    return variables

# Create the CLI option parser
parser= optparse.OptionParser(version='%prog (database quality platform aka project steve austin) version 0.1.1')

# find some default values
# assume we are in-tree testing in general and operating from root/test(?)
testdir_default = os.path.abspath(os.getcwd())
workdir_default = os.path.join(testdir_default,'workdir')
clientbindir_default = os.path.abspath(os.path.join(testdir_default,
                                       '../client'))
basedir_default = os.path.split(testdir_default)[0]

config_control_group = optparse.OptionGroup(parser, 
                     "Configuration controls - allows you to specify a file with a number of options already specified")

config_control_group.add_option(
   "--config_file"
    , dest="configfilepath"
    , action='store'
    , default=None # We want to have a file that will be our default defaults file...
    , help="The file that specifies system configuration specs for dbqp to execute tests"
    )

parser.add_option_group(config_control_group)

# We start adding our option groups to the parser
# option groups kept in separate files just for cleanliness /
# organization.  One file induced head pain
# We may just scan the dbqp_opts folder for modules rather
# than list them in the future

option_modules = [ 'system_control'
                 , 'test_control'
                 , 'test_subject_control'
                 , 'environment_control'
                 , 'option_passing'
                 , 'analysis_control'
                 , 'debugger_control'
                 , 'utility_control'
                 ]
option_file_suffix = 'opt'

for option_module in option_modules:
    option_module = os.path.join('lib/dbqp_opts','%s.%s' %(option_module, option_file_suffix))
    execfile(option_module)

# supplied will be those arguments matching an option, 
# and test_cases will be everything else
(args, test_cases)= parser.parse_args()

variables = {}
variables = organize_options(args, test_cases)

