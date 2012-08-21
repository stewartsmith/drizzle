#! /usr/bin/env python
# -*- mode: python; indent-tabs-mode: nil; -*-
# vim:expandtab:shiftwidth=2:tabstop=2:smarttab:
#
# Copyright (C) 2011 Patrick Crews
# Copyright (C) 2012 M.Sharan Kumar
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

import os
import re
import subprocess

def prepare_drizzleslap(test_executor, test_cmd):
    """ Prepare the server for a drizzleslap test run

    """
    bot = test_executor
    drizzleslap_outfile = os.path.join(bot.logdir,'drizzleslap.out')
    drizzleslap_output = open(drizzleslap_outfile,'w')
    drizzleslap_prep_cmd = ' '.join([test_cmd,'prepare'])      
    bot.logging.info("Preparing database for drizzleslap run...")
    bot.logging.verbose(drizzleslap_prep_cmd)
    drizzleslap_subproc = subprocess.Popen( drizzleslap_prep_cmd
                                          , shell  = True
                                          , env    = bot.working_environment
                                          , stdout = sysbench_output
                                          , stderr = subprocess.STDOUT
                                          )
    drizzleslap_subproc.wait()
    retcode = drizzleslap_subproc.returncode
    drizzleslap_output.close()
    with open(drizzleslap_outfile,'r') as drizzleslap_file:
        output = ''.join(drizzleslap_file.readlines())
        drizzleslap_file.close()
    bot.logging.verbose("drizzleslap_retcode: %d" %(retcode))
    return retcode, output

def execute_drizzleslap(test_executor, test_cmd):
        """ Execute the commandline and return the result.
            We use subprocess as we can pass os.environ dicts and whatnot 

        """
        
        bot = test_executor
        drizzleslap_cmd = ' '.join([test_cmd, 'run'])
        bot.logging.info("Executing :  %s" %(drizzleslap_cmd))
        drizzleslap_outfile = os.path.join(bot.logdir,'drizzleslap.out')
        with open(drizzleslap_outfile,'w') as drizzleslap_output:
            drizzleslap_subproc = subprocess.Popen( drizzleslap_cmd
                                                  , shell  = True
                                                  , env    = bot.working_environment
                                                  , stdout = sysbench_output
                                                  , stderr = subprocess.STDOUT
                                                  )
            drizzleslap_subproc.wait()
            drizzleslap_output.close()
        retcode = drizzleslap_subproc.returncode     

        drizzleslap_file = open(drizzlslap_outfile,'r')
        output = ''.join(drizzleslap_file.readlines())
        bot.logging.debug(output)
        drizzleslap_file.close()
        return retcode, output
        
def process_drizzleslap_output(test_output):
        """ drizzleslap has run, we now check out what we have 
            We also output the data from the run
        
        """
        # This slice code taken from drizzle-automation's drizzleslap handling
        # Slice up the output report into a matrix and insert into the DB.

        
        #TODO complete the regexes
        regexes= {
          'run_id': re.compile()
        , 'engine_name': re.compile()
        , 'test_name': re.compile()
        , 'queries_avg': re.compile()
        , 'queries_min': re.compile()
        , 'queries_max': re.compile()
        , 'total_time': re.compile()
        , 'stddev':re.compile()
        , 'iterations':re.compile()
        , 'concurrency':re.compile()
        , 'concurrency2':re.compile()
        , 'queries_per_client':re.compile()
        }
        run= {}
        for line in test_output.split("\n"):
            for key in regexes.keys():
                result= regexes[key].match(line)
                if result:
                    run[key]= float(result.group(1)) # group(0) is entire match...
        # we set our test output to the regex'd-up data
        # we also make it a single string, separated by newlines
        parsed_test_output = str(run)[1:-1].replace(',','\n').replace("'",'')
        return parsed_test_output            
        
