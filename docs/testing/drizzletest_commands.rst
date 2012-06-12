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

.. _copy_file:

copy_file
---------

.. _dec:

dec
---

.. _delimiter:

delimiter
---------

.. _die:

die
---

.. _diff_files:

diff_files
----------

.. _dirty_close:

dirty_close
-----------

.. _disable/enable_abort_on_error:

disable/enable_abort_on_error
-----------------------------

.. _disable/enable_connect_log:

disable/enable_connect_log
--------------------------

.. _disable/enable_info:

disable/enable_info
-------------------

.. _disable/enable_metadata:

disable/enable_metadata
-----------------------

.. _disable/enable_parsing:

disable/enable_parsing
----------------------

.. _disable/enable_ps_protocol:

disable/enable_ps_protocol
--------------------------

.. _disable/enable_query_log:

disable/enable_query_log
------------------------

.. _disable/enable_reconnect:

disable/enable_reconnect
------------------------

.. _disable/enable_result_log:

disable/enable_result_log
-------------------------

.. _disable/enable_rpl_parse:

disable/enable_rpl_parse
------------------------

.. _disable/enable_warnings:

disable/enable_warnings
-----------------------

.. _disconnect:

disconnect
----------

.. _echo:

echo
----

.. _end:

end
---

.. _end_timer:

end_timer
---------

.. _error:

error
-----

.. _eval:

eval
----

.. _exec:

exec
----

.. _exit:

exit
----

.. _file_exists:

file_exists
-----------

.. _horizontal_results:

horizontal_results
------------------

.. _if:

if
--

.. _inc:

inc
---

.. _let:

let
---

.. _mkdir:

mkdir
-----

.. _list_files:

list_files
----------

.. _list_files_append_file:

list_files_append_file
----------------------

.. _list_files_write_file:

list_files_write_file
---------------------

.. _lowercase_result:

lowercase_result
----------------

.. _move_file:

move_file
---------

.. _perl:

perl
----

.. _ping:

ping
----

.. _query:

query
-----

.. _query_get_value:

query_get_value
---------------

.. _query_horizontal:

query_horizontal
----------------

.. _query_vertical:

query_vertical
--------------

.. _real_sleep:

real_sleep
----------

.. _reap:

reap
----

.. _remove_file:

remove_file
-----------

.. _remove_files_wildcard:

remove_files_wildcard
---------------------

.. _replace_column:

replace_column
--------------

.. _replace_regex:

replace_regex
-------------

.. _replace_result:

replace_result
--------------

.. _require:

require
-------

.. _result:

result
------

.. _rmdir:

rmdir
-----

.. _save_master_pos:

save_master_pos
---------------

.. _send:

send
----

.. _send_eval:

send_eval
---------

.. _send_quit:

send_quit
---------

.. _shutdown_server:

shutdown_server
--------------- 

.. _skip:

skip
----

.. _sleep:

sleep
-----

.. _sorted_result:

sorted_result
-------------

.. _source:

source
------

.. _start_timer:

start_timer
-----------

.. _sync_slave_with_master:

sync_slave_with_master
----------------------

.. _sync_with_master:

sync_with_master
----------------

.. _system:

system
------

.. _vertical_results:

vertical_results
----------------

.. _wait_for_slave_to_stop:

wait_for_slave_to_stop
----------------------

.. _while:

while
-----

.. _write_file:

write_file
----------
   
   
