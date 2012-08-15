#! /usr/bin/env python
# -*- mode: python; indent-tabs-mode: nil; -*-
# vim:expandtab:shiftwidth=2:tabstop=2:smarttab:
#
# Copyright (C) 2011, 2012 Patrick Crews, M.Sharan Kumar
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

from lib.util.mysql_methods import execute_query

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
        bot.logging.info("Executing sysbench:  %s" %(sysbench_cmd))
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
        bot.logging.debug(output)
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

def sysbench_db_analysis(dsn_string, test_data):
    """ Process the data from a sysbench run by
        INSERTing the data into the provided dsn_string / database
        and doing checks on performance regressions (against tps)

        test_data is a dictionary whose keys are the concurrencies
        used in the test and whose data is a list of dictionaries,
        one per iteration taken for said concurrency

    """

    # log our sysbench_run
    run_id = getNextRunId(dsn_string)
    config_id = getConfigId(dsn_string, test_data)
    log_sysbench_run( run_id
                    , config_id
                    , test_data['test_machine']
                    , "staging-%s" %test_data['test_server_revno']
                    , test_data['run_date']
                    , dsn_string
                    )
    test_concurrencies = [ i for i in test_data.keys() if type(i) is int ]
    test_concurrencies.sort()
    for concurrency in test_concurrencies:
        for iteration_data in test_data[concurrency]:
            log_sysbench_iteration(run_id, concurrency, iteration_data, dsn_string)
    msg_data = getSysbenchRegressionReport(run_id, test_data, dsn_string)
    return 0, msg_data

def log_sysbench_run(run_id, config_id, server_name, server_version, run_date, dsn_string):
    """Creates a new run record in the database for this run"""

    query = """INSERT INTO bench_runs (
               run_id
             , config_id
             , server
             , version
             , run_date
             ) VALUES (%d, %d, '%s', '%s', '%s')
           """ % ( run_id
                 , config_id
                 , server_name
                 , server_version
                 , run_date
                 )
    retcode, result= execute_query(query, dsn_string=dsn_string)
    return result


def log_sysbench_iteration(run_id, concurrency, iteration_data, dsn_string):
    # TODO: make sure we properly capture full commentary
    full_commentary = None
    # Write results to the DB
    query = """INSERT INTO sysbench_run_iterations (
          run_id
        , concurrency
        , iteration
        , tps
        , read_write_req_per_second
        , deadlocks_per_second
        , min_req_latency_ms
        , max_req_latency_ms
        , avg_req_latency_ms
        , 95p_req_latency_ms
        ) VALUES (%d, %d, %d, %0.2f, %0.2f, %0.2f, %0.2f, %0.2f, %0.2f, %0.2f) 
      """ % ( int(run_id)
            , int(concurrency)
            , int(iteration_data['iteration'])
            , iteration_data['tps']
            , iteration_data['rwreqps']
            , iteration_data['deadlocksps']
            , iteration_data['min_req_lat_ms']
            , iteration_data['max_req_lat_ms']
            , iteration_data['avg_req_lat_ms']
            , iteration_data['95p_req_lat_ms']
            )
    retcode, result= execute_query(query, dsn_string=dsn_string)
    return result

