Drizzletest Commands
====================

The commands that are endorsed in Drizzletest are delineated in the following documentation. Examples are given for the commands. Browse **tests/t** for more examples. 

.. note:: 
          The commands are not case sensitive.
          All commands must end with semi-colon.

List of commands
----------------

* :ref:`append_file`
* :ref:`cat_file`
* :ref:`change_user`
* :ref:`character_set`
* :ref:`chmod`
* :ref:`connect`
* :ref:`connection`
* :ref:`copy_file`
* :ref:`dec`
* :ref:`delimiter`
* :ref:`die`
* :ref:`diff_files`
* :ref:`dirty_close`
* :ref:`disable/enable_abort_on_error`
* :ref:`disable/enable_connect_log`
* :ref:`disable/enable_info`
* :ref:`disable/enable_metadata`
* :ref:`disable/enable_parsing`
* :ref:`disable/enable_ps_protocol`
* :ref:`disable/enable_query_log`
* :ref:`disable/enable_reconnect`
* :ref:`disable/enable_result_log`
* :ref:`disable/enable_rpl_parse`
* :ref:`disable/enable_warnings`
* :ref:`disconnect`
* :ref:`echo`
* :ref:`end`
* :ref:`end_timer`
* :ref:`error`
* :ref:`eval`
* :ref:`exec`
* :ref:`exit`
* :ref:`file_exists`
* :ref:`horizontal_results`
* :ref:`if`
* :ref:`inc`
* :ref:`let`
* :ref:`mkdir`
* :ref:`list_files`
* :ref:`list_files_append_file`
* :ref:`list_files_write_file`
* :ref:`lowercase_result`
* :ref:`move_file`
* :ref:`perl`
* :ref:`ping`
* :ref:`query`
* :ref:`query_get_value`
* :ref:`query_horizontal`
* :ref:`query_vertical`
* :ref:`real_sleep`
* :ref:`reap`
* :ref:`remove_file`
* :ref:`remove_files_wildcard`
* :ref:`replace_column`
* :ref:`replace_regex`
* :ref:`replace_result`
* :ref:`require`
* :ref:`result`
* :ref:`rmdir`
* :ref:`save_master_pos`
* :ref:`send`
* :ref:`send_eval`
* :ref:`send_quit`
* :ref:`shutdown_server`
* :ref:`skip`
* :ref:`sleep`
* :ref:`sorted_result`
* :ref:`source`
* :ref:`start_timer`
* :ref:`sync_slave_with_master`
* :ref:`sync_with_master`
* :ref:`system`
* :ref:`vertical_results`
* :ref:`wait_for_slave_to_stop`
* :ref:`while`
* :ref:`write_file`
   
.. _append_file:

append_file
-----------
:Syntax: 

:program:`append_file file_name [terminator]`

:program:`append_file` command is used to append / add data to the end of an existing file. It is similar to :ref:`write_file`. In case, the specified file does not exist, it is created and the data is written on it. The end of data, that is to be appended, is marked by the terminator. 

.. note:: The default terminator is EOF

The ``file_name`` can be substituted via variables.
   
:Example:

:: 
  
    let $MY_FILE = ~/foo/bar.txt;

    append_file $MY_FILE;
    writing text...
    EOF

    append_file $MY_FILE;
    appending text with default terminator...
    EOF

    append_file $MY_FILE stop
    appending text with `stop` terminator...
    stop

:Output:

::
    
    ~/foo/bar.txt:
    writing text...
    appending text with default terminator...
    appending text with `stop` terminator...


.. _cat_file:

cat_file
--------

:Syntax: 

:program:`cat_file file_name`

:program:`cat_file` is similar to the unix ``cat`` command. cat_file expects only one argument. The ``cat_file`` command reads the file given as its argument and writes its contents to the `test_name`.result file. 
   
.. note:: If extra argument is passed to cat_file command, the following error is displayed. testname: At line N: Extra argument '/path/to/file/file_name' passed to 'cat_file'

:Example:

::

    /foo/log.txt:
    The test produced the following results:

    /tests/t/test_name.test:
    let $LOG_RESULT = /foo;
    cat_file $LOG_RESULT/log.txt
    SELECT 1;

:Output:

::

    /tests/r/test_name.result:
    The test produced the following results:
    SELECT 1;
    1
    1

.. note:: The file_name can be specified via variables. In the example above, we have used LOG_RESULT as variable. We can also specify it as "let $LOG_RESULT = /foo/log.txt" and use it as "cat_file $LOG_RESULT".

