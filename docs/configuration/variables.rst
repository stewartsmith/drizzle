.. program:: drizzled

.. _configuration_variables:

Variables
=========

Variables reflect the running configuration of Drizzle.
Variables allow you to configure (or reconfigure) certain values at runtime,
whereas :ref:`configuration_options` configure Drizzle at startup and cannot
be changed without restarting Drizzle.

To see which variables are avaiable, execute the following:

.. code-block:: mysql

   drizzle> SELECT * FROM DATA_DICTIONARY.GLOBAL_VARIABLES;
   +--------------------------------------------+--------------------+
   | Variable_name                              | Value              |
   +--------------------------------------------+--------------------+
   | auto_increment_increment                   | 1                  | 
   | auto_increment_offset                      | 1                  | 
   | autocommit                                 | ON                 | 
   | back_log                                   | 128                | 
   | basedir                                    | /opt/drizzle       |
   | bulk_insert_buffer_size                    | 8388608            | 
   | collation_server                           | utf8_general_ci    | 
   | completion_type                            | 0                  | 
   | datadir                                    | /opt/drizzle/data  |
   | div_precision_increment                    | 4                  | 
   | drizzle_protocol_bind_address              | localhost          | 
   | drizzle_protocol_buffer_length             | 16384              | 
   | drizzle_protocol_connect_timeout           | 10                 | 
   | drizzle_protocol_max_connections           | 1000               | 
   | drizzle_protocol_port                      | 290132299          | 
   | drizzle_protocol_read_timeout              | 30                 | 
   | drizzle_protocol_retry_count               | 10                 | 
   | drizzle_protocol_write_timeout             | 60                 | 
   | error_count                                | 0                  | 
   | foreign_key_checks                         | ON                 | 
   ...

The ``DATA_DICTIONARY.GLOBAL_VARIABLES`` and
``DATA_DICTIONARY.SESION_VARIABLES`` views only list the available variables
and their current values, but internally each variable has four attributes
which affect how, if, and when it can be set:

#. Creator
#. Option
#. Dynamic
#. Scope

Variable are created either by the Drizzle kernel or by a plugin.
Varaibles created by a plugin are prefixed with the plugin's name.
For example, ``drizzle_protocol_port`` is created by the
:doc:`/plugins/drizzle_protocol/index` plugin.  Else, the variable
is created by the Drizzle kernel.

Most variables have a corresponding
:ref:`command line option <command_line_options>` which
initializes the variable.  For example, :option:`--drizzle-protocol.port`
initializes ``drizzle_protocol_port``.  Command line options are
converted to variable names by stripping
the option's leading ``--`` and changing all ``-`` (hyphens) and ``.`` 
(periods) to ``_`` (underscores).  Variables without corresponding
command line options are are usually switches (to enable or disable
some feature) or counters (like ``error_count``), and these variables can
only be accessed or changed once Drizzle is running.

Dynamic variables can be changed at runtime, and the new value takes
affect immediately.  Documentation for each variables indicates if it is
dynamic or not.  Most variables, however, are static which means that
they only reflect the value of their corresponding option.  To change a static
variable, you must change its corresponding option, then restart Drizzle.
Certain variables are static and do not have a corresponding option because
they are purely informational, like ``version`` and ``version_comment``.

A variable's scope is either global, session, or both (global and session).
Global variables apply to all connections (each connection is a session).
Session variables apply to each connection and changes to a session in one
connection do not change or affect the same variable in another connection.
Changes to session variables are lost when the connection closes, but
changes to global variables remain in affect until changed again.

.. note::

   Configuration variables and :ref:`user_defined_variables` are
   different.  :ref:`user_defined_variables` do not affect the
   configuration of Drizzle, and they are always dynamic, session
   variables.

.. _setting_variables:

Setting Variables
-----------------

The ``SET`` command sets global and session variables:

.. code-block:: mysql

   -- Set global variable
   SET GLOBAL variable=value;

   -- Set sesion variable
   SET variable=value
   SET SESSION variable=value

If setting the variable succeeds, the command finishes silently like:

.. code-block:: mysql

   drizzle> SET SESSION max_allowed_packet=10485760;
   Query OK, 0 rows affected (0.001475 sec)

Else, an error occurs if the variable cannot be changed:

.. code-block:: mysql

   drizzle> SET tmpdir="/tmp";
   ERROR 1238 (HY000): Variable 'tmpdir' is a read only variable

.. _querying_variables:

Querying Variables
------------------

The ``DATA_DICTIONARY.GLOBAL_VARIABLES`` and
``DATA_DICTIONARY.SESSION_VARIABLES`` are views for querying 
global and session variables respectively.  For example, to see all
variables for the :doc:`/plugins/syslog/index` plugin:

.. code-block:: mysql

   drizzle> SELECT * FROM DATA_DICTIONARY.GLOBAL_VARIABLES WHERE VARIABLE_NAME LIKE 'syslog_%';
   +----------------------------------------+----------------+
   | VARIABLE_NAME                          | VARIABLE_VALUE |
   +----------------------------------------+----------------+
   | syslog_errmsg_enable                   | OFF            | 
   | syslog_errmsg_priority                 | warning        | 
   | syslog_facility                        | local0         | 
   | syslog_logging_enable                  | OFF            | 
   | syslog_logging_priority                | warning        | 
   | syslog_logging_threshold_big_examined  | 0              | 
   | syslog_logging_threshold_big_resultset | 0              | 
   | syslog_logging_threshold_slow          | 0              | 
   +----------------------------------------+----------------+