def getSysbenchRegressionReport(run_id, test_data, dsn_string, diff_check_data = None):
  """Returns a textual report of the regression over a series of runs"""

  # TODO: Allow for comparing one branch name vs. another...
  # add greater flexibility for working with such data
  bzr_branch = 'staging'
  report_notation = ''
  full_commentary = None

  (last_5_revs, last_20_revs)= get5and20RevisionRanges(run_id, bzr_branch, test_data, dsn_string)
  report_text= """====================================================================================================
SYSBENCH BENCHMARK REPORT %s
====================================================================================================
MACHINE:  %s
RUN ID:   %d
RUN DATE: %s
WORKLOAD: %s
SERVER:   %s
VERSION:  %s
REVISION: %d
COMMENT:  %s
====================================================================================================

TRENDING OVER LAST 5 runs %s
""" % (
    report_notation
  , test_data['test_machine']
  , run_id
  , test_data['run_date']
  , test_data['config_name'] 
  , test_data['test_server_type']
  , bzr_branch 
  , int(test_data['test_server_revno'])
  , test_data['test_server_comment']
  , report_notation
)

  report_text= report_text + "%-6s %-7s %-17s %-10s %-10s %-10s %-10s %-10s" % ("Conc","TPS","% Diff from Avg","Diff","Min","Max","Avg","STD")
  report_text= report_text + """
====================================================================================================
"""
  if len(last_5_revs) > 0:
    results= getRegressionData(run_id, last_5_revs, dsn_string)
    for result in results:
      report_text= report_text + "%-6s %6s %12s %10s %10s %10s %10s %10s\n" % tuple(result)
    report_text= report_text + """====================================================================================================

TRENDING OVER Last 20 runs %s

""" % (
    report_notation
)

  report_text= report_text + "%-6s %-7s %-17s %-10s %-10s %-10s %-10s %-10s" % ("Conc","TPS","% Diff from Avg","Diff","Min","Max","Avg","STD")
  report_text= report_text + """
====================================================================================================
"""
  if len(last_20_revs) > 0:
    results= getRegressionData(run_id, last_20_revs, dsn_string)
    for result in results:
      report_text= report_text + "%-6s %6s %12s %10s %10s %10s %10s %10s\n" % tuple(result)

    report_text= report_text + """====================================================================================================

TRENDING OVER ALL runs %s

""" % (
    report_notation
)
    report_text= report_text + "%-6s %-7s %-17s %-10s %-10s %-10s %-10s %-10s" % ("Conc","TPS","% Diff from Avg","Diff","Min","Max","Avg","STD")
    report_text= report_text + """
====================================================================================================
"""

  results= getAllRegressionData(test_data['test_machine'], bzr_branch, run_id, dsn_string, test_data)
  for result in results:
    report_text= report_text + "%-6s %6s %12s %10s %10s %10s %10s %10s\n" % tuple(result)
  report_text= report_text + "===================================================================================================="

  if full_commentary:
    report_text= report_text + """
FULL REVISION COMMENTARY:

%s""" % full_commentary

  if diff_check_data:
      report_text += "ERROR: The following tests were flagged as performance regressions.\n"
      for datum in diff_check_data:
          report_text += "%s\n" %datum
  return report_text

def get5and20RevisionRanges(run_id, bzr_branch, test_data, dsn_string):
    """ Return a tuple with 2 ranges of run_id values for the last 5 and 20 runs
        Ported from drizzle-automation
        TODO:  Further refactor / eliminate this

    """

    query = """ SELECT 
                run_id
                FROM bench_config c
                NATURAL JOIN bench_runs r
                WHERE c.name = '%s'
                AND r.server = '%s'
                AND r.version LIKE '%s%%'
                AND r.run_id <= %d
                ORDER BY run_id DESC
                LIMIT 20
                """ % ( test_data['config_name']
                      , test_data['test_machine']
                      , bzr_branch
                      , run_id
                      )
    retcode, results = execute_query(query, dsn_string=dsn_string)
    results_data = []
    for result in results:
        cur_run_id= int(result[0])
        results_data.append(str(cur_run_id))
    last_5_revs = results_data[0:5]
    last_20_revs = results_data[0:20]
    return (last_5_revs, last_20_revs)

def getRegressionData(run_id, id_range, dsn_string):
    query= """ SELECT 
           i.concurrency
           , ROUND(AVG(i.tps), 2) AS tps
           , IF (AVG(i.tps) >= agg.avg_tps
           , CONCAT('+', ROUND(((AVG(i.tps) - agg.avg_tps) / agg.avg_tps) * 100, 2), '%%') 
           , CONCAT('-', ROUND(((agg.avg_tps - AVG(i.tps)) / agg.avg_tps) * 100, 2), '%%')
           ) as pct_diff_from_avg
           , ROUND((AVG(i.tps) - agg.avg_tps), 2) as diff_from_avg
           , ROUND(agg.min_tps, 2) AS min_tps
           , ROUND(agg.max_tps, 2) AS max_tps
           , ROUND(agg.avg_tps, 2) AS avg_tps
           , FORMAT(ROUND(agg.stddev_tps, 2),2) AS stddev_tps
           FROM bench_config c
           NATURAL JOIN bench_runs r
           NATURAL JOIN sysbench_run_iterations i
           INNER JOIN (
             SELECT
               concurrency
             , MIN(tps) as min_tps
             , MAX(tps) as max_tps
             , AVG(tps) as avg_tps
             , STDDEV(tps) as stddev_tps
             FROM sysbench_run_iterations iter
             WHERE run_id IN (%s) 
             GROUP BY concurrency
                 ) AS agg
                 ON i.concurrency = agg.concurrency
                 WHERE r.run_id = %d
                 GROUP BY i.concurrency
                 ORDER BY i.concurrency
                 """ %( ",".join(id_range)
                      , run_id)
    retcode, result= execute_query(query, dsn_string=dsn_string)
    return result          

