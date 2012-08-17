**********************************
sysbench
**********************************



Description
===========
kewpie's sysbench mode allows a user to run a specific iteration of a sysbench test (eg an oltp readonly run at concurrency = 16)


Requirements
============

The SYSBENCH command requires that the Drizzle library header be installed. Simply build Drizzle and do::

    $> sudo make install

This will install the right headers.

The SYSBENCH command also requires installation of the drizzle-sysbench program, which can be installed from source like so::

    $> bzr branch lp:~drizzle-developers/sysbench/trunk  drizzle-sysbench
    $> cd drizzle-sysbench
    $> ./autogen.sh && ./configure && make && sudo make install

Make sure sysbench is in your path


sysbench / kewpie tests
=======================

The sysbench test suite consists of python unittests that encapsulate / automate sysbench oltp runs for readonly and readwrite.

The tests are written in Python and are rather straightforward to modify.

Changing server options:

Altering concurrencies tested:


Altering iterations:
By default, the tests will execute 3 iterations of each concurrency tested.  This is to help achieve some consistency for performance analysis. 

Running tests
=============

There are several different ways to run tests using :doc:`kewpie` 's sysbench mode.

It should be noted that unless :option:`kewpie.py --force` is used, the program
will stop execution upon encountering the first failing test.
:option:`kewpie.py --force` is recommended if you are running several tests
- it will allow you to view all successes and failures in one run.

Running tests
------------------------
If one only wants to run a few, specific tests, they may do so this way::

    ./kewpie --suite=sysbench [OPTIONS] sysbench_readonly_test | sysbench_readwrite_test 

Calling tests using <suitename>.<testname> currently does not work.  One must
specify the test suite via the :option:`kewpie.py --suite` option.

Results database
------------------
The Drizzle team has ported drizzle-automation functionality for sysbench.
This means that a user can run a Drizzle / MySQL database and use it to
store sysbench run data and to compare runs against historic data.

The SQL to create the required tables are in tests/std_data/sysbench_db.sql

This will create 3 tables, bench_config, bench_runs, and sysbench_run_iterations
This emulates drizzle-automation's historic behavior.

Once the tables have been created, add the following option to the kewpie call:
--results-db-dsn='host_ip:user:user_pass:drizzle_stats:port'
::

====================================================================================================
SYSBENCH BENCHMARK REPORT 
====================================================================================================
MACHINE:  erlking
RUN ID:   19
RUN DATE: 2012-08-15T18:16:12.596937
WORKLOAD: sysbench
SERVER:   drizzle
VERSION:  staging
REVISION: 2595
COMMENT:  2595: Ported this reporting capability! 
====================================================================================================

TRENDING OVER LAST 5 runs 
Conc   TPS     % Diff from Avg   Diff       Min        Max        Avg        STD       
====================================================================================================
128    218.72       +0.58%       1.26     192.64     232.90     217.46      13.02
256    223.71       +6.85%      14.34     192.28     227.73     209.37      14.07
512    223.40       +7.47%      15.53     192.70     228.93     207.87      12.76
====================================================================================================

TRENDING OVER Last 20 runs 

Conc   TPS     % Diff from Avg   Diff       Min        Max        Avg        STD       
====================================================================================================
128    218.72       +2.95%       6.26     188.42     232.90     212.45      15.27
256    223.71       +7.92%      16.42     189.18     232.99     207.29      14.94
512    223.40       +8.46%      17.42     191.35     232.79     205.98      14.09
====================================================================================================

TRENDING OVER ALL runs 

Conc   TPS     % Diff from Avg   Diff       Min        Max        Avg        STD       
====================================================================================================
128    218.72       +2.95%       6.26     188.42     232.90     212.45      15.27
256    223.71       +7.92%      16.42     189.18     232.99     207.29      14.94
512    223.40       +8.46%      17.42     191.35     232.79     205.98      14.09
====================================================================================================
20120815-184028  sysbench.sysbench_readonly_test            [ pass ]  1455879
20120815-184028  ===============================================================


