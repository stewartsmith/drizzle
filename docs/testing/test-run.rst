.. _test-run-label:

**********************************
test-run - Drizzle testing tool
**********************************

Synopsis
========

**./test-run** [ *OPTIONS* ] [ TESTCASE ]

Description
===========

:program:`test-run` (aka test-run, dtr, mtr) is used to execute tests
from the Drizzle/MySQL test suite.  These tests are included with their respective 
distributions and provide a way for users to verify that the system will
operate according to expectations.

The tests use a diff-based paradigm, meaning that the test runner executes
a test and then compares the results received with pre-recorded expected 
results.  In the event of a test failure, the program will provide output
highlighting the differences found between expected and actual results; this
can be useful for troubleshooting and in bug reports.

While most users are concerned with ensuring general functionality, the 
program also allows a user to quickly spin up a server for ad-hoc testing
and to run the test-suite against an already running test server.

Running tests
=========================

There are several different ways to run tests using :program:`test-run`.

It should be noted that unless :option:`test-run --force` is used, the program will
stop execution upon encountering the first failing test. 
:option:`test-run --force` is recommended if you are running several tests - it will
allow you to view all successes and failures in one run.

Running individual tests
------------------------
If one only wants to run a few, specific tests, they may do so this way::

    ./test-run [OPTIONS] test1 [test2 ... testN]

Running all tests within a suite
--------------------------------
Many of the tests supplied with MySQL-based servers are organized into suites.  

The tests within drizzle/tests/t are considered the 'main' suite.  
Other suites are located in either drizzle/tests/suite or within the various
directories in drizzle/plugin.  Tests for a specific plugin should live in 
the plugin's directory - drizzle/plugin/example_plugin/tests

To run the tests in a specific suite::

    ./test-run [OPTIONS] --suite=SUITENAME

Running specific tests within a suite
--------------------------------------
To run a specific set of tests within a suite::

    ./test-run [OPTIONS] --suite=SUITENAME TEST1 [TEST2..TESTN]

Calling tests using <suitename>.<testname> currently does not work.  One must
specify the test suite via the :option:`test-run --suite` option.


Running all available tests
---------------------------
Currently, the quickest way to execute all tests in all suites is
to use 'make test' from the drizzle root.

Otherwise, one should simply name all suites::

    ./test-run [OPTIONS] --suite=SUITE1, SUITE2, ...SUITEN

Interpreting test results
=========================
The output of the test runner is quite simple.  Every test should pass.
In the event of a test failure, please take the time to file a bug here:
*https://bugs.launchpad.net/drizzle*

During a run, the program will provide the user with:
  * test name (suite + name)
  * test status (pass/fail/skipped)
  * time spent executing each test

At the end of a run, the program will provide the user with a listing of:
  * how many tests were run
  * how many tests failed
  * percentage of passing tests
  * a listing of failing tests
  * total time spent executing the tests

Example output::

    <snip>
    main.snowman                                                 [ pass ]       9
    main.statement_boundaries                                    [ pass ]      17
    main.status                                                  [ pass ]      12
    main.strict                                                  [ pass ]      50
    main.subselect                                               [ pass ]    6778
    main.subselect2                                              [ pass ]      51
    main.subselect3                                              [ fail ]
    drizzletest: At line 621: query 'select a, (select max(b) from t1) into outfile
    <snip>
    --------------------------------------------------------------------------------
    Stopping All Servers
    Failed 10/231 tests, 95.67% were successful.

    The log files in var/log may give you some hint
    of what went wrong.
    If you want to report this error, go to:
	http://bugs.launchpad.net/drizzle
    The servers were restarted 16 times
    Spent 64.364 of 115 seconds executing testcases

    drizzle-test-run in default mode: *** Failing the test(s): main.exp1 
    main.func_str main.loaddata main.null main.outfile main.subselect3 
    main.warnings jp.like_utf8 jp.select_utf8 jp.where_utf8
    
Additional uses
===============
Starting a server for manual testing
------------------------------------

:program:`test-run` allows a user to get a Drizzle server up and running
quickly.  This can be useful for fast ad-hoc testing.

To do so call::

    ./test-run --start-and-exit [*OPTIONS*]

This will start a Drizzle server that you can connect to and query

Starting a server against a pre-populated DATADIR
--------------------------------------------------

Using :option:`test-run --start-dirty` prevents :program:`test-run` from
attempting to initialize (clean) the datadir.  This can be useful if you want
to use an already-populated datadir for testing.