def getAllRegressionData(server_name, bzr_branch, run_id, dsn_string, test_data):
    query = """ SELECT 
                i.concurrency
                , ROUND(AVG(i.tps), 2) AS tps
                , IF (AVG(i.tps) >= agg.avg_tps
                , CONCAT('+', ROUND(((AVG(i.tps) - agg.avg_tps) / agg.avg_tps) * 100, 2), '%%') 
                , CONCAT('-', ROUND(((agg.avg_tps - AVG(i.tps)) / agg.avg_tps) * 100, 2), '%%')
                ) as pct_diff_from_avg
                , ROUND((AVG(i.tps) - agg.avg_tps), 2) as diff_from_avg
                , ROUND(agg.min_tps, 2) AS min_tps
                , ROUND(agg.max_tps, 2) AS max_tps
                , ROUND(agg.avg_tps, 2) AS avg_tps
                , FORMAT(ROUND(agg.stddev_tps, 2),2) AS stddev_tps
                FROM bench_config c
                NATURAL JOIN bench_runs r
                NATURAL JOIN sysbench_run_iterations i
                INNER JOIN (
                SELECT
                    iter.concurrency
                  , MIN(tps) as min_tps
                  , MAX(tps) as max_tps
                  , AVG(tps) as avg_tps
                  , STDDEV(tps) as stddev_tps
                  FROM bench_config conf
                  NATURAL JOIN bench_runs runs
                  NATURAL JOIN sysbench_run_iterations iter
                  WHERE conf.name = '%s'
                  AND runs.server = '%s'
                  AND runs.version LIKE '%s%%'
                  GROUP BY iter.concurrency
                ) AS agg
                  ON i.concurrency = agg.concurrency
                WHERE r.run_id = %d
                GROUP BY i.concurrency
                ORDER BY i.concurrency
                      """ % ( test_data['config_name']
                            , server_name
                            , bzr_branch
                            , run_id
                            )

    retcode, result= execute_query(query, dsn_string=dsn_string)
    return result

def getConfigId(dsn_string, test_data):
  """Returns the integer ID of the configuration name used in this run."""

  # If we have not already done so, we query the local DB for the ID
  # matching this sqlbench config name.  If none is there, we insert
  # a new record in the bench_config table and return the newly generated
  # identifier.
  benchmark_name = test_data['config_name'] 
  query = "SELECT config_id FROM bench_config WHERE name = '%s'" %benchmark_name
  retcode, result= execute_query(query, dsn_string=dsn_string)
  if len(result) == 0:
      # Insert a new record for this config and return the new ID...
      query = "INSERT INTO bench_config (config_id, name) VALUES (NULL, '%s')" %benchmark_name
      retcode, result= execute_query(query, dsn_string=dsn_string)
      return getConfigId(dsn_string, test_data)
  else:
      config_id= int(result[0][0])
  return config_id

def getNextRunId(dsn_string):
  """Returns a new run identifier from the database.  
     The run ID is used in logging the results of the run iterations.

  """

  query = "SELECT MAX(run_id) as new_run_id FROM bench_runs"
  retcode, result= execute_query(query, dsn_string=dsn_string)
  if result[0][0] >= 1:
      new_run_id= int(result[0][0]) + 1
  else:
      new_run_id= 1
  return new_run_id


def getSysbenchReport(run,fetch):
    """returns the report of the last sysbench test executed"""

    report="""==============================
  SYSBENCH REGRESSION REPORT
==============================
CONCURRENCY    :  %d
ITERATIONS     :  %d
MODE           :  %s
TPS            :  %f      %f
MIN_REQ_LAT_MS :  %f      %f
MAX_REQ_LAT_MS :  %f      %f
AVG_REQ_LAT_MS :  %f      %f
95P_REQ_LAT_MS :  %f      %f
RWREQPS        :  %f      %f
DEADLOCKSPS    :  %f      %f
=============================
          """ % (fetch['concurrency'],
                 fetch['iteration']+1,
                 iteration_data['mode'],
                 iteration_data['tps'],           iteration_data['tps']-fetch['tps'],
                 iteration_data['min_req_lat_ms'],iteration_data['min_req_lat_ms']-fetch['min_req_lat_ms'],
                 iteration_data['max_req_lat_ms'],iteration_data['max_req_lat_ms']-fetch['max_req_lat_ms'],
                 iteration_data['avg_req_lat_ms'],iteration_data['avg_req_lat_ms']-fetch['avg_req_lat_ms'],
                 iteration_data['95p_req_lat_ms'],iteration_data['95p_req_lat_ms']-fetch['95p_req_lat_ms'],
                 iteration_data['rwreqps'],       iteration_data['rwreqps']-fetch['rwreqps'],
                 iteration_data['deadlocksps'],   iteration_data['deadlocksps']-fetch['deadlocksps']
                )
    return report     
        
