Drizzletest Commands
====================

The commands that are endorsed in Drizzletest are delineated in the following documentation. Examples are given for the commands. Browse **tests/t** for more examples.

.. note:: The commands are not case sensitive

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
   
`append_file` is similar to 
:ref:`write_file`
except for the text untill the ``[terminator]`` are appended to the end of the existing file. If the file ``file_name`` does not exist, a new file with the name ``file_name`` is created first. The file name can be substituted with variables.

:Example:

.. code-block:: python

    write_file $TMP_DIR/file1;
    writing line one
    writing line two
    EOF
    append_file $TMP_DIR/file1;
    appending line three
    EOF
    
    write_file $TMP_DIR/file2 END_OF_FILE;
    writing line one
    writing line two
    END_OF_FILE
    append_file $TMP_DIR/file2 END_OF_FILE;
    appending line three
    END_OF_FILE

.. _cat_file:

cat_file
--------

:Syntax: 

:program:`cat_file file_name`
   
`cat_file` is used to display the contents of the file ``file_name`` on to the console output. 

:Example:

.. code-block:: python

    cat_file $TMP_DIR/file1;
    displaying line one
    displaying line two
    .
    .
    displaying line n
    EOF    

.. _change_user:

change_user
-----------

:Syntax: 

:program:`change_user [user_name], [password], [db_name]`

`change_user` changes the current user to the one specified by ``user_name``, sets ``password`` as the password and makes the database specified by ``db_name`` as the default database for the current connection

:Example:

.. code-block:: python
    
    change_user root;
    (changes the current user to root)
    change_user root,passwd;
    (changes the current user to root and makes passwd as the password
    change_user root,passwd,drizzle
    (changes the current user to root, makes passwd as the password and drizzle as the default database)

.. _character_set:

character_set
-------------

:Syntax: 

:program:`character_set charset_name`

`character_set` sets the default character set to the one specified by ``charset_name``. By default the character set is ``latin1``

:Example:

.. code-block:: python

    character_set utf8;
    --character_set sjis

.. _chmod:

chmod
-----

:Syntax: 

:program:`chmod octal_mode file_name`

`chmod` changes the access priviledges of the file ``file_name``. The file mode is given by a 4 digit octal number ``octal_mode``. Variables can be used to specify the file. 

:Example:

.. code-block:: python

    chmod 0777 $TMP_DIR/file1;
    The above command makes the file *file1* as readable, writable and executable by all users.


.. _connect:

connect
-------

:Syntax: 

:program:`connect (name, host_name, user_name, password, db_name [,port_num [,socket [,options [,default_auth]]]])`

`connect` opens a connection specified by the arguments and makes it the current connection. The various arguments are

``name`` is the name given to the connection. This name can be used with command like ``connection``, ``disconnect`` and ``dirty_close``. The ``name`` must not be currently in use by another open connection

``host_name`` refers to the host on which the server is running

``user_name`` and ``password`` are the username and the corresponding password for Drizzle account to use.

``db_name`` is the name of the default database to be used. :program:`*NO-ONE*` indicates that, no default database should be selected. This argument can also be left blank to select no database.

``port_num`` is the TCP/IP port number to use for the connection. The argument is optional. It can also be specified by a variable.

``socket`` is the socket file to use for connections to localhost. This argument is optional. The value can also be given by a variable.

``options`` can be one or more of the words SSL and COMPRESS, separated by white spaces. They specify the use of SSL and the compressed clien/server protocol, respectively

``default_auth`` refers to the authentication plugin name. It is passed to mysql_options() C API function using MYSQL_DEFAULT_AUTH option. The option --plugin-dir can be used to specify in which directory, the plugin resides.

.. note:: In order to omit an argument, leave it as blank(white spaces). An empty string replaces its position. If a port number / socket options is omitted, the default port / socket is chosen.

.. code-block:: python

    connect(conn1,localhost,root,,);
    connect(conn2,localhost,root,mypass,test);
    connect(conn3,127.0.0.1,root,,test,$MASTER_MYPORT);
    
.. note:: If a connection fails initially, and if the abort-on-error flag is set, then five retries are done to establish the connection.

.. _connection:

connection
----------

:Syntax:

:program:`connection connection_name`

``connection`` creates ``connection_name`` as the current connection. The connection_name can be specified by variables. Specifying the connection_name as ``default`` creates the connections that drizzletest opens when it starts.

:Example:

.. code-block:: python

    connection default;
    connection conn;
    connection root;

.. _copy_file:

copy_file
---------

:Syntax:

:program:`copy_file from_file to_file`

`copy_file` copies the contents of `from_file` into `to_file`. If the file `to_file` exists, then the command fails to execute. The names of the files can be provided through variables.

:Example:

.. code-block:: python

    copy_file source_file1.txt dest_file.txt;
    copy_file source_file2.txt dest_file.txt; (this command will not execute since dest_file.txt already exists)

.. _dec:

dec
---

:Syntax:

:program:`dec $variable_name`

``dec`` command is used to decrement the numeric value of a variable. If the variable does not have a numeric value associated with it, the result is undefined.

:Example:

.. code-block:: python

    dec $100;
    dec $count_value;
    
.. _delimiter:

delimiter
---------

:Syntax:

:program:`delimiter string`

``delimiter`` makes the ``string`` specified, as the default delimiter for the commands. The string can consist of 1 to 15 characters. The default delimiter for commands is semicolon(;)

:Example:

.. code-block:: python

    delimiter /;
    delimeter stop;
    
.. note:: Changing the delimiter becomes handy when we need to use a long SQL statement like the ``CREATE PROCEDURE`` which includes a semicolon delimited statement, but to be considered as a single statement. 

To reset the previous default delimiter, we can use ``delimiter ;|``

.. _die:

die
---

:Syntax:

:program:`die [message]`

``die`` command kills the test with appropriate error code. It also prints the message given as argument, as the reason for killing the test. 

:Example:

.. code-block:: python

    die cannot continue;
    drizzletest: At line 1: cannot continue
    not ok
    
When this command is executed, the test is killed and the message for killing the test is also displayed.

.. _diff_files:

diff_files
----------

:Syntax:

:program:`diff_files file_name1 file_name2`

``diff_files`` compares the two files given as arguments. That is ``file_name1`` and ``file_name2``. The command succeeds only if the two files are same. Else if the files are different or if either file does not exist, the command fails. The file names can be specified with variables.

:Example:

.. code-block:: python

    diff_files file_data1 file_data2;

.. _dirty_close:

dirty_close
-----------

:Syntax:

:program:`dirty_close connection_name`

``dirty_close`` closes the connection specified by ``connection_name``. This command is similar to ``disconnect``. However, this command calls ``vio_delete()`` prior to closing the connection. The connection_name can be specified via a variable. 

.. note:: If the ``connection_name`` refers to a current connection, then ``connection`` command must be used to swithch to a different(new) connection before executing any further SQL statements.

.. _disable/enable_abort_on_error:

disable/enable_abort_on_error
-----------------------------

:Syntax:

:program:`disable_abort_on_error,enable_abort_on_error`

This command is used to enable / disable the ``abort-on-error`` behavior. When this setting is enabled, drizzletest aborts the test, when an unexpected error is produced by a statement. In this case, the ``.reject`` file is not generated.

.. note:: The setting is enabled by default.

:Example:

.. code-block:: python

    --disable_abort_on_error
    --enable_abort_on_error

.. _disable/enable_connect_log:

disable/enable_connect_log
--------------------------

:Syntax:

:program:`disable_connect_log, enable_connect_log`

This command is used to enable / disable logging of connection details such as, when the connection was created, switch of connections etc. When this setting is enabled, drizzletest enters the details about the connection in the test results.

.. note:: The setting is disabled by default.

.. note:: If the query logging is disabled using ``disable_query_log``, connection logging is also automatically turned off, untill query logging is enabled.

:Example:

.. code-block:: python

    --disable_connect_log
    --enable_connect_log
    
.. _disable/enable_info:

disable/enable_info
-------------------

:Syntax:

:program:`disable_info, enable_info`

This command enables / disables the displaying of additional information about the SQL statement results. When this setting is enabled, drizzletest displays the affected rows count. The ``affected-rows`` value is the number of rows selected from statements such as ``SELECT``.

.. note:: The setting is disabled by default.

:Example:

.. code-block:: python

    --disable_info
    --enable_info

.. _disable/enable_metadata:

disable/enable_metadata
-----------------------

:Syntax:

:program:`disable_metadata, enable_metadata`

This command disables / enables the ``query metadata`` display. When this setting is enabled, drizzletest adds the query metadata to the result. The metadata consists of values corresponding to MYSQL_FIELD C API data structure.

.. note:: The setting is disabled by default

:Example:

.. code-block:: python

    --disable_metadata
    --enable_metadata

.. _disable/enable_parsing:

disable/enable_parsing
----------------------

:Syntax:

:program:`disable_parsing, enable_parsing`

This command enables / disables ``query parsing``. These commands are useful when we want to ``comment out`` (deactivate) a piece of code from the test case, without performing the hectic task of adding comment marker to each line.

.. note:: The setting is enabled by default

:Example:

.. code-block:: python

    --disable_parsing
    --enable_parsing

.. _disable/enable_ps_protocol:

disable/enable_ps_protocol
--------------------------

:Syntax:

:program:`disable_ps_protocol, enable_ps_protocol`

This command enables / disables ``prepared-statement protocol``.

.. note:: The setting is disabled by default

:Example:

.. code-block:: python

    --disable_ps_protocol
    --enable_ps_protocol

.. _disable/enable_query_log:

disable/enable_query_log
------------------------

:Syntax:

:program:`disable_query_log, enable_query_log`

This command enables / disables ``query logging``. When this setting is enabled, drizzletest displays the SQL statements, given as input, in the test results. Disabling this option will reduce the size of the test result produced. This in turn, reduces the complexity of comparing the actual test results with the expected ones.

.. note:: The setting is enabled by default.

:Example:

.. code-block:: python

    --disable_query_log
    --enable_query_log

.. _disable/enable_reconnect:

disable/enable_reconnect
------------------------

:Syntax:

:program:`disable_reconnect, enable_reconnect`

This command is used to enable / disable the ``automatic reconnection`` to a connection which has failed to connect. The automatic reconnection applies only to the ``current connection``. 

.. note:: The default setting depends on client library version

:Example:

.. code-block:: python

    --disable_reconnect
    --enable_reconnect

.. _disable/enable_result_log:

disable/enable_result_log
-------------------------

:Syntax:

:program:`disable_result_log, enable_result_log`

This command enables / disables the ``logging of results``. Enabling this setting will allow drizzletest to display the query results. It also displays the results from command like ``echo`` and ``exec``

.. note:: The setting is enabled by default

:Example:

.. code-block:: python

    --disable_result_log
    --enable_result_log

.. _disable/enable_rpl_parse:

disable/enable_rpl_parse
------------------------

:Syntax:

:program:`disable_rpl_parse, enable_rpl_parse`

This command enables / disables the parsing of statements that allow the statemtents to go to the master or the slave. 

.. note:: The default setting depends on the C API library

:Example:

.. code-block:: python

    --disable_rpl_parse
    --enable_rpl_parse

.. _disable/enable_warnings:

disable/enable_warnings
-----------------------

:Syntax:

:program:`disable_warnings, enable_warnings`

This command enables / disables the display of warnings. When this setting is enabled, drizzletest displays the warnings produced by any SQL statements using the ``SHOW WARNINGS``

.. note:: The setting is enabled by default

:Example:

.. code-block:: python

    --disable_warnings
    --enable_warnings

.. _disconnect:

disconnect
----------

:Syntax:

:program:`disconnect connection_name`

``disconnect`` closes (terminates) the connection specified by ``connection_name``. If a current connection is closed, then another connection must be created, or switched to another existing connection using the ``connection`` command, before executing any SQL statements.

:Example:

.. code-block:: python

    disconnect conn;

.. _echo:

echo
----

:Syntax:

:program:`echo text`

``echo`` is used to echo the text of test results. Variables can be used in the text. In that case, the value referenced by the variable will be displayed. 

.. note:: Quotation marks are not need around the text. If they are included, then they are included in the output.

:Example:

.. code-block:: python

    --echo another sql_mode test
    echo should return only 1 row;

.. _end:

end
---

:Syntax:

:program:`end`

``end`` is used to close a block, such as an if / while block. If there is no block open, then drizzletest exits with an error. 

.. note:: } and ``end`` are considered as the same

