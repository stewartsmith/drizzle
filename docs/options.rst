.. _options:

Drizzled Options
================

There are many different options one can use to configure Drizzle.

Config File Options
-------------------

.. program:: drizzled

.. option:: --help-extended

   Display this help and exit after initializing plugins.

.. option:: --help, -?

   Display this help and exit.

.. option:: --no-defaults

   Configuration file defaults are not used if no-defaults is set

.. option:: --defaults-file arg

   Configuration file to use

.. option:: --config-dir arg (=/etc/drizzle)

   Base location for config files

.. option:: --plugin-dir arg

   Directory for plugins

.. option:: --pid-file arg

   Pid file used by drizzled.

Plugin Loading Options
----------------------

.. program:: drizzled

.. option:: --plugin-add arg

   Optional comma separated list of plugins to load at startup in addition
   to the default list of plugins.
  
   For example::

     --plugin_add=crc32,console,pbxt

.. option:: --plugin-remove arg

   Optional comma separated list of plugins to not load at startup.
   Effectively removes a plugin from the list of plugins to be loaded.

   For example::

     --plugin_remove=syslog,md5

.. option:: --plugin-load arg (=version, uuid_function, user_function, transaction_log, syslog, substr_functions, sleep, show_schema_proto, rot13, reverse_function, rand_function, multi_thread, md5, logging_stats, length, hex_functions, hello_world, filtered_replicator, errmsg_stderr, default_replicator, database_function, crc32, connection_id, compression, charlength, benchmark, ascii)

   Optional comma separated list of plugins to load at starup instead of 
   the default plugin load list. This completely replaces the whole list.

Kernel Options
--------------

.. program:: drizzled

.. option:: --auto-increment-increment arg (=1)
  
   Auto-increment columns are incremented by this

.. option:: --auto-increment-offset arg (=1)

   Offset added to Auto-increment columns.
   Used when auto-increment-increment != 1

.. option:: --basedir, -b arg

   Path to installation directory.
   All paths are usually resolved relative to this.

.. option:: --chroot, -r arg

   Chroot drizzled daemon during startup.

.. option:: --collation-server arg

   Set the default collation.

.. option:: --completion-type arg (=0)

   Default completion type.

.. option:: --core-file

   Write core on errors.

.. option:: --datadir arg

   Path to the database root.

.. option:: --default-storage-engine arg

   Set the default storage engine for tables.

.. option:: --default-time-zone arg

   Set the default time zone.

.. We should really remove --exit-info as an option
.. option:: --exit-info, -T arg

   Used for debugging;  Use at your own risk!

.. option:: --gdb

   Set up signals usable for debugging

.. option:: --lc-time-name arg

   Set the language used for the month names and the days of the week.

.. option:: --log-warnings, -W arg

   Log some not critical warnings to the log file.

.. Why is this a core argument?
.. option:: --port-open-timeout arg (=0)

   Maximum time in seconds to wait for the port to become free.
   A value of 0 means not to wait.

.. option:: --secure-file-priv arg

   Limit LOAD DATA, SELECT ... OUTFILE, and LOAD_FILE() to files within
   specified directory

.. Why is this still here?
.. option:: --server-id arg (=0)

   Uniquely identifies the server instance in the community of replication
   partners.

.. option:: --skip-stack-trace

   Don't print a stack trace on failure.

.. option:: --symbolic-links, -s

   Enable symbolic link support.

.. option:: --timed-mutexes

   Specify whether to time mutexes (only InnoDB mutexes are currently supported)

.. option:: --tmpdir, -t arg

   Path for temporary files.

.. option:: --transaction-isolation arg

   Default transaction isolation level.

.. option:: --user, -u arg

   Run drizzled daemon as user.
  
.. option:: --version, -V

   Output version information and exit.

.. option:: --back-log arg (=50)

   The number of outstanding connection requests Drizzle can have. This comes
   into play when the main Drizzle thread gets very many connection requests in
   a very short time.

.. option:: --bulk-insert-buffer-size arg (=8388608)
  
   Size of tree cache used in bulk insert optimization. Note that this is a
   limit per thread!

.. option:: --div-precision-increment arg (=4)
  
   Precision of the result of '/' operator will be increased on that value.

.. option:: --group-concat-max-len arg (=1024)

   The maximum length of the result of function  group_concat.