.. _change_user:

change_user
-----------

:Syntax: 

:program:`change_user [user_name], [password], [db_name]`



:Example:

.. code-block:: python

.. _character_set:

character_set
-------------

:Syntax: 

:program:`character_set charset_name`

:Example:

.. code-block:: python

.. _chmod:

chmod
-----

:Syntax: 

:program:`chmod octal_mode file_name`

:Example:

.. code-block:: python

.. _connect:

connect
-------

:Syntax: 

:program:`connect (name, host_name, user_name, password, db_name [,port_num [,socket [,options [,default_auth]]]])`

:Example:

.. code-block:: python

.. _connection:

connection
----------

:Syntax:

:program:`connection connection_name`

:Example:

.. code-block:: python

.. _copy_file:

copy_file
---------

:Syntax:

:program:`copy_file from_file to_file`

:Example:

.. code-block:: python

.. _dec:

dec
---

:Syntax:

:program:`dec $variable_name`

:program:`dec` takes in exactly one argument. The argument should be a ``variable``. The ``dec`` decrements the value of the variable by 1. This command takes two forms. :program:`dec $variable_name;` and :program:`- -dec $variable_name`. This command is the reverse of :ref:`inc` 

.. note:: If a constant is given as argument, the following error is thrown. ERROR:The argument to dec must be a variable (start with $)

.. note:: If two arguments are given, the following error is thrown ERROR:End of line junk detected

:Example:

::

    /tests/t/testname.test:
    let $foo = 5;
    echo $foo;
    dec $foo;
    echo $foo;

:Output:

::

    /tests/r/testname.result:
    5
    4

.. note:: If a string is stored in the variable, then it is not considered as an error. Decrementing such a variable will store -1 in the variable. 

:Example:

::
  
    /tests/t/testname.test:
    let $foo = 5;
    echo $foo;
    --dec $foo
    echo $foo;

:Output:

::

    /tests/r/testname.result:
    5
    4

.. note:: In the second form, the ``;`` is not required as a delimiter.
    
.. _delimiter:

delimiter
---------

:Syntax:

:program:`delimiter string`

:program:`delimiter` is used to change the default delimiter ``;`` to the one specified by the argument ``string``. The default delimiter is ``;`` (semi-colon). This command takes two forms. :program:`delimiter string` and :program:`- -delimiter string`. 

.. note:: The string argument can have space in between. In such cases, the entire string (with the space) should be used as delimiter 

:Example:

::

   /tests/t/testname.test:
   SELECT 1;
   delimiter stop;
   SELECT 1 stop

:Output:

::

   /tests/r/testname.result:
   SELECT 1;
   1
   1
   SELECT 1 stop
   1
   1

.. note:: The strings are case sensitive. For example, if the delimiter is set to ``stop``, then ``Stop`` or ``STOP`` cannot be considered as delimiter. 

:Example:

::

   tests/t/testname.test:
   SELECT 1;
   --delimiter END OF LINE
   SELECT 1 END OF LINE

:Output:

::

   tests/r/testname.result:
   SELECT 1;
   1
   1
   SELECT 1 END OF LINE
   1
   1

.. note:: In the above example, note the usage of ``--delimiter`` form. Also note the string with spaces in them.

When a string is set as delimiter, make sure that, the string is not used anywhere else. It should be unique. A common mistake that can be left unnoticed is given in the following example.

:Example:

::

   tests/t/testname.test:
   CREATE TABLE test (id INT, start FLOAT, end FLOAT);
   INSERT INTO test VALUES (1,10,12);
   delimiter end;
   SELECT start,end FROM test;

.. note:: We get the following error. At line 4: query 'select start,' failed: 1064: You have an error in your SQL syntax;

This test seems to be correct. However not that, the ``end`` in line 4 is treated as delimiter, and not as a field.

.. note:: To set the delimiter again to another one, ``delimiter new_delimiter`` should be followed by the old_delimiter

.. _die:

die
---

:Syntax:

:program:`die [message]`

:program:`die` is used to terminate the test. This command takes in a ``message`` ( string ) as argument. When this line is executed, the test fails, and the message is printed as the reason for aborting the test. This is similar to :ref:`exit`.

:Example:

::

   tests/t/testname.test:
   let $i=3;
   while($i)
   {
      die INFINITE LOOP ENCONTERED;
   }
   
:Output:

::

   ================================================================================
   DEFAULT STORAGE ENGINE: innodb
   TEST                                                         RESULT    TIME (ms)
   --------------------------------------------------------------------------------

   main.testname                                               [ fail ]
   testname: At line 4: INFINITE LOOP ENCOUNTERED

.. note:: This is often used within a conditional statement such as ``if``. That is, if a particular condition is reached, and the test will here after produce a fail result, then there is no need to carry out the remaining tests. Hence a die statement with the appropriate message can be used.

.. _diff_files:

diff_files
----------

:Syntax:

:program:`diff_files file_name1 file_name2`

:Example:

.. code-block:: python

.. _dirty_close:

dirty_close
-----------

:Syntax:

:program:`dirty_close connection_name`

.. _disable/enable_abort_on_error:

disable/enable_abort_on_error
-----------------------------

:Syntax:

:program:`disable_abort_on_error,enable_abort_on_error`

:Example:

.. code-block:: python

.. _disable/enable_connect_log:

disable/enable_connect_log
--------------------------

:Syntax:

:program:`disable_connect_log, enable_connect_log`

:Example:

.. code-block:: python

.. _disable/enable_info:

disable/enable_info
-------------------

:Syntax:

:program:`disable_info, enable_info`

:Example:

.. code-block:: python

.. _disable/enable_metadata:

disable/enable_metadata
-----------------------

:Syntax:

:program:`disable_metadata, enable_metadata`

:Example:

.. code-block:: python

.. _disable/enable_parsing:

disable/enable_parsing
----------------------

:Syntax:

:program:`disable_parsing, enable_parsing`

:Example:

.. code-block:: python

.. _disable/enable_ps_protocol:

disable/enable_ps_protocol
--------------------------

:Syntax:

:program:`disable_ps_protocol, enable_ps_protocol`

:Example:

.. code-block:: python

.. _disable/enable_query_log:

disable/enable_query_log
------------------------

:Syntax:

:program:`disable_query_log, enable_query_log`

:Example:

.. code-block:: python

.. _disable/enable_reconnect:

disable/enable_reconnect
------------------------

:Syntax:

:program:`disable_reconnect, enable_reconnect`

:Example:

.. code-block:: python

.. _disable/enable_result_log:

disable/enable_result_log
-------------------------

:Syntax:

:program:`disable_result_log, enable_result_log`

:Example:

.. code-block:: python

.. _disable/enable_rpl_parse:

disable/enable_rpl_parse
------------------------

:Syntax:

:program:`disable_rpl_parse, enable_rpl_parse`

:Example:

.. code-block:: python

.. _disable/enable_warnings:

disable/enable_warnings
-----------------------

:Syntax:

:program:`disable_warnings, enable_warnings`

:Example:

.. code-block:: python

.. _disconnect:

disconnect
----------

:Syntax:

:program:`disconnect connection_name`

:Example:

.. code-block:: python

.. _echo:

echo
----

:Syntax:

:program:`echo text`

:program:`echo` is used to display ``text`` in the test.result file. This is often used for giving a verbose explanation about the test in the test.result file. 

.. note:: If no text is provided, then a blank line is printed in the test.result file.

A good test file should echo all the important comments, so that, they are displayed into the test.result file for more clarity to the readers

:Example:

::

   tests/t/testname.test:
   echo testing select statement...
   --echo #test1
   SELECT 1;
   --echo #test2
   SELECT 2;

:Output:

::
  
   test started...
   #test1
   SELECT 1;
   1
   1
   #test2
   SELECT 2;
   2
   2

In the above example, we can see that, comments ``test1`` and ``test2`` are echoed into the testname.result file. This gives a better understanding and clarity for the readers while tracing through the testname.result file.

.. _end:

end
---

:Syntax:

:program:`end`

.. _end_timer:

end_timer
---------

:Syntax:

:program:`end_timer`

.. _error:

error
-----

:Syntax:

:program:`error error_code [,error_code[,...]]`

:Example:

.. code-block:: python

.. _eval:

eval
----

:Syntax:

:program:`eval statement`

:Example:

.. code-block:: python

.. _exec:

exec
----

:Syntax:

:program:`exec command [arg1[,arg2[,...]]]`

:Example:

.. code-block:: python

.. _exit:

exit
----

:Syntax:

:program:`exit`

:program:`exit` command is used to terminate the test. It is similar to :ref:`die`. However, here the test is not considered to have failed. 

:Example:

::

   tests/t/testname.test:
   SELECT 1;
   exit
   SELECT 2;
   
:Output:

::

   tests/r/testname.result:
   SELECT 1;
   1
   1

.. note:: In the above example, the test for ``select 2`` is not executed. Often this statement is used with a conditional statement such as ``if``. That is, if a particular condition is satisfied, and the test has not yet failed so far, and needs no more testing, this exit statement can be used.

.. _file_exists:

file_exists
-----------

:Syntax:

:program:`file_exists file_name`

:Example:

.. code-block:: python

.. _horizontal_results:

horizontal_results
------------------

:Syntax:

:program:`horizontal_results`

:Example:

.. code-block:: python

.. _if:

if
--

:Syntax:

:program:`if(expr)`

:Example:

.. code-block:: python

.. _inc:

inc
---

:Syntax:

:program:`inc $var_name`

:program:`inc` takes in exactly one argument. The argument should be a ``variable``. The ``inc`` increments the value of the variable by 1. This command takes two forms. :program:`inc $variable_name;` and :program:`- -inc $variable_name`. This command is the reverse of :ref:`dec`

.. note:: If a constant is given as argument, the following error is thrown. ERROR:The argument to inc must be a variable (start with $)

.. note:: If two arguments are given, the following error is thrown ERROR:End of line junk detected

:Example:

::

    /tests/t/testname.test:
    let $foo = 5;
    echo $foo;
    inc $foo;
    echo $foo;

:Output:

::

    /tests/r/testname.result:
    5
    6

.. note:: If a string is stored in the variable, then it is not considered as an error. Incrementing such a variable will store 1 in the variable. 

:Example:

::
  
    /tests/t/testname.test:
    let $foo = 5;
    echo $foo;
    --inc $foo
    echo $foo;

:Output:

::

    /tests/r/testname.result:
    5
    6

.. note:: In the second form, the ``;`` is not required as a delimiter.

.. _let:

let
---

:Syntax:

:program:`let $var_name = value`

:program:`let $var_name = query_get_value(query, col_name, row_num)`

:Example:

.. code-block:: python

.. _mkdir:

mkdir
-----

:Syntax:

:program:`mkdir dir_name`

:Example:

.. code-block:: python

.. _list_files:

list_files
----------

:Syntax:

:program:`list_files dir_name [pattern]`

:Example:

.. code-block:: python

.. _list_files_append_file:

list_files_append_file
----------------------

:Syntax:

:program:`list_files_append_file file_name dir_name [pattern]`

:Example:

.. code-block:: python

.. _list_files_write_file:

list_files_write_file
---------------------

:Syntax:

:program:`list_files_write_file file_name dir_name [pattern]`

:Example:

.. code-block:: python

.. _lowercase_result:

lowercase_result
----------------

:Syntax:

:program:`lowercase_result`

:Example:

.. code-block:: python

.. _move_file:

move_file
---------

:Syntax:

:program:`move_file from_file to_file`

:Example:

.. code-block:: python

.. _perl:

perl
----

:Syntax:

:program:`perl [terminator]`

:Example:

.. code-block:: python

.. _ping:

ping
----

:Syntax:

:program:`ping`

.. _query:

query
-----

:Syntax:

:program:`query [statement]`

.. _query_get_value:

query_get_value
---------------

:Syntax:

:program:`query_get_value(query,col_name,row_num)`

:Example:

.. code-block:: python

.. _query_horizontal:

query_horizontal
----------------

:Syntax:

:program:`query_horizontal statement`

:Example:

.. code-block:: python

.. _query_vertical:

query_vertical
--------------

:Syntax:

:program:`query_vertical statement`

:Example:

.. code-block:: python

.. _real_sleep:

real_sleep
----------

:Syntax:

:program:`real_sleep num`

:Example:

.. code-block:: python

.. _reap:

reap
----

:Syntax:

:program:`reap`

.. _remove_file:

remove_file
-----------

:Syntax:

:program:`remove_file file_name`

:Example:

.. code-block:: python

.. _remove_files_wildcard:

remove_files_wildcard
---------------------

:Syntax:

:program:`remove_files_wildcard dir_name [pattern]`

:Example:

.. code-block:: python

.. _replace_column:

replace_column
--------------

:Syntax:

:program:`replace_column col_num value [col_num value [,...] ]`

:Example:

.. code-block:: python

.. _replace_regex:

replace_regex
-------------

:Syntax:

:program:`replace_regex /pattern/replacement/[i] ...`

