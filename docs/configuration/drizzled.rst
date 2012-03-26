.. program:: drizzled

.. _drizzled_configuration:

drizzled Configuration
======================

.. _drizzled_options:

Options
-------

.. _drizzled_gneral_options:

General Options
^^^^^^^^^^^^^^^

.. option:: --daemon, -d

   Run :program:`drizzled` as a daemon.

.. option:: --help, -?

   Display this help and exit.

.. option:: --help-extended

   Display this help and exit after initializing plugins.

.. option:: --user, -u ARG

   :Default:
   :Variable: 

   Run drizzled daemon as user.

.. option:: --version, -V

   :Default:
   :Variable: ``version``

   Print the version of Drizzle and exit.

.. _drizzled_config_file_options:

Config File Options
^^^^^^^^^^^^^^^^^^^

.. option:: --config-dir DIR

   :Default: :file:`/etc/drizzle`
   :Variable: 

   Base location for config files.

.. option:: --defaults-file FILE
   
   :Default:
   :Variable: 

   Configuration file to use.

.. option:: --no-defaults

   :Default:
   :Variable: 

   Configuration file defaults are not used if no-defaults is set.

.. _drizzled_plugin_options:

Plugin Options
^^^^^^^^^^^^^^

.. option:: --plugin-add LIST

   :Default:
   :Variable: 

   Optional comma separated list of plugins to load at startup in addition
   to the default list of plugins.
  
   For example::

     --plugin_add=crc32,console,pbxt

.. option:: --plugin-dir DIR

   :Default:
   :Variable: 

   Directory for plugins.

.. option:: --plugin-load LIST

   :Default: See :ref:`default_plugins`
   :Variable: 

   Optional comma separated list of plugins to load at starup instead of 
   the default plugin load list. This completely replaces the whole list.

.. option:: --plugin-remove LIST

   :Default:
   :Variable: 

   Optional comma separated list of plugins to not load at startup.
   Effectively removes a plugin from the list of plugins to be loaded.

   For example::

     --plugin_remove=syslog,md5

.. _drizzled_replication_options:

Replication Options
^^^^^^^^^^^^^^^^^^^

.. option:: --replicate-query

   :Default:
   :Variable: ``replicate_query``
   
   Include the SQL query in replicated protobuf messages.

.. option:: --transaction-message-threshold

   :Default: 1048576
   :Variable: ``transaction_message_threshold``

   Max message size written to transaction log, valid values 131072 - 1048576 bytes.

.. _drizzled_kernel_options:

Kernel Options
^^^^^^^^^^^^^^

.. option:: --auto-increment-increment ARG

   :Default: 1
   :Variable: ``auto_increment_increment``

   Auto-increment columns are incremented by this

.. option:: --auto-increment-offset ARG

   :Default: 1
   :Variable: ``auto_increment_offset``

   Offset added to Auto-increment columns.
   Used when auto-increment-increment != 1

.. option:: --back-log ARG

   :Default: 50
   :Variable: ``back_log``

   The number of outstanding connection requests Drizzle can have. This comes
   into play when the main Drizzle thread gets very many connection requests in
   a very short time.

.. option:: --basedir, -b ARG

   :Default:
   :Variable: ``basedir``

   Path to installation directory.
   All paths are usually resolved relative to this.

.. option:: --bulk-insert-buffer-size SIZE

   :Default: 8388608
   :Variable: ``bulk_insert_buffer_size``

   Size of tree cache used in bulk insert optimization. Note that this is a
   limit per thread!

.. option:: --chroot, -r ARG

   :Default:
   :Variable: 

   Chroot drizzled daemon during startup.

.. option:: --collation-server ARG

   :Default:
   :Variable: ``collation_server``

   Set the default collation.

.. option:: --completion-type ARG

   :Default: 0
   :Variable: ``completion_type``

   Unknown.

.. option:: --core-file

   :Default:
   :Variable: 

   Write core on errors.

.. option:: --datadir ARG

   :Default:
   :Variable: ``datadir``

   Path to the database root.