.. _end_timer:

end_timer
---------

:Syntax:

:program:`end_timer`

``end_timer`` is used to stop the running timer. Usually, the timer stops only when drizzletest exits.

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

``eval`` command replaces all the variables within the statement with their corresponding values. This processed statement is then sent to the server for execution. In short, eval provides ``variable expansion`` unlike using just ``statement`` alone.

.. note:: In order to specify a `$` character, use \$.

:Example:

.. code-block:: python

    eval USE $DB;
    eval CHANGE MASTER TO MASTER_PORT=$SLAVE_MYPORT;
    eval PREPARE STMT FROM "$stmt_1";

.. _exec:

exec
----

:Syntax:

:program:`exec command [arg1[,arg2[,...]]]`

``exec`` executes shell commands using the ``popen()`` library call. Variables used in the command are replaced by their corresponding values. In order to specify a `$` character, use \$

.. note:: In Cygwin, the commands are executed from cmd.exe. Some commands such as the ``rm`` cannot be executed using exec. For such cases, use ``system`` command.

:Example:

.. code-block:: python

    --exec $MYSQL_DUMP --xml --skip-create test
    --exec rm $MYSQLTEST_VARDIR/tmp/t1
    exec $MYSQL_SHOW test -v -v;
    
.. note:: exec and system commands may perform file system operations. But in that case, the test portability is reduced because, the commands tend to be OS specific. In the motive of providing portability, drizzletest makes available several commands like remove_file, chmod_file, mkdir etc.

