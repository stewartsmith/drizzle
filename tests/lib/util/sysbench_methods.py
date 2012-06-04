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

import os
import subprocess

def execute_sqlbench(test_cmd, test_executor, servers):
    """ Execute the commandline and return the result.
        We use subprocess as we can pass os.environ dicts and whatnot 

    """
    
    bot = test_executor
    sqlbench_outfile = os.path.join(bot.logdir,'sqlbench.out')
    sqlbench_output = open(sqlbench_outfile,'w')
    bot.logging.info("Executing sqlbench:  %s" %(test_cmd))
    bot.logging.info("This may take some time...")
    sqlbench_subproc = subprocess.Popen( test_cmd
                                       , shell=True
                                       , cwd=os.path.join(bot.system_manager.testdir, 'sql-bench')
                                       , env=bot.working_environment
                                       , stdout = sqlbench_output
                                       , stderr = subprocess.STDOUT
                                       )
    sqlbench_subproc.wait()
    retcode = sqlbench_subproc.returncode     

    sqlbench_output.close()
    sqlbench_file = open(sqlbench_outfile,'r')
    output = ''.join(sqlbench_file.readlines())
    sqlbench_file.close()

    bot.current_test_retcode = retcode
    bot.current_test_output = output
    test_status = process_sqlbench_output(bot)
    return test_status, retcode, output

def process_sqlbench_output(bot):
        
    # Check for 'Failed' in sql-bench output
    # The tests don't die on a failed test and
    # require some checking of the output file
    error_flag = False
    for inline in bot.current_test_output:
        if 'Failed' in inline:
            error_flag= True
            logging.info(inline.strip())
    if bot.current_test_retcode == 0 and not error_flag:
        return 'pass'
    else:
        return 'fail'

    def prepare_sysbench(test_executor, test_cmd):
        """ Prepare the server for a sysbench run

        """
        bot = test_executor
        sysbench_outfile = os.path.join(bot.logdir,'sysbench.out')
        sysbench_output = open(sysbench_outfile,'w')
        sysbench_prep_cmd = ' '.join([test_cmd,'prepare'])      
        bot.logging.info("Preparing database for sysbench run...")
        bot.logging.verbose(sysbench_prep_cmd)
        sysbench_subproc = subprocess.Popen( sysbench_prep_cmd
                                           , shell  = True
                                           , env    = bot.working_environment
                                           , stdout = sysbench_output
                                           , stderr = subprocess.STDOUT
                                           )
        sysbench_subproc.wait()
        retcode = sysbench_subproc.returncode
        sysbench_output.close()
        with open(sysbench_outfile,'r') as sysbench_file:
            output = ''.join(sysbench_file.readlines())
            sysbench_file.close()
        bot.logging.verbose("sysbench_retcode: %d" %(retcode))
        return retcode, output

    def execute_sysbench(test_executor, test_cmd):
        """ Execute the commandline and return the result.
            We use subprocess as we can pass os.environ dicts and whatnot 

        """
        
        bot = test_executor
        sysbench_cmd = ' '.join([test_cmd, 'run'])
        self.logging.info("Executing sysbench:  %s" %(sysbench_cmd))
        sysbench_outfile = os.path.join(bot.logdir,'sysbench.out')
        with open(sysbench_outfile,'w') as sysbench_output:
            sysbench_subproc = subprocess.Popen( sysbench_cmd
                                               , shell  = True
                                               , env    = bot.working_environment
                                               , stdout = sysbench_output
                                               , stderr = subprocess.STDOUT
                                               )
            sysbench_subproc.wait()
            sysbench_output.close()
        retcode = sysbench_subproc.returncode     

        sysbench_file = open(sysbench_outfile,'r')
        output = ''.join(sysbench_file.readlines())
        self.logging.debug(output)
        sysbench_file.close()
        return retcode, output
        
    def process_sysbench_output(test_output):
        """ sysbench has run, we now check out what we have 
            We also output the data from the run
        
        """
        # This slice code taken from drizzle-automation's sysbench handling
        # Slice up the output report into a matrix and insert into the DB.
        regexes= {
          'tps': re.compile(r".*transactions\:\s+\d+\D*(\d+\.\d+).*")
        , 'deadlocksps': re.compile(r".*deadlocks\:\s+\d+\D*(\d+\.\d+).*")
        , 'rwreqps': re.compile(r".*read\/write\s+requests\:\s+\d+\D*(\d+\.\d+).*")
        , 'min_req_lat_ms': re.compile(r".*min\:\s+(\d*\.\d+)ms.*")
        , 'max_req_lat_ms': re.compile(r".*max\:\s+(\d*\.\d+)ms.*")
        , 'avg_req_lat_ms': re.compile(r".*avg\:\s+(\d*\.\d+)ms.*")
        , '95p_req_lat_ms': re.compile(r".*approx.\s+95\s+percentile\:\s+(\d+\.\d+)ms.*")
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
        