.. option:: --default-storage-engine ARG

   :Default: InnoDB
   :Variable: ``storage_engine``

   Set the default storage engine for tables.

.. option:: --default-time-zone ARG

   :Default:
   :Variable: 

   Set the default time zone.

.. option:: --disable-optimizer-prune

   :Default:
   :Variable: ``optimizer_prune_level``

   Do not apply any heuristic(s) during query optimization to prune, thus
   perform an exhaustive search from the optimizer search space.

.. option:: --div-precision-increment ARG

   :Default: 4
   :Variable: ``div_precision_increment``
  
   Precision of the result of '/' operator will be increased on that value.

.. We should really remove --exit-info as an option
.. option:: --exit-info, -T ARG

   :Default:
   :Variable: 

   Used for debugging;  Use at your own risk!

.. option:: --gdb

   :Default:
   :Variable: 

   Set up signals usable for debugging.

.. option:: --group-concat-max-len ARG

   :Default: 1024
   :Variable: ``group_concat_max_len``

   The maximum length of the result of function  group_concat.

.. option:: --join-buffer-constraint ARG

   :Default: 0
   :Variable: 

   A global constraint for join-buffer-size for all clients, cannot be set lower
   than :option:`--join-buffer-size`.  Setting to 0 means unlimited.

.. option:: --join-buffer-size SIZE

   :Default: 131072
   :Variable: ``join_buffer_size``

   The size of the buffer that is used for full joins.

.. option:: --lc-time-name ARG

   :Default:
   :Variable: ``lc_time_names``

   Set the language used for the month names and the days of the week.

.. option:: --log-warnings, -W ARG

   :Default:
   :Variable: 

   Log some not critical warnings to the log file.

.. Why is this a core ARG?
.. option:: --max-allowed-packet SIZE

   :Default: 64M
   :Variable: ``max_allowed_packet``

   Max packetlength to send/receive from to server.

.. option:: --max-connect-errors ARG

   :Default: 10
   :Variable: 

   If there is more than this number of interrupted connections from a host 
   this host will be blocked from further connections.

.. option:: --max-error-count ARG

   :Default: 64
   :Variable: ``max_error_count``

   Max number of errors/warnings to store for a statement.

.. option:: --max-heap-table-size SIZE

   :Default: 16M
   :Variable: ``max_heap_table_size``

   Don't allow creation of heap tables bigger than this.

.. option:: --max-join-size SIZE

   :Default: 2147483647
   :Variable: ``max_join_size``

   Joins that are probably going to read more than max_join_size records return 
   an error.

.. option:: --max-length-for-sort-data SIZE

   :Default: 1024
   :Variable: ``max_length_for_sort_data``

   Max number of bytes in sorted records.

.. option:: --max-seeks-for-key ARG

   :Default: -1
   :Variable: ``max_seeks_for_key``

   Limit assumed max number of seeks when looking up rows based on a key.
   Set to -1 to disable.

.. option:: --max-sort-length SIZE

   :Default: 1024
   :Variable: ``max_sort_length``

   The number of bytes to use when sorting BLOB or TEXT values (only the first 
   max_sort_length bytes of each value are used; the rest are ignored).

.. option:: --max-write-lock-count ARG

   :Default: -1
   :Variable: ``max_write_lock_count``

   After this many write locks, allow some read locks to run in between.
   Set to -1 to disable.

.. option:: --min-examined-row-limit ARG

   :Default: 0
   :Variable: ``min_examined_row_limit``

   Don't log queries which examine less than min_examined_row_limit rows to
   file.

.. option:: --optimizer-search-depth ARG

   :Default: 0
   :Variable: ``optimizer_search_depth``

   Maximum depth of search performed by the query optimizer. Values larger than
   the number of relations in a query result in better query plans, but take
   longer to compile a query. Smaller values than the number of tables in a
   relation result in faster optimization, but may produce very bad query plans. 
   If set to 0, the system will automatically pick a reasonable value; if set to
   MAX_TABLES+2, the optimizer will switch to the original find_best (used for
   testing/comparison).