.. _exit:

exit
----

:Syntax:

:program:`exit`

``exit`` terminates a test. However, the termination is considered as normal and will not consider it as a failure.

.. _file_exists:

file_exists
-----------

:Syntax:

:program:`file_exists file_name`

``file_exists`` returns true if the file ``file_name`` exists, else it returns false. The file name can be provided via variable substitution. 

:Example:

.. code-block:: python

    file_exists /etc/passwd;

.. _horizontal_results:

horizontal_results
------------------

:Syntax:

:program:`horizontal_results`

``horizontal_results`` displays the query results in horizontal format. By default, the results are displayed horizontally.

:Example:

.. code-block:: python

    --horizontal_results

.. _if:

if
--

:Syntax:

:program:`if(expr)`

``if`` begins an if block. drizzletest executes the block if the condition / expression evaluates to a non-zero number. The block ends with ``end`` of ``}``.

.. note:: if block doesn't have an else block

:Example:

.. code-block:: python

    let $condition=1;
    if($condition)
    {
        echo if block is executed;
    }
    if(!$condition)
    {
        echo if block is not executed;
    }
    
.. _inc:

inc
---

:Syntax:

:program:`inc $var_name`

``inc`` increments a variable of the type integer. If the variable is of any other type, the result is undefined.

:Example:

.. code-block:: python

    inc $var;
    inc $10

.. _let:

let
---

:Syntax:

:program:`let $var_name = value`



:Example:

.. code-block:: python

.. _mkdir:

mkdir
-----

:Syntax:

:program:`mkdir dir_name`

``mkdir`` creates a directory with the name specified by ``dir_name``. 

.. note:: This command returns 0 for success and returns 1 for failure.

:Example:

.. code-block:: python

    --mkdir repodir

.. _list_files:

list_files
----------

:Syntax:

:program:`list_files dir_name [pattern]`

``list_files`` is used to list all the files in the directory given by ``dir_name``. A pattern can be given optionally. If given, ``list_files`` displays the files in the directory ``dir_name`` and matching the given ``pattern``

.. note:: The pattern may contain wildcards 

:Example:

.. code-block:: python

    --list_files $MYSQLD_DATADIR/test t1*

.. _list_files_append_file:

list_files_append_file
----------------------

:Syntax:

:program:`list_files_append_file file_name dir_name [pattern]`

``list_files_append_file`` is similar to ``list_file``. Unlike list_file, this command lists the file in the given directory ``dir_name`` and appends the result into a file specified by ``file_name``.

.. note:: If the file ``file_name`` is not present, then it is created

 A pattern can be given optionally. If given, ``list_files_append_file`` displays the files which match the given ``pattern``.

.. note:: The pattern may contain wildcards

:Example:

.. code-block:: python

    --list_files_append_file $DRIZZLE_FILE_LIST_DIR/filelist $DRIZZLE_FILE_LIST_DIR/testdir *.txt;

.. _list_files_write_file:

list_files_write_file
---------------------

:Syntax:

:program:`list_files_write_file file_name dir_name [pattern]`

``list_files_write_file`` is similar to ``list_file_append_file``. Unlike list_file_append_file, this command always writes the result into a new file. 

.. note:: Even if the file ``file_name`` already exists, a new file will be created and the existing file will be replaced with it.

:Example:

.. code-block:: python

    --list_file_write_file $DRIZZLE_FILE_LIST_DIR/filelist $DRIZZLE_FILE_LIST_DIR/testdir *.txt;
    
.. _lowercase_result:

lowercase_result
----------------

:Syntax:

:program:`lowercase_result`

``lowercase_result`` will convert the resulting output of executing a SQL command into lowercase characters. This command is useful for achieving consistent results across different platforms.

.. note:: This command can be used along with other commands like ``replace`` and ``sorted_result``. In that case, lowercase is performed first and followed by the other command

:Example:

.. code-block:: python

    --lowercase_result

.. _move_file:

move_file
---------

:Syntax:

:program:`move_file from_file to_file`

``move_file`` will move the file contents from the file specified by ``from_file`` to the file specified by ``to_file``. 

.. note:: move_file actually performs a ``file renaming``. 

The ``from_file`` will be deleted after the contents are moved to the ``to_file``

.. note:: The filenames can be specified using variables.

:Example:

.. code-block:: python

    move_file $DRIZZLE_FILE_LIST_DIR/source $DRIZZLE_FILE_LIST_DIR/destination.out;

.. _perl:

perl
----

:Syntax:

:program:`perl [terminator]`

``perl`` command uses ``Perl`` to execute the lines following this command. This processing ends, when a line with the terminator is reached. 

.. note:: The default terminator is ``EOF``. The terminator can be changed by specifying along with the command.

:Example:

.. code-block:: python

    perl;
    print "using Perl to execute this line";
    EOF
    
    perl END;
    print "using Perl to execute till END";
    END

.. _ping:

ping
----

:Syntax:

:program:`ping`

``ping`` command pings the server. Whenever this command is issued, the drizzle_ping() API function is invoked. 

.. note:: ping is used to reconnect to a server when the connection is lost.

.. _query:

query
-----

:Syntax:

:program:`query [statement]`

``query`` command is used to send a query specified by ``statement`` to the server for execution. 

.. note:: This command is useful when we need to execute a SQL statement which begins with a drizzletest command.

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