:Example:

.. code-block:: python

.. _replace_result:

replace_result
--------------

:Syntax:

:program:`replace_result from_val to_val [from_val to_val [...]]`

:Example:

.. code-block:: python

.. _require:

require
-------

:Syntax:

:program:`require file_name`

:Example:

.. code-block:: python

.. _result:

result
------

:Syntax:

:program:`result file_name`

.. _rmdir:

rmdir
-----

:Syntax:

:program:`rmdir dir_name`

:Example:

.. code-block:: python

.. _save_master_pos:

save_master_pos
---------------

:Syntax:

:program:`save_master_pos`

.. _send:

send
----

:Syntax:

:program:`send [statement]`

:Example:

.. code-block:: python

.. _send_eval:

send_eval
---------

:Syntax:

:program:`send_eval [statement]`

:Example:

.. code-block:: python

.. _send_quit:

send_quit
---------

:Syntax:

:program:`send_quit [timeout]`

:Example:

.. code-block:: python

.. _shutdown_server:

shutdown_server
--------------- 

:Syntax:

:program:`shutdown_server [timeout]`

:Example:

.. code-block:: python

.. _skip:

skip
----

:Syntax:

:program:`skip [message]`

:Example:

.. code-block:: python

.. _sleep:

sleep
-----

:Syntax:

:program:`sleep num`

:Example:

.. code-block:: python

.. _sorted_result:

sorted_result
-------------

:Syntax:

:program:`sorted_result`

:Example:

.. code-block:: python

.. _source:

source
------

:Syntax:

:program:`source file_name`

:Example:

.. code-block:: python

.. _start_timer:

start_timer
-----------

:Syntax:

:program:`start_timer`

.. _sync_slave_with_master:

sync_slave_with_master
----------------------

:Syntax:

:program:`sync_slave_with_master [connection_name]`

.. _sync_with_master:

sync_with_master
----------------

:Syntax:

:program:`sync_with_master offset`

.. _system:

system
------

:Syntax:

:program:`system command [arg1[,arg2[,...]]]`

:Example:

.. code-block:: python

.. _vertical_results:

vertical_results
----------------

:Syntax:

:program:`vertical_results`

:Example:

.. code-block:: python

.. _wait_for_slave_to_stop:

wait_for_slave_to_stop
----------------------

:Syntax:

:program:`wait_for_slave_to_stop`

.. _while:

while
-----

:Syntax:

:program:`while(expr)`

:program:`while()` defines an action block which gets executed over a loop. The while command expects a value / variable (expr) which decides whether or not the next iteration has to be carried out. If the value is 0, it is considered as ``false`` and the loop terminates. The body of the while block, which contains the set of statements to be executed repeatedly, should be enclosed within curly braces ``{`` and ``}``.

.. note:: Any non-zero value, positive / negative is treated as a true, and the loop gets executed. The expression expr does not support boolean expressions. 

:Example:

::
   
   /tests/t/testname.test:
   let $test=3;
   let $iteration=1;
   while($test)
   {
     echo test iteration $iteration;
     SELECT 1;
     dec $test;
     inc $iteration;
   }

:Output:

::

   /tests/r/testname.result:
   test iteration 1
   SELECT 1;
   1
   1
   test iteration 2
   SELECT 1;
   1
   1
   test iteration 3
   SELECT 1;
   1
   1

.. note:: Ensure that, the expr value becomes zero at some point of time. Else, the loop gets executed infinitely and the test gets stalled. 

.. _write_file:

write_file
----------

:Syntax:

:program:`write_file file_name [terminator]`

:program:`write_file` command is write data to the file specified by ``file_name``. When this command is issued, a file with the name as ``file_name`` is created and data is written to it. The end of the data, that is to be written, is marked by the terminator.

.. note:: If the file exists, it is not considered as error / the test will not fail. Instead, the contents of the file will be replaced by the data that is to be written.

The ``file_name`` can be substituted via variables. 

:Example:

::

   let $MY_FILE = ~/foo/bar.txt
   
   write_file $MY_FILE;
   testing...
   EOF

:Output:

::

   ~/foo/bar.txt:
   testing...

:Example:

::

   let $MY_FILE = ~/foo/bar.txt

   write_file $MY_FILE stop;
   testing with test-run...
   stop
  
:Output:

::

   ~/foo/bar.txt:
   testing with test-run...

.. note:: In the above example, the contents present previously in bar.txt are overwritten
   