.. option:: --pid-file FILE
   
   :Default:
   :Variable: ``pid_file``

   PID file used by :program:`drizzled`.

.. Why is this a core argument?
.. option:: --port-open-timeout ARG

   :Default: 0
   :Variable: 

   Maximum time in seconds to wait for the port to become free.
   A value of 0 means not to wait.

.. option:: --preload-buffer-size SIZE

   :Default: 32768
   :Variable: ``preload_buffer_size``

   The size of the buffer that is allocated when preloading indexes.

.. option:: --query-alloc-block-size SIZE

   :Default: 8192
   :Variable: ``query_alloc_block_size``

   Allocation block size for query parsing and execution.

.. option:: --query-prealloc-size SIZE

   :Default: 8192
   :Variable: ``query_prealloc_size``

   Persistent buffer for query parsing and execution.

.. option:: --range-alloc-block-size SIZE

   :Default: 4096
   :Variable: ``range_alloc_block_size``

   Allocation block size for storing ranges during optimization.

.. option:: --read-buffer-constraint ARG

   :Default: 0
   :Variable: 

   A global constraint for read-buffer-size for all clients, cannot be set lower
   than --read-buffer-size.  Setting to 0 means unlimited.

.. option:: --read-buffer-size SIZE

   :Default: 131072
   :Variable: ``read_buffer_size``

   Each thread that does a sequential scan allocates a buffer of this size for
   each table it scans. If you do many sequential scans, you may want to
   increase this value.  Note that this only affect MyISAM.

.. option:: --read-rnd-buffer-size SIZE

   :Default: 262144
   :Variable: ``read_rnd_buffer_size``

   When reading rows in sorted order after a sort, the rows are read through
   this buffer to avoid a disk seeks. If not set, then it's set to the value of
   record_buffer.

.. option:: --read-rnd-constraint ARG

   :Default: 0
   :Variable: 

   A global constraint for read-rnd-buffer-size for all clients, cannot be set
   lower than --read-rnd-buffer-size.  Setting to 0 means unlimited.

.. option:: --scheduler ARG

   :Default: multi-thread
   :Variable: ``scheduler``

   Select scheduler to be used.

.. option:: --secure-file-priv ARG

   :Default:
   :Variable: ``secure_file_priv``

   Limit LOAD DATA, SELECT ... OUTFILE, and LOAD_FILE() to files within
   specified directory.

.. Why is this still here?
.. option:: --server-id ARG

   :Default: 0
   :Variable: ``server_id``

   Uniquely identifies the server instance in the community of replication
   partners.

.. option:: --skip-stack-trace

   :Default:
   :Variable: 

   Don't print a stack trace on failure.

.. option:: --sort-buffer-constraint ARG

   :Default: 0
   :Variable: 

   A global constraint for sort-buffer-size for all clients, cannot be set lower
   than --sort-buffer-size.  Setting to 0 means unlimited.

.. option:: --sort-buffer-size SIZE
   
   :Default: 2097144
   :Variable: ``sort_buffer_size``

   Each thread that needs to do a sort allocates a buffer of this size.

.. option:: --symbolic-links, -s

   :Default:
   :Variable: 

   Enable symbolic link support.

.. option:: --table-definition-cache ARG

   :Default: 128
   :Variable: ``table_definition_cache``

   The number of cached table definitions.

.. option:: --table-lock-wait-timeout ARG

   :Default: 50
   :Variable: ``table_lock_wait_timeout``

   Timeout in seconds to wait for a table level lock before returning an error.
   Used only if the connection has active cursors.

.. option:: --table-open-cache ARG

   :Default: 1024
   :Variable: ``table_open_cache``

   The number of cached open tables.

.. option:: --thread-stack ARG

   :Default: 0
   :Variable: ``thread_stack``

   The stack size for each thread. 0 means use OS default.

.. option:: --timed-mutexes

   :Default:
   :Variable: ``timed_mutexes``

   Specify whether to time mutexes (only InnoDB mutexes are currently supported).