``query_horizontal`` is used to execute the SQL specified using ``statement`` and then outputs the result of executing the query in horizontal manner.

:Example:

.. code-block:: python

    query_horizontal SELECT VERSION();

.. _query_vertical:

query_vertical
--------------

:Syntax:

:program:`query_vertical statement`

``query_vertical`` is used to execute the SQL specified using ``statement`` and then outputs the result of executing the query in vertical manner.

:Example:

.. code-block:: python

    query_vertical SELECT VERSION();

.. _real_sleep:

real_sleep
----------

:Syntax:

:program:`real_sleep num`

``real_sleep`` command is used to sleep for a time specified by ``num``. The time is in seconds. This command should not be used above the required level. This makes the test suite slower.


:Example:

.. code-block:: python

    --real_sleep 7;
    real_sleep 24
    
.. note:: real_sleep is not affected by --sleep command-line option. However sleep command is affected

.. _reap:

reap
----

:Syntax:

:program:`reap`

``reap`` command is used to receive a result of an SQL statement that is sent using the ``send`` command. 

.. note:: The reap command should not be used if there is no send command issued prior to that. 

.. _remove_file:

remove_file
-----------

:Syntax:

:program:`remove_file file_name`

``remove_file`` command is used to remove a file specified using  ``file_name``. The file name can be specified using variables. 

:Example:

.. code-block:: python

    remove_file $DRIZZLE_FILE_DIR/temp_file;
    
.. note:: If the file specified using ``file_name`` is not existing, an error is thrown.

.. _remove_files_wildcard:

remove_files_wildcard
---------------------

:Syntax:

:program:`remove_files_wildcard dir_name [pattern]`

This command is used to remove files in a directory specified using ``dir_name``, whose filenames match with the pattern given. Directories, sub-directories and files in sub-directories will not be deleted even if they match the pattern.

.. note:: ``?`` is used to represent single character. ``* `` is used to represent any sequence of 0 or more characters.  ``.`` is treated as normal character. The pattern should not include ``/``

.. note:: If no pattern is included, then this command deletes all the files in the directory. The directory remains undeleted.

:Example:

.. code-block:: python

    remove_files_wildcard $DRIZZLE_FILE_DIR temp*.txt;

.. _replace_column:

replace_column
--------------

:Syntax:

:program:`replace_column col_num value [col_num value [,...] ]`

``replace_column`` is used to replace strings in the output of the next statement that is to be executed. In this command, the content of ``col_num`` is replaced by the content specified by ``value``. 

.. note:: There can be any number of col_num value pairs. Column numbers start with 1

:Example:

.. code-block:: python

    --replace_column 5 !
    replace_column 1 a 2 b
    
.. note:: replace_column does not affect the output of exec (which replace_regex and replace_result do), because, the output of exec is not always columnar.

.. note:: To specify a double quote within the string that replaces, we can use \"

.. note:: If we use several ``replace_`` commands, for example, replace_regex, replace_result, etc, only the final one applies.

.. _replace_regex:

replace_regex
-------------

:Syntax:

:program:`replace_regex /pattern/replacement/[i] ...`

``replace_regex`` takes in two parameter ``pattern`` and ``replacement``. The ``pattern`` is used to find the specified patter in the output of the next statement and ``replacement`` is used to replace the found pattern with the specified replacement pattern. If more than one instance of the pattern is found in the string, then all of them are replaced. 

.. note:: For the match to be case insensitive, we can use ``i`` modifier.

:Example:

.. code-block:: python

    --replace_regex /strawberry/raspberry 
    
.. note:: The allowable patterns are the same as REGEXP SQL operator. Also, the pattern can contain parantheses for marking it as substrings. To refer to the substring we use ``\N``. \N in the replacement string will result in insertion of N-th substring matched by pattern.

:Example:

.. code-block:: python

    --replace_regex /(strawberry)/raspberry and \1/
    