.. option:: --join-buffer-size arg (=131072)

   The size of the buffer that is used for full joins.

.. option:: --join-buffer-constraint arg (=0)

   A global constraint for join-buffer-size for all clients, cannot be set lower
   than --join-buffer-size.  Setting to 0 means unlimited.

.. Why is this a core arg?
.. option:: --max-allowed-packet arg (=64M)

   Max packetlength to send/receive from to server.

.. option:: --max-connect-errors arg (=10)

   If there is more than this number of interrupted connections from a host 
   this host will be blocked from further connections.

.. option:: --max-error-count arg (=64)

   Max number of errors/warnings to store for a statement.

.. option:: --max-heap-table-size arg (=16M)

   Don't allow creation of heap tables bigger than this.

.. option:: --max-join-size arg (=2147483647)

   Joins that are probably going to read more than max_join_size records return 
   an error.

.. option:: --max-length-for-sort-data arg (=1024)

   Max number of bytes in sorted records.
  
.. option:: --max-seeks-for-key arg (=18446744073709551615)

   Limit assumed max number of seeks when looking up rows based on a key

.. option:: --max-sort-length arg (=1024)

   The number of bytes to use when sorting BLOB or TEXT values (only the first 
   max_sort_length bytes of each value are used; the rest are ignored).

.. option:: --max-write-lock-count arg (=18446744073709551615)

   After this many write locks, allow some read locks to run in between.

.. option:: --min-examined-row-limit arg (=0)

   Don't log queries which examine less than min_examined_row_limit rows to
   file.

.. option:: --disable-optimizer-prune

   Do not apply any heuristic(s) during query optimization to prune, thus
   perform an exhaustive search from the optimizer search space.

.. option:: --optimizer-search-depth arg (=0)

   Maximum depth of search performed by the query optimizer. Values larger than
   the number of relations in a query result in better query plans, but take
   longer to compile a query. Smaller values than the number of tables in a
   relation result in faster optimization, but may produce very bad query plans. 
   If set to 0, the system will automatically pick a reasonable value; if set to
   MAX_TABLES+2, the optimizer will switch to the original find_best (used for
   testing/comparison).

.. option:: --preload-buffer-size arg (=32768)

   The size of the buffer that is allocated when preloading indexes

.. option:: --query-alloc-block-size arg (=8192)

   Allocation block size for query parsing and execution

.. option:: --query-prealloc-size arg (=8192)

   Persistent buffer for query parsing and execution

.. option:: --range-alloc-block-size arg (=4096)

   Allocation block size for storing ranges during optimization

.. option:: --read-buffer-size arg (=131072)

   Each thread that does a sequential scan allocates a buffer of this size for
   each table it scans. If you do many sequential scans, you may want to
   increase this value.  Note that this only affect MyISAM.

.. option:: --read-buffer-constraint arg (=0)

   A global constraint for read-buffer-size for all clients, cannot be set lower
   than --read-buffer-size.  Setting to 0 means unlimited.

.. option:: --read-rnd-buffer-size arg (=262144)

   When reading rows in sorted order after a sort, the rows are read through
   this buffer to avoid a disk seeks. If not set, then it's set to the value of
   record_buffer.

.. option:: --read-rnd-constraint arg (=0)

   A global constraint for read-rnd-buffer-size for all clients, cannot be set
   lower than --read-rnd-buffer-size.  Setting to 0 means unlimited.

.. option:: --scheduler arg (=multi-thread)

   Select scheduler to be used.

.. option:: --sort-buffer-size arg (=2097144)

   Each thread that needs to do a sort allocates a buffer of this size.

.. option:: --sort-buffer-constraint arg (=0)

   A global constraint for sort-buffer-size for all clients, cannot be set lower
   than --sort-buffer-size.  Setting to 0 means unlimited.

.. option:: --table-definition-cache arg (=128)

   The number of cached table definitions.

.. option:: --table-open-cache arg (=1024)

   The number of cached open tables.

.. option:: --table-lock-wait-timeout arg (=50)

   Timeout in seconds to wait for a table level lock before returning an error.
   Used only if the connection has active cursors.

.. option:: --thread-stack arg (=0)

   The stack size for each thread. 0 means use OS default.

.. option:: --tmp-table-size arg (=16M)

   If an internal in-memory temporary table exceeds this size, Drizzle will
   automatically convert it to an on-disk MyISAM table.

