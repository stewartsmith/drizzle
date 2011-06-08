**********************************
sql-bench
**********************************



Description
===========
dbqp's sql-bench mode allows a user to run the sql-bench testing suite. 


Requirements
============
DBD::drizzle
-------------
The DBD::drizzle module is required it can be found here http://launchpad.net/dbd-drizzle/

Additional information for installing the module::

    Prerequisites
    ----------------
    * Perl
    * Drizzle (bzr branch lp:drizzle)
    * libdrizzle (bzr branch lp:libdrizzle)
    * C compiler

    Installation
    -------------
    You should only have to run the following:

    perl Makefile.PL --cflags=-I/usr/local/drizzle/include/ --libs=-"L/usr/local/drizzle/lib -ldrizzle"


    Depending on where libdrizzle is installed. Also, you'll want to make 
    sure that ldconfig has configured libdrizzle to be in your library path 


sql-bench / dbqp tests
=====================

Currently, there are only two sql-bench test cases for dbqp.  As one might expect, main.all_sqlbench_tests executes::

    run-all-tests --server=drizzle --dir=$DRIZZLE_TEST_WORKDIR --log --connect-options=port=$MASTER_MYPORT --create-options=ENGINE=innodb --user=root 

against a Drizzle server.  The second test case executes the crashme tool against a running server.

Test cases are defined in python .cnf files and live in tests/sqlbench_tests.

Running tests
=========================

NOTE:  all_sqlbench_tests can take a significant amount of time to execute (45 minutes or so on a decently provisioned laptop)

There are several different ways to run tests using :doc:`dbqp` 's sql-bench mode.

It should be noted that unless :option:`--force` is used, the program will
stop execution upon encountering the first failing test. 
:option:`--force` is recommended if you are running several tests - it will
allow you to view all successes and failures in one run.

Running individual tests
------------------------
If one only wants to run a few, specific tests, they may do so this way::

    ./dbqp --mode=sql-bench [OPTIONS] test1 [test2 ... testN]

Running all tests within a suite
--------------------------------
Many of the tests supplied with Drizzle are organized into suites.  

The tests within drizzle/tests/randgen_tests/main are considered the 'main' suite.  
Other suites are also subdirectories of drizzle/tests/randgen_tests.

To run the tests in a specific suite::

    ./dbqp --mode=sql-bench [OPTIONS] --suite=SUITENAME

Running specific tests within a suite
--------------------------------------
To run a specific set of tests within a suite::

    ./dbqp --mode=sql-bench [OPTIONS] --suite=SUITENAME TEST1 [TEST2..TESTN]

Calling tests using <suitename>.<testname> currently does not work.
One must specify the test suite via the :option:`--suite` option.


Running all available tests
---------------------------
One would currently have to name all suites, but the majority of the working tests live in the main suite
Other suites utilize more exotic server combinations and we are currently tweaking them to better integrate with the 
dbqp system.  The slave-plugin suite does currently have a good config file for setting up simple replication setups for testing.
To execute several suites' worth of tests::

    ./dbqp --mode=sql-bench [OPTIONS] --suite=SUITE1, SUITE2, ...SUITEN

Interpreting test results
=========================
The output of the test runner is quite simple.  Every test should pass.
In the event of a test failure, please take the time to file a bug here:
*https://bugs.launchpad.net/drizzle*

During a run, the program will provide the user with:
  * test name (suite + name)
  * test status (pass/fail/skipped)
  * time spent executing each test

Example output::

    20110601-191706  ===============================================================
    20110601-191706  TEST NAME                                  [ RESULT ] TIME (ms)
    20110601-191706  ===============================================================
    20110601-191706  readonly.concurrency_16                    [ pass ]   240019
    20110601-191706  max_req_lat_ms: 21.44
    20110601-191706  rwreqps: 4208.2
    20110601-191706  min_req_lat_ms: 6.31
    20110601-191706  deadlocksps: 0.0
    20110601-191706  tps: 150.29
    20110601-191706  avg_req_lat_ms: 6.65
    20110601-191706  95p_req_lat_ms: 7.02
    20110601-191706  ===============================================================
    20110601-191706 INFO Test execution complete in 275 seconds
    20110601-191706 INFO Summary report:
    20110601-191706 INFO Executed 1/1 test cases, 100.00 percent
    20110601-191706 INFO STATUS: PASS, 1/1 test cases, 100.00 percent executed
    20110601-191706 INFO Spent 240 / 275 seconds on: TEST(s)
    20110601-191706 INFO Test execution complete
    20110601-191706 INFO Stopping all running servers...

