Query Log
=========

The Query Log plugin logs queries to :ref:`logging-destinations`.  When :program:`drizzled` is started with  ``--plugin-add=query-log``, the query log plugin is enabled but all logging destinations are disabled to prevent flooding on busy servers because *all* queries are logged by default.  A destination can be enabled on the command line or later with ``SET GLOBAL``, and various thresholds can be set which restrict logging.

All query log system variables are global and dynamic so they can be changed while Drizzle is running.

Configuration
-------------

.. program:: drizzled

.. option:: --query-log.file FILE

  :Default: :file:`drizzled-queries.log`
  :Variable: ``query_log_file``

  The query log file.  The file is created if it does not exist.  If a full
  path is not specified, the default relative path is the :file:`local`
  directory under the :option:`--datadir` directory.

.. option:: --query-log.file-enabled

  :Default: ``FALSE``
  :Variable: ``query_log_file_enabled``

  Enable logging to :option:`--query-log.file`.
  This logging destination is disabled by default.  Specify this option to
  enable it from the command line, or it can be enabled once Drizzle is running
  by executing ``SET GLOBAL query_log_file_enabled = TRUE``.

.. option:: --query-log.threshold-execution-time MICROSECONDS

  :Default: ``0``
  :Variable: ``query_log_threshold_execution_time``

  Log queries with execution times greater than ``MICROSECONDS``.
  The query log file writes ``execution_time`` as seconds with six decimal
  place of precision, but this option must specify a number of microseconds.
  See :ref:`converting-seconds-to-microseconds`.

.. option:: --query-log.threshold-lock-time MICROSECONDS

  :Default: ``0``
  :Variable: ``query_log_threshold_lock_time``

  Log queries with lock times greater than ``MICROSECONDS``.
  The query log file writes ``lock_time`` as seconds with six decimal
  place of precision, but this option must specify a number of microseconds.
  See :ref:`converting-seconds-to-microseconds`.

.. option:: --query-log.threshold-rows-examined N

  :Default: ``0``
  :Variable: ``query_log_threshold_rows_examined``

  Log queries that examine more than ``N`` rows.

.. option:: --query-log.threshold-rows-sent N

  :Default: ``0``
  :Variable: ``query_log_threshold_rows_sent``

  Log queries that send (return) more than ``N`` rows.

.. option:: --query-log.threshold-session-time MICROSECONDS

  :Default: ``0``
  :Variable: ``query_log_threshold_session_time``

  Log queries form sessions active longer than ``MICROSECONDS``.
  The query log file writes ``session_time`` as seconds with six decimal
  place of precision, but this option must specify a number of microseconds.
  See :ref:`converting-seconds-to-microseconds`.

.. option:: --query-log.threshold-tmp-tables N

  :Default: ``0``
  :Variable: ``query_log_threshold_tmp_tables``

  Log queries that use more than ``N`` temporary tables.

.. option:: --query-log.threshold-warnings N

  :Default: ``0``
  :Variable: ``query_log_threshold_warnings``

  Log queries that cause more than ``N`` errors.

Examples
--------

Start Drizzle with the query plugin and log queries that take longer than 1 second to execute to the default log file:

.. code-block:: none

  sbin/drizzled               \
    --plugin-add=query-log    \
    --query-log.file-enabled  \
    --query-log.threshold-execution-time=1000000

Disable the query log plugin while Drizzle is running:

.. code-block:: mysql

  SET GLOBAL query_log_enabled = FALSE;

Disable and close the query log file while Drizzle is running:

.. code-block:: mysql

  SET GLOBAL query_log_file_enabled = FALSE;

Change the query log file while Drizzle is running:

.. code-block:: mysql

  SET GLOBAL query_log_file = "/tmp/new-file.log";

.. _converting-seconds-to-microseconds:


.. _logging-destinations:

Logging Destinations
--------------------

A logging destination is a place where the query log plugin writes queries.
There is currently only one logging destination: the :ref:`log-file` specified by :option:`--query-log.file`.  Other destinations are planned, like a table.

.. _log-file:

Log File
^^^^^^^^

The log file destination is enabled when both ``query_log_enabled`` and ``query_log_file_enabled`` are true (``SHOW VARIABLES`` lists ``ON`` and ``OFF`` instead of ``TRUE`` and ``FASLE``).  When ``query_log_file_enabled`` is true, the ``query_log_file`` is open.  When ``query_log_file_enabled`` is set false, the log file is closed.  This is helpful if you want to rotate the log file.

The log file is a plain text, structured file that is readable by humans and easily parsable by tools.  It looks like:

.. code-block:: none

  # 2011-05-15T01:48:17.814985
  # session_id=1 query_id=6 rows_examined=0 rows_sent=0 tmp_tables=0 warnings=1
  # execution_time=0.000315 lock_time=0.000315 session_time=16.723020
  # error=true
  # schema=""
  set query_log_file_enabled=true;
  #
  # 2011-05-15T01:48:21.526746
  # session_id=1 query_id=7 rows_examined=10 rows_sent=10 tmp_tables=0 warnings=0
  # execution_time=0.000979 lock_time=0.000562 session_time=20.435445
  # error=false
  # schema=""
  show variables like 'query_log%';
  #

Events are separated by a single ``#`` character.  This record separator can be used by programs like :program:`awk` and :program:`perl` to easily separate events in a log.

The first line line of each event is a UTC/GMT timestamp with microsecond precision; the timezone cannot be changed.  The second line has attributes with integer values.  The third line has attributes with high-precision time values, always with six decimals places of precision.  The fourth line has attributes with boolean values, either ``true`` or ``false``.  The fifth line has attributes with string values, always double-quoted.  Remaining lines are the query which can contain multiple lines, blank lines, et.  The record separator marks the event of the event.

Bugs and Limitations
--------------------

The authoritative source for issues, bugs and updated information is always
`Drizzle on Launchpad <https://launchpad.net/drizzle>`_, but this is a list of notable bugs and limitations at the time of writing of which you should be aware before using this plugin.

* Error handling and reporting is not the best.  This mostly affects changing ``query_log_file``.  If you try to use a file that cannot be opened, the query log plugin prints an error to ``STDERR`` and disabled ``query_log_file_enabled``.
* ``lock_time`` is broken, wrong.  See https://bugs.launchpad.net/drizzle/+bug/779708.
* If the query log file is removed or changed while open (i.e. while ``query_log_file_enabled`` is true), it will not be recreated and query logging will stop.  You need to disable and re-enable the log file to restore logging.

Converting Seconds to Microseconds
----------------------------------

Attributes in the query log file that end with ``_time``, like ``execution_time`` and ``lock_time``, are written as seconds with six decimal places of precision, like ``1.000456``.  These values are easier for humans to read, but Drizzle uses micrsecond values internally so it is necessary to convert from one to the other.

To convert from seconds to microseconds, multiply the seconds value by
``1000000`` (that's a one and six zeros).  For example:

  0.5 second *  1000000 = 500000 microseconds

To convert back, multiple the number of microseconds by ``0.000001`` (that's zero point five zeros and a one).

Authors
-------

Daniel Nichter

Version
-------

This documentation applies to **query_log 0.1**.

To see which version of the plugin a Drizzle server is running, execute:

.. code-block:: mysql

   SELECT MODULE_VERSION FROM DATA_DICTIONARY.MODULES WHERE MODULE_NAME='query_log'

Changelog
---------

v0.1
^^^^
* First release.