Program architecture
====================

:program:`test-run` uses a simple diff-based mechanism for testing.  
It will execute the statements contained in a test and compare the results 
to pre-recorded expected results.  In the event of a test failure, you
will be presented with a diff::

    main.exp1                                                    [ fail ]
    --- drizzle/tests/r/exp1.result	2010-11-02 02:10:25.107013998 +0300
    +++ drizzle/tests/r/exp1.reject	2010-11-02 02:10:32.017013999 +0300
    @@ -5,4 +5,5 @@
    a
    1
    2
    +3
    DROP TABLE t1;

A test case consists of a .test and a .result file.  The .test file includes
the various statements to be executed for a test.  The .result file lists
the expected results for a given test file.  These files live in tests/t 
and tests/r, respectively.  This structure is the same for all test suites.

test-run options
===================

The :program:`test-run` tool has several available options:

./test-run [ OPTIONS ] [ TESTCASE ]

Options to control what engine/variation to run
-----------------------------------------------

.. program:: test-run

.. option:: --compress
   
   Use the compressed protocol between client and server

.. program:: test-run

.. option:: --bench
   
   Run the benchmark suite

.. program:: test-run

.. option:: --small-bench

   Run the benchmarks with --small-tests --small-tables

Options to control directories to use
-------------------------------------

.. program:: test-run

.. option:: --benchdir=DIR          

   The directory where the benchmark suite is stored
   (default: ../../mysql-bench)
  
.. program:: test-run

.. option:: --tmpdir=DIR

   The directory where temporary files are stored
   (default: ./var/tmp).

.. program:: test-run

.. option:: --vardir=DIR  
         
   The directory where files generated from the test run
   is stored (default: ./var). Specifying a ramdisk or
   tmpfs will speed up tests.

.. program:: test-run

.. option:: --mem 
   
   Run testsuite in "memory" using tmpfs or ramdisk
   Attempts to find a suitable location
   using a builtin list of standard locations
   for tmpfs (/dev/shm)
   The option can also be set using environment
   variable :envvar:`DTR_MEM` =[DIR]

Options to control what test suites or cases to run
---------------------------------------------------

.. program:: test-run

.. option:: --force                 
   
   Continue to run the suite after failure

.. program:: test-run

.. option:: --do-test=PREFIX or REGEX
                        
   Run test cases which name are prefixed with PREFIX
   or fulfills REGEX

.. program:: test-run

.. option:: --skip-test=PREFIX or REGEX
                        
   Skip test cases which name are prefixed with PREFIX
   or fulfills REGEX

.. program:: test-run

.. option:: --start-from=PREFIX     

   Run test cases starting from test prefixed with PREFIX
   suite[s]=NAME1,..,NAMEN Collect tests in suites from the comma separated
   list of suite names.
   The default is: "main,jp"

.. program:: test-run

.. option:: --skip-rpl              

   Skip the replication test cases.
   combination="ARG1 .. ARG2" Specify a set of "drizzled" arguments for one
   combination.

.. program:: test-run

.. option:: --skip-combination      

   Skip any combination options and combinations files

.. program:: test-run

.. option:: --repeat-test=n         
  
   How many times to repeat each test (default: 1)

Options that specify ports
--------------------------

.. program:: test-run

.. option:: --master_port=PORT      

   Specify the port number used by the first master

.. program:: test-run

.. option:: --slave_port=PORT      

   Specify the port number used by the first slave

.. program:: test-run

.. option:: --dtr-build-thread=#    

   Specify unique collection of ports. Can also be set by
   setting the environment variable :envvar:`DTR_BUILD_THREAD`.

Options for test case authoring
-------------------------------

.. program:: test-run

.. option:: --record TESTNAME       

   (Re)genereate the result file for TESTNAME

.. program:: test-run

.. option:: --check-testcases       

   Check testcases for sideeffects

.. program:: test-run

.. option:: --mark-progress         

   Log line number and elapsed time to <testname>.progress

Options that pass on options
----------------------------

.. program:: test-run

.. option:: --drizzled=ARGS           
 
   Specify additional arguments to "drizzled"

Options to run test on running server
-------------------------------------

.. program:: test-run

.. option:: --extern                

   Use running server for tests

.. program:: test-run

.. option:: --user=USER             

   User for connection to extern server

Options for debugging the product
---------------------------------