.. option:: --tmp-table-size SIZE

   :Default: 16M
   :Variable: ``tmp_table_size``

   If an internal in-memory temporary table exceeds this size, Drizzle will
   automatically convert it to an on-disk MyISAM table.

.. option:: --tmpdir, -t DIR

   :Default:
   :Variable: ``tmpdir``

   Path for temporary files.

.. option:: --transaction-isolation ARG

   :Default: REPEATABLE-READ
   :Variable: ``tx_isolation``

   Default transaction isolation level.

.. _drizzled_variables:

Variables
---------

.. _drizzled_auto_increment_increment:

* ``auto_increment_increment``

   :Scope: Global
   :Dynamic: No
   :Option: :option:`--auto-increment-increment`

.. _drizzled_auto_increment_offset:

* ``auto_increment_offset``

   :Scope: Global
   :Dynamic: No
   :Option: :option:`--auto-increment-offset`

.. _drizzled_autocommit:

* ``autocommit``

   :Scope: Global
   :Dynamic: No
   :Option: 

   If statements are auto-committed.

.. _drizzled_back_log:

* ``back_log``

   :Scope: Global
   :Dynamic: No
   :Option: :option:`--back-log`

.. _drizzled_basedir:

* ``basedir``

   :Scope: Global
   :Dynamic: No
   :Option: :option:`--basedir`

.. _drizzled_bulk_insert_buffer_size:

* ``bulk_insert_buffer_size``

   :Scope: Global
   :Dynamic: No
   :Option: :option:`--bulk-insert-buffer-size`

.. _drizzled_collation_server:

* ``collation_server``

   :Scope: Global
   :Dynamic: No
   :Option: :option:`--collation-server`

.. _drizzled_completion_type:

* ``completion_type``

   :Scope: Global
   :Dynamic: No
   :Option: :option:`--completion-type`

.. _drizzled_datadir:

* ``datadir``

   :Scope: Global
   :Dynamic: No
   :Option: :option:`--datadir`

.. _drizzled_div_precision_increment:

* ``div_precision_increment``

   :Scope: Global
   :Dynamic: No
   :Option: :option:`--div-precision-increment`

.. _drizzled_error_count:

* ``error_count``

   :Scope: Global
   :Dynamic: No
   :Option: 

   Error count.

.. _drizzled_foreign_key_checks:

* ``foreign_key_checks``

   :Scope: Global
   :Dynamic: No
   :Option: 

   If foreign key checks are enabled.

.. _drizzled_group_concat_max_len:

* ``group_concat_max_len``

   :Scope: Global
   :Dynamic: No
   :Option: :option:`--group-concat-max-len`

.. _drizzled_hostname:

* ``hostname``

   :Scope: Global
   :Dynamic: No
   :Option: 

   Hostname of the server.

.. _drizzled_identity:

* ``identity``

   :Scope: Global
   :Dynamic: No
   :Option: 

   Unknown.

.. _drizzled_join_buffer_size:

* ``join_buffer_size``

   :Scope: Global
   :Dynamic: No
   :Option: :option:`--join-buffer-size`

.. _drizzled_last_insert_id:

* ``last_insert_id``

   :Scope: Global
   :Dynamic: No
   :Option: 

   Last auto-increment insert ID value.

.. _drizzled_lc_time_names:

* ``lc_time_names``

   :Scope: Global
   :Dynamic: No
   :Option: 

   Unknown.

.. _drizzled_max_allowed_packet:

* ``max_allowed_packet``

   :Scope: Global
   :Dynamic: No
   :Option: :option:`--max-allowed-packet`

.. _drizzled_max_error_count:

* ``max_error_count``

   :Scope: Global
   :Dynamic: No
   :Option: :option:`--max-error-count`

.. _drizzled_max_heap_table_size:

* ``max_heap_table_size``

   :Scope: Global
   :Dynamic: No
   :Option: :option:`--max-heap-table-size`

.. _drizzled_max_join_size:

* ``max_join_size``

   :Scope: Global
   :Dynamic: No
   :Option: :option:`--max-join-size`

.. _drizzled_max_length_for_sort_data:

* ``max_length_for_sort_data``

   :Scope: Global
   :Dynamic: No
   :Option: :option:`--max-length-for-sort-data`

.. _drizzled_max_seeks_for_key:

* ``max_seeks_for_key``

   :Scope: Global
   :Dynamic: No
   :Option: :option:`--max-seeks-for-key`

.. _drizzled_max_sort_length:

* ``max_sort_length``

   :Scope: Global
   :Dynamic: No
   :Option: :option:`--max-sort-length`

.. _drizzled_max_write_lock_count:

* ``max_write_lock_count``

   :Scope: Global
   :Dynamic: No
   :Option: :option:`--max-write-lock-count`

.. _drizzled_min_examined_row_limit:

* ``min_examined_row_limit``

   :Scope: Global
   :Dynamic: No
   :Option: :option:`--min-examined-row-limit`

.. _drizzled_optimizer_prune_level:

* ``optimizer_prune_level``

   :Scope: Global
   :Dynamic: No
   :Option: :option:`--disable-optimizer-prune`

   Optimizer prune level.

.. _drizzled_optimizer_search_depth:

* ``optimizer_search_depth``

   :Scope: Global
   :Dynamic: No
   :Option: :option:`--optimizer-search-depth`

.. _drizzled_pid_file:

* ``pid_file``

   :Scope: Global
   :Dynamic: No
   :Option: :option:`--pid-file`

.. _drizzled_plugin_dir:

* ``plugin_dir``

   :Scope: Global
   :Dynamic: No
   :Option: :option:`--plugin-dir`

.. _drizzled_preload_buffer_size:

* ``preload_buffer_size``

   :Scope: Global
   :Dynamic: No
   :Option: :option:`--preload-buffer-size`

.. _drizzled_pseudo_thread_id:

* ``pseudo_thread_id``

   :Scope: Global
   :Dynamic: No
   :Option: 

   Unknown.

.. _drizzled_query_alloc_block_size:

* ``query_alloc_block_size``

   :Scope: Global
   :Dynamic: No
   :Option: :option:`--query-alloc-block-size`

.. _drizzled_query_prealloc_size:

* ``query_prealloc_size``

   :Scope: Global
   :Dynamic: No
   :Option: :option:`--query-prealloc-size`

.. _drizzled_range_alloc_block_size:

* ``range_alloc_block_size``

   :Scope: Global
   :Dynamic: No
   :Option: :option:`--range-alloc-block-size`

.. _drizzled_read_buffer_size:

* ``read_buffer_size``

   :Scope: Global
   :Dynamic: No
   :Option: :option:`--read-buffer-size`

.. _drizzled_read_rnd_buffer_size:

* ``read_rnd_buffer_size``

   :Scope: Global
   :Dynamic: No
   :Option: :option:`--read-rnd-buffer-size`

.. _drizzled_replicate_query:

* ``replicate_query``

   :Scope: Global
   :Dynamic: No
   :Option: :option:`--replicate-query`

.. _drizzled_scheduler:

* ``scheduler``

   :Scope: Global
   :Dynamic: No
   :Option: :option:`--scheduler`

.. _drizzled_secure_file_priv:

* ``secure_file_priv``

   :Scope: Global
   :Dynamic: No
   :Option: :option:`--secure-file-priv`

.. _drizzled_server_id:

* ``server_id``

   :Scope: Global
   :Dynamic: No
   :Option: :option:`--server-id`

.. _drizzled_server_uuid:

* ``server_uuid``

   :Scope: Global
   :Dynamic: No
   :Option: 

   Server UUID.

.. _drizzled_sort_buffer_size:

* ``sort_buffer_size``

   :Scope: Global
   :Dynamic: No
   :Option: :option:`--sort-buffer-size`

.. _drizzled_sql_big_selects:

* ``sql_big_selects``

   :Scope: Global
   :Dynamic: No
   :Option: 

   Unknown.

.. _drizzled_sql_buffer_result:

* ``sql_buffer_result``

   :Scope: Global
   :Dynamic: No
   :Option: 

   Unknown.

.. _drizzled_sql_notes:

* ``sql_notes``

   :Scope: Global
   :Dynamic: No
   :Option: 

   Unknown.

.. _drizzled_sql_select_limit:

* ``sql_select_limit``

   :Scope: Global
   :Dynamic: No
   :Option: 

   Unknown.

.. _drizzled_sql_warnings:

* ``sql_warnings``

   :Scope: Global
   :Dynamic: No
   :Option: 

   Unknown.

.. _drizzled_storage_engine:

* ``storage_engine``

   :Scope: Global
   :Dynamic: No
   :Option: :option:`--default-storage-engine`

.. _drizzled_table_definition_cache:

* ``table_definition_cache``

   :Scope: Global
   :Dynamic: No
   :Option: :option:`--table-definition-cache`

.. _drizzled_table_lock_wait_timeout:

* ``table_lock_wait_timeout``

   :Scope: Global
   :Dynamic: No
   :Option: :option:`--table-lock-wait-timeout`

.. _drizzled_table_open_cache:

* ``table_open_cache``

   :Scope: Global
   :Dynamic: No
   :Option: :option:`--table-open-cache`

.. _drizzled_thread_stack:

* ``thread_stack``

   :Scope: Global
   :Dynamic: No
   :Option: :option:`--thread-stack`

.. _drizzled_timed_mutexes:

* ``timed_mutexes``

   :Scope: Global
   :Dynamic: No
   :Option: :option:`--timed-mutexes`

.. _drizzled_timestamp:

* ``timestamp``

   :Scope: Global
   :Dynamic: No
   :Option: 

   Current UNIX timestamp.

.. _drizzled_tmp_table_size:

* ``tmp_table_size``

   :Scope: Global
   :Dynamic: No
   :Option: :option:`--tmp-table-size`

.. _drizzled_tmpdir:

* ``tmpdir``

   :Scope: Global
   :Dynamic: No
   :Option: :option:`--tmpdir`

.. _drizzled_transaction_message_threshold:

* ``transaction_message_threshold``

   :Scope: Global
   :Dynamic: No
   :Option: :option:`--transaction-message-threshold`

.. _drizzled_tx_isolation:

* ``tx_isolation``

   :Scope: Global
   :Dynamic: No 
   :Option: :option:`--transaction-isolation`

.. _drizzled_unique_checks:

* ``unique_checks``

   :Scope: Global
   :Dynamic: No
   :Option: 

   Check UNIQUE indexes for uniqueness.

.. _drizzled_vc_branch:

* ``vc_branch``

   :Scope: Global
   :Dynamic: No
   :Option: 

   Version control (Bazaar) branch.

.. _drizzled_vc_release_id:

* ``vc_release_id``

   :Scope: Global
   :Dynamic: No
   :Option: 

   Version control (Bazaar) release id.

.. _drizzled_vc_revid:

* ``vc_revid``

   :Scope: Global
   :Dynamic: No
   :Option: 

   Version control (Bazaar) revision id.

.. _drizzled_vc_revno:

* ``vc_revno``

   :Scope: Global
   :Dynamic: No
   :Option: 

   Version control (Bazaar) revision number.

.. _drizzled_version:

* ``version``

   :Scope: Global
   :Dynamic: No
   :Option: :option:`--version`

   Drizzle version.

.. _drizzled_version_comment:

* ``version_comment``

   :Scope: Global
   :Dynamic: No
   :Option: 

   Version comment.

.. _drizzled_version_compile_machine:

* ``version_compile_machine``

   :Scope: Global
   :Dynamic: No
   :Option: 

   Version compile for machine type.

.. _drizzled_version_compile_os:

* ``version_compile_os``

   :Scope: Global
   :Dynamic: No
   :Option: 

   Version compile for OS.

.. _drizzled_version_compile_vendor:

* ``version_compile_vendor``

   :Scope: Global
   :Dynamic: No
   :Option: 

   Version compile for OS vendor.

.. _drizzled_warning_count:

* ``warning_count``

   :Scope: Global
   :Dynamic: No
   :Option: 

   Unknown.
