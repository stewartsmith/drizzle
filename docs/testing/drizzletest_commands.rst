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

:program:``

:Example:

.. code-block:: python

.. _exec:

exec
----

:Syntax:

:program:``

:Example:

.. code-block:: python

.. _exit:

exit
----

:Syntax:

:program:``

:Example:

.. code-block:: python

.. _file_exists:

file_exists
-----------

:Syntax:

:program:``

:Example:

.. code-block:: python

.. _horizontal_results:

horizontal_results
------------------

:Syntax:

:program:``

:Example:

.. code-block:: python

.. _if:

if
--

:Syntax:

:program:``

:Example:

.. code-block:: python

.. _inc:

inc
---

:Syntax:

:program:``

:Example:

.. code-block:: python

.. _let:

let
---

:Syntax:

:program:``

:Example:

.. code-block:: python

.. _mkdir:

mkdir
-----

:Syntax:

:program:``

:Example:

.. code-block:: python

.. _list_files:

list_files
----------

:Syntax:

:program:``

:Example:

.. code-block:: python

.. _list_files_append_file:

list_files_append_file
----------------------

:Syntax:

:program:``

:Example:

.. code-block:: python

.. _list_files_write_file:

list_files_write_file
---------------------

:Syntax:

:program:``

:Example:

.. code-block:: python

.. _lowercase_result:

lowercase_result
----------------

:Syntax:

:program:``

:Example:

.. code-block:: python

.. _move_file:

move_file
---------

:Syntax:

:program:``

:Example:

.. code-block:: python

.. _perl:

perl
----

:Syntax:

:program:``

:Example:

.. code-block:: python

.. _ping:

ping
----

:Syntax:

:program:``

:Example:

.. code-block:: python

.. _query:

query
-----

:Syntax:

:program:``

:Example:

.. code-block:: python

.. _query_get_value:

query_get_value
---------------

:Syntax:

:program:``

:Example:

.. code-block:: python

.. _query_horizontal:

query_horizontal
----------------

:Syntax:

:program:``

:Example:

.. code-block:: python

.. _query_vertical:

query_vertical
--------------

:Syntax:

:program:``

:Example:

.. code-block:: python

.. _real_sleep:

real_sleep
----------

:Syntax:

:program:``

:Example:

.. code-block:: python

.. _reap:

reap
----

:Syntax:

:program:``

:Example:

.. code-block:: python

.. _remove_file:

remove_file
-----------

:Syntax:

:program:``

:Example:

.. code-block:: python

.. _remove_files_wildcard:

remove_files_wildcard
---------------------

:Syntax:

:program:``

:Example:

.. code-block:: python

.. _replace_column:

replace_column
--------------

:Syntax:

:program:``

:Example:

.. code-block:: python

.. _replace_regex:

replace_regex
-------------

:Syntax:

:program:``

:Example:

.. code-block:: python

.. _replace_result:

replace_result
--------------

:Syntax:

:program:``

:Example:

.. code-block:: python

.. _require:

require
-------

:Syntax:

:program:``

:Example:

.. code-block:: python

.. _result:

result
------

:Syntax:

:program:``

:Example:

.. code-block:: python

.. _rmdir:

rmdir
-----

:Syntax:

:program:``

:Example:

.. code-block:: python

.. _save_master_pos:

save_master_pos
---------------

:Syntax:

:program:``

:Example:

.. code-block:: python

.. _send:

send
----

:Syntax:

:program:``

:Example:

.. code-block:: python

.. _send_eval:

send_eval
---------

:Syntax:

:program:``

:Example:

.. code-block:: python

.. _send_quit:

send_quit
---------

:Syntax:

:program:``

:Example:

.. code-block:: python

.. _shutdown_server:

shutdown_server
--------------- 

:Syntax:

:program:``

:Example:

.. code-block:: python

.. _skip:

skip
----

:Syntax:

:program:``

:Example:

.. code-block:: python

.. _sleep:

sleep
-----

:Syntax:

:program:``

:Example:

.. code-block:: python

.. _sorted_result:

sorted_result
-------------

:Syntax:

:program:``

:Example:

.. code-block:: python

.. _source:

source
------

:Syntax:

:program:``

:Example:

.. code-block:: python

.. _start_timer:

start_timer
-----------

:Syntax:

:program:``

:Example:

.. code-block:: python

.. _sync_slave_with_master:

sync_slave_with_master
----------------------

:Syntax:

:program:``

:Example:

.. code-block:: python

.. _sync_with_master:

sync_with_master
----------------

:Syntax:

:program:``

:Example:

.. code-block:: python

.. _system:

system
------

:Syntax:

:program:``

:Example:

.. code-block:: python

.. _vertical_results:

vertical_results
----------------

:Syntax:

:program:``

:Example:

.. code-block:: python

.. _wait_for_slave_to_stop:

wait_for_slave_to_stop
----------------------

:Syntax:

:program:``

:Example:

.. code-block:: python

.. _while:

while
-----

:Syntax:

:program:``

:Example:

.. code-block:: python

.. _write_file:

write_file
----------

:Syntax:

:program:``

:Example:

.. code-block:: python
   
   