.. program:: test-run

.. option:: --client-ddd            

   Start drizzletest client in ddd

.. program:: test-run

.. option:: --client-debugger=NAME  

   Start drizzletest in the selected debugger

.. program:: test-run

.. option:: --client-gdb            

   Start drizzletest client in gdb

.. program:: test-run

.. option:: --ddd                   

   Start drizzled in ddd

.. program:: test-run

.. option:: --debug                 

   Dump trace output for all servers and client programs

.. program:: test-run

.. option:: --debugger=NAME         

   Start drizzled in the selected debugger

.. program:: test-run

.. option:: --gdb                   

   Start the drizzled(s) in gdb

.. program:: test-run

.. option:: --manual-debug          

   Let user manually start drizzled in debugger, before running test(s)

.. program:: test-run

.. option:: --manual-gdb            

   Let user manually start drizzled in gdb, before running test(s)

.. program:: test-run

.. option:: --manual-ddd            

   Let user manually start drizzled in ddd, before running test(s)

.. program:: test-run

.. option:: --master-binary=PATH    
   
   Specify the master "drizzled" to use

.. program:: test-run

.. option:: --slave-binary=PATH     

   Specify the slave "drizzled" to use

.. program:: test-run

.. option:: --strace-client         

   Create strace output for drizzletest client

.. program:: test-run

.. option:: --max-save-core         

   Limit the number of core files saved (to avoid filling up disks for 
   heavily crashing server). Defaults to 5, set to 0 for no limit.

Options for coverage, profiling etc
-----------------------------------

.. todo::
   
   .. option:: --gcov                  

.. program:: test-run

.. option:: --gprof                 

   See online documentation on how to use it.

.. program:: test-run

.. option:: --valgrind              

   Run the *drizzletest* and *drizzled* executables using valgrind with 
   default options

.. program:: test-run

.. option:: --valgrind-all          
   
   Synonym for :option:`--valgrind`

.. program:: test-run

.. option:: --valgrind-drizzleslap  

   Run "drizzleslap" with valgrind.

.. program:: test-run

.. option:: --valgrind-drizzletest  

   Run the *drizzletest* and *drizzle_client_test* executable with valgrind

.. program:: test-run

.. option:: --valgrind-drizzled       

   Run the "drizzled" executable with valgrind

.. program:: test-run

.. option:: --valgrind-options=ARGS 

   Deprecated, use :option:`--valgrind-option`

.. program:: test-run

.. option:: --valgrind-option=ARGS  

   Option to give valgrind, replaces default option(s), 
   can be specified more then once

.. program:: test-run

.. option:: --valgrind-path=[EXE]   

   Path to the valgrind executable

.. program:: test-run

.. option:: --callgrind             

   Instruct valgrind to use callgrind

.. program:: test-run

.. option:: --massif                

   Instruct valgrind to use massif

Misc options
------------

.. program:: test-run

.. option:: --comment=STR           

   Write STR to the output

.. program:: test-run

.. option:: --notimer               

   Don't show test case execution time

.. program:: test-run

.. option:: --script-debug          

   Debug this script itself

.. program:: test-run

.. option:: --verbose               

   More verbose output

.. program:: test-run

.. option:: --start-and-exit        

   Only initialize and start the servers, using the
   startup settings for the specified test case (if any)

.. program:: test-run

.. option:: --start-dirty           

   Only start the servers (without initialization) for
   the specified test case (if any)

.. program:: test-run

.. option:: --fast                  

   Don't try to clean up from earlier runs

.. program:: test-run

.. option:: --reorder               

   Reorder tests to get fewer server restarts

.. program:: test-run

.. option:: --help                  

   Get this help text

.. program:: test-run

.. option:: --testcase-timeout=MINUTES 

   Max test case run time (default 15)

.. program:: test-run

.. option:: --suite

   Test suite to run.

.. program:: test-run

.. option:: --suite-timeout=MINUTES 

   Max test suite run time (default 180)

.. program:: test-run

.. option:: --warnings | log-warnings 

   Pass --log-warnings to drizzled

.. program:: test-run

.. option:: --sleep=SECONDS         

   Passed to drizzletest, will be used as fixed sleep time

.. program:: test-run

.. envvar:: DTR_MEM

   Same as :option:`test-run --mem`

.. program:: test-run

.. envvar:: DTR_BUILD_THREAD

   Same as :option:`test-run --dtr-build-thread`