.. note:: We can have multiple ``pattern/replacement`` 

.. _replace_result:

replace_result
--------------

:Syntax:

:program:`replace_result from_val to_val [from_val to_val [...]]`

This command replaces the ``from_val`` character in a string by the character specified using ``to_val``. We can issue more than one (from_val/to_val) pairs. 

:Example:

.. code-block:: python

    --replace_result 1024 MAX_KEY_LENGTH 3072 MAX_KEY_LENGTH
    replace_result $MASTER_MYPORT MASTER_PORT;

.. _require:

require
-------

:Syntax:

:program:`require file_name`

``require`` command is used to specify a file ``file_name`` for comparing the results of the next query with the contents of the file. 

.. note:: If the contents of the file, does not match with the results of the query / there is some error, then the test aborts.

:Example:

.. code-block:: python

    --require r/test1.result
    --require r/test2.require

.. _result:

result
------

:Syntax:

:program:`result file_name`

``result`` command is used to compare the contents of the file specified using ``file_name`` with the result of a test case, only after the test completes. If there is no match, then the result is written to ``r/file_name.reject`` file.

.. note:: If the --record command-line option is given, then the result is written to the file.result.

.. _rmdir:

rmdir
-----

:Syntax:

:program:`rmdir dir_name`

``rmdir`` command is used to delete / remove a directory specified by ``dir_name``. This command return 0 if the operation is successful or 1 if the operation fails.

:Example:

.. code-block:: python

    --rmdir DRIZZLE_DIR/testdir

.. _save_master_pos:

save_master_pos
---------------

:Syntax:

:program:`save_master_pos`

``save_master_pos`` saves the current binary log file name and position for master replication server. These can be used with commands like ``sync_with_master`` and ``sync_slave_with_master``

.. _send:

send
----

:Syntax:

:program:`send [statement]`

``send`` command is used to send a SQL statement to the server. The result of the statement must be received with reap command. 

.. note:: if statement is missed, then the next statement which is executed, will be sent. 

:Example:

.. code-block:: python

    send SELECT VERSION();
    
.. code-block:: python

    send;
    SELECT VERSION();
    
.. note:: Another SQL statement cannot be sent on the same connection between the send and reap.

.. _send_eval:

send_eval
---------

:Syntax:

:program:`send_eval [statement]`

This command sends the ``statement`` specifying the command, to the server, after evaluation. Thus, its a combination of send + eval. 

:Example:

.. code-block:: python

    --send_eval $STATEMENT

.. _send_quit:

send_quit
---------

:Syntax:

:program:`send_quit [timeout]`

``send_quit`` command is used to stop the server. This command has a view of the process ID of the server. It waits for the server to close by itself. If the server's process ID is still available even after the stipulated time ``timeout``, the process is killed. 

.. note:: If the timeout is not mentioned, it defaults to 60 seconds.

:Example:

.. code-block:: python

    send_quit;
    send_quit 100;

.. _shutdown_server:

shutdown_server
--------------- 

:Syntax:

:program:`shutdown_server [timeout]`

This command is similar to 
:ref:`send_quit` command.

:Example:

.. code-block:: python

    shutdown_server;
    shutdown_server 100;

.. _skip:

skip
----

:Syntax:

:program:`skip [message]`

``skip`` is used to terminate the processing of the test file. It stops the execution and displays the message specified as argument.

:Example:

.. code-block:: python

    let $condition=$cond_for_execution
    if(!$condition)
    {
        skip condition fails;
    }
        

.. _sleep:

sleep
-----

:Syntax:

:program:`sleep num`

``sleep`` command is used to sleep for specified ``num`` seconds. The num value can be fractional too.

.. note:: If a --sleep command-line argument is also given, it supresses the effect of sleep command. 

:Example:

If sleep 100 is given along with --sleep=50, then the command sleep 100 will sleep for 50 seconds only 

:Example:

.. code-block:: python

    --sleep 50;
    sleep 100;
    sleep 50.25;

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

``source`` command is used mainly for creating modular test cases. For example, if we have certain number of tests, all of which perform some basic tasks initially in setting up the server, then those tasks can be written in another file. This file can be invoked in the test by using the ``source`` command. 

.. note:: source command is similar to functions in programming languages like C

:Example:

.. code-block:: python

    --source drizzletest/create_schema.inc
    source drizzletest/create_schema.inc;
    
A file can use ``source`` to read other files. The maximum level of nesting allowed is 16.

Variables can be used to specify the files. If these variables contain quotation marks, then those marks are also considered during variable expansion. So quotes are usually not included.

.. _start_timer:

start_timer
-----------

:Syntax:

:program:`start_timer`

This command will restart an existing timer. Initially, the timer is always restarted during the start of each drizzletest.

.. _sync_slave_with_master:

sync_slave_with_master
----------------------

:Syntax:

:program:`sync_slave_with_master [connection_name]`

``sync_slave_with_master`` saves the replication coordinates for the current server which acts as the master, and then switches to the slave server till it syncs with the replication coordinates. 

This command is equivalent to 

.. code-block:: python

    save_master_pos;
    connection connection_name;
    sync_with_mater 0;

.. _sync_with_master:

sync_with_master
----------------

:Syntax:

:program:`sync_with_master offset`

``sync_with_master`` command is used for a slave replication server to wait untill it syncs with its master. The position to synchronize is specified by the ``save_master_pos`` and the ``offset``. So just specifying the offset would add its contents to the contents of ``save_master_pos``. 

.. note:: The save_master_pos should contain a value / executed prior executing this command. 

.. _system:

system
------

:Syntax:

:program:`system command [arg1[,arg2[,...]]]`

``system`` command is used to execute ``shell command`` or ``system related functions`` using the ``system()`` library function. 

.. note:: system command can reduce portablity. This is because, we give the command which are specific to an operating system (say unix-like), which can fail, if run on windows. For this purpose, there are commands like ``remove_file``, ``chmod_file``, which performs the system functions with added portability.

:Example:

.. code-block:: python

    --system rm $DRIZZLE_TEMP_DIR
    --system mkdir $DRIZZLE_REPO
    
.. note:: We can use variables as commands. The references of these variables are replaced by their corresponding values. If $ is being used, then preceed it by ``\`` ( \$ )

.. _vertical_results:

vertical_results
----------------

:Syntax:

:program:`vertical_results`

This command will display the results in vertical format. Horizontal display is the default format.

:Example:

.. code-block:: python

    --vertical_results

.. _wait_for_slave_to_stop:

wait_for_slave_to_stop
----------------------

:Syntax:

:program:`wait_for_slave_to_stop`

This command polls the connection to the slave replication server, by executing SHOW STATUS LIKE `slave_running` statements, untill the result is ``OFF``

.. _while:

while
-----

:Syntax:

:program:`while(expr)`

This statment marks the beginning of the while block. This block ends with ``end`` statement. The while block keeps on executing untill the expression ``expr`` evaluated to false.

.. note:: The expr should evaluate to false at some point of time. Else, the block moves into an infinite loop

:Example:

.. code-block:: python

    let $iter=10;
    while($iter)
    {
        echo "executing this statement";
        dec $iter;
    }

.. _write_file:

write_file
----------

:Syntax:

:program:`write_file file_name [terminator]`

``write_file`` command is used to write the lines following it, to the file specified using ``file_name``, untill the terminator is reached. 

.. note:: The default terminator is EOF

:Example:

.. code-block:: python

    write_file $DRIZZLE_FILE_DIR/test;
    test condition 1
    test condition 2
    EOF
    
    write_file $DRIZZLE_FILE_DIR/test END;
    test condition 1
    test condition 2
    END
    
.. note:: If the file specified using ``file_name`` already exists, an error is thrown.
   
   
