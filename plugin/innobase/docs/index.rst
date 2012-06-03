.. _innobase_plugin:

InnoDB Storage Engine
=====================

The :program:`innodb` plugin provides the InnoDB storage engine,
a fully transactional MVCC storage engine developed by
`Innobase Oy <http://www.innodb.com/>`_.  InnoDB is the default
storage engine for Drizzle. 


.. _innodb_compatibility_with_mysql:

Compatibility with MySQL
------------------------

The :program:`innodb` plugin is maintained in Drizzle as a downstream
project of the :program:`innodb_plugin` for MySQL.  The two are nearly
identical, and the Drizzle plugin is usually kept up-to-date with the MySQL
plugin, but there are some notable differences:

  * The on disk formats are slightly incompatible (to allow for the same
    index length for the four byte UTF-8 that Drizzle supports)
  * The table definitions (.from for MySQL, .dfe for Drizzle) are completely
    different. This means that you cannot directly share InnoDB tablespaces
    between MySQL and Drizzle. Use the drizzledump tool to migrate data from
    MySQL to Drizzle.

There are also some notable compatibilies:

  * AUTO_INCREMENT behaves the standard way (as in MyISAM)
  * Supports four byte UTF-8 with the same index size

.. _innodb_native_aio_support:

Native AIO Support
------------------

InnoDB supports Linux native AIO when compiled on platforms that have the
libaio development files installed (typically a package called libaio-dev or
libaio-devel).  For more information on the advantages of this please see
http://blogs.innodb.com/wp/2010/04/innodb-performance-aio-linux/

To confirm that Linux native AIO is enabled, execute:

.. code-block:: mysql

  SHOW GLOBAL VARIABLES LIKE 'innodb_use_native_aio';

.. _innodb_transaction_log:

InnoDB Replicaiton Log
----------------------

The ``innodb`` plugin provides a mechanism to store replication
events in an InnoDB table. When enabled, this transaction log can be accessed
through the SYS_REPLICATION_LOG and INNODB_REPLICATION_LOG tables in the
DATA_DICTIONARY schema.

To enable this transaction log, start the server with the 
:option:`drizzled --innodb.replication-log`.

Loading
-------

This plugin is loaded by default, but it may need to be configured.  See
the plugin's :ref:`innodb_configuration` and
:ref:`innodb_variables`.

To stop the plugin from loading by default, start :program:`drizzled`
with::

   --plugin-remove=innodb

.. seealso:: :ref:`drizzled_plugin_options` for more information about adding and removing plugins.

.. _innodb_configuration:

Configuration
-------------

These command line options configure the plugin when :program:`drizzled`
is started.  See :ref:`command_line_options` for more information about specifying
command line options.

.. program:: drizzled

.. option:: --innodb.adaptive-flushing-method ARG

   :Default: estimate
   :Variable: :ref:`innodb_adaptive_flushing_method <innodb_adaptive_flushing_method>`

   Adaptive flushing method.  Possible values are:

   * native
   * estimate
   * keep_average

.. option:: --innodb.additional-mem-pool-size ARG

   :Default: 8388608 (8M)
   :Variable: :ref:`innodb_additional_mem_pool_size <innodb_additional_mem_pool_size>`

   Size of a memory pool InnoDB uses to store data dictionary information and other internal data structures.

.. option:: --innodb.auto-lru-dump 

   :Default: 0
   :Variable: :ref:`innodb_auto_lru_dump <innodb_auto_lru_dump>`

   Time in seconds between automatic buffer pool dumps. 

.. option:: --innodb.autoextend-increment ARG

   :Default: 64
   :Variable: :ref:`innodb_autoextend_increment <innodb_autoextend_increment>`

   Data file autoextend increment in megabytes.

.. option:: --innodb.buffer-pool-instances ARG

   :Default: 1
   :Variable:

   Number of buffer pool instances.

.. option:: --innodb.buffer-pool-size ARG

   :Default: 134217728 (128M)
   :Variable: :ref:`innodb_buffer_pool_size <innodb_buffer_pool_size>`

   The size of the memory buffer InnoDB uses to cache data and indexes of its tables.

.. option:: --innodb.change-buffering 

   :Default: all
   :Variable: :ref:`innodb_change_buffering <innodb_change_buffering>`

   Buffer changes to reduce random access.  Possible values:

   * none
   * inserts
   * deletes
   * changes
   * purges
   * all

.. option:: --innodb.checkpoint-age-target 

   :Default: 0
   :Variable: :ref:`innodb_checkpoint_age_target <innodb_checkpoint_age_target>`

   Control soft limit of checkpoint age. (0 = no control)

.. option:: --innodb.commit-concurrency 

   :Default: 0
   :Variable: :ref:`innodb_commit_concurrency <innodb_commit_concurrency>`

   Helps in performance tuning in heavily concurrent environments.

.. option:: --innodb.concurrency-tickets ARG

   :Default: 500
   :Variable: :ref:`innodb_concurrency_tickets <innodb_concurrency_tickets>`

   Number of times a thread is allowed to enter InnoDB within the same SQL query after it has once got the ticket.

.. option:: --innodb.data-file-path 

   :Default: ibdata1:10M:autoextend
   :Variable: :ref:`innodb_data_file_path <innodb_data_file_path>`

   Path to individual files and their sizes.

.. option:: --innodb.data-home-dir 

   :Default: 
   :Variable: :ref:`innodb_data_home_dir <innodb_data_home_dir>`

   Directory for InnoDB data.

.. option:: --innodb.disable-adaptive-flushing 

   :Default: 
   :Variable: :ref:`innodb_adaptive_flushing <innodb_adaptive_flushing>`

   Do not attempt flushing dirty pages to avoid IO bursts at checkpoints.

.. option:: --innodb.disable-adaptive-hash-index 

   :Default: 
   :Variable: :ref:`innodb_adaptive_hash_index <innodb_adaptive_hash_index>`

   Enable InnoDB adaptive hash index (enabled by default)

.. option:: --innodb.disable-checksums 

   :Default: false
   :Variable: :ref:`innodb_checksums <innodb_checksums>`

   Disable InnoDB checksums validation.

.. option:: --innodb.disable-doublewrite 

   :Default: 
   :Variable: :ref:`innodb_doublewrite <innodb_doublewrite>`

   Disable InnoDB doublewrite buffer.

.. option:: --innodb.disable-native-aio 

   :Default: 
   :Variable:

   Do not use Native AIO library for IO, even if available.
   See :ref:`innodb_native_aio_support`.

.. option:: --innodb.disable-stats-on-metadata 

   :Default: 
   :Variable:

   Disable statistics gathering for metadata commands such as SHOW TABLE STATUS (on by default).

.. option:: --innodb.disable-table-locks 

   :Default: 
   :Variable:

   Disable InnoDB locking in LOCK TABLES.

.. option:: --innodb.disable-xa 

   :Default: 
   :Variable: :ref:`innodb_support_xa <innodb_support_xa>`

   Disable InnoDB support for the XA two-phase commit.

.. option:: --innodb.fast-shutdown ARG

   :Default: 1
   :Variable: :ref:`innodb_fast_shutdown <innodb_fast_shutdown>`

   Speeds up the shutdown process of the InnoDB storage engine. Possible values are:

   * 0 (off)
   * 1 (faster)
   * 2 (fastest, crash-like)

.. option:: --innodb.file-format ARG

   :Default: Antelope
   :Variable: :ref:`innodb_file_format <innodb_file_format>`

   File format to use for new tables in .ibd files.

.. option:: --innodb.file-format-check 

   :Default: true
   :Variable: :ref:`innodb_file_format_check <innodb_file_format_check>`

   Whether to perform system file format check.

.. option:: --innodb.file-format-max ARG

   :Default: Antelope
   :Variable: :ref:`innodb_file_format_max <innodb_file_format_max>`

   The highest file format in the tablespace.

.. option:: --innodb.file-per-table 

   :Default: false
   :Variable: :ref:`innodb_file_per_table <innodb_file_per_table>`

   Stores each InnoDB table to an .ibd file in the database dir.

.. option:: --innodb.flush-log-at-trx-commit ARG

   :Default: 1
   :Variable: :ref:`innodb_flush_log_at_trx_commit <innodb_flush_log_at_trx_commit>`

   Flush lot at transaction commit.  Possible values are:

   * 0 (write and flush once per second)
   * 1 (write and flush at each commit)
   * 2 (write at commit, flush once per second)

.. option:: --innodb.flush-method 

   :Default: 
   :Variable: :ref:`innodb_flush_method <innodb_flush_method>`

   Data flush method.

.. option:: --innodb.flush-neighbor-pages ARG

   :Default: 1
   :Variable: :ref:`innodb_flush_neighbor_pages <innodb_flush_neighbor_pages>`

   Enable/Disable flushing also neighbor pages. 0:disable 1:enable

.. option:: --innodb.force-recovery 

   :Default: 0
   :Variable: :ref:`innodb_force_recovery <innodb_force_recovery>`

   Helps to save your data in case the disk image of the database becomes corrupt.

.. option:: --innodb.ibuf-accel-rate ARG

   :Default: 100
   :Variable: :ref:`innodb_ibuf_accel_rate <innodb_ibuf_accel_rate>`

   Tunes amount of insert buffer processing of background, in addition to innodb_io_capacity. (in percentage)

.. option:: --innodb.ibuf-active-contract ARG

   :Default: 1
   :Variable: :ref:`innodb_ibuf_active_contract <innodb_ibuf_active_contract>`

   Enable/Disable active_contract of insert buffer. 0:disable 1:enable

.. option:: --innodb.ibuf-max-size ARG

   :Default: UINT64_MAX
   :Variable: :ref:`innodb_ibuf_max_size <innodb_ibuf_max_size>`

   The maximum size of the insert buffer (in bytes).

.. option:: --innodb.io-capacity ARG

   :Default: 200
   :Variable: :ref:`innodb_io_capacity <innodb_io_capacity>`

   Number of IOPs the server can do. Tunes the background IO rate.

.. option:: --innodb.lock-wait-timeout ARG

   :Default: 50
   :Variable: :ref:`innodb_lock_wait_timeout <innodb_lock_wait_timeout>`

   Timeout in seconds an InnoDB transaction may wait for a lock before being rolled back. Values above 100000000 disable the timeout.

.. option:: --innodb.log-buffer-size ARG

   :Default: 8,388,608 (8M)
   :Variable: :ref:`innodb_log_buffer_size <innodb_log_buffer_size>`

   The size of the buffer which InnoDB uses to write log to the log files on disk.

.. option:: --innodb.log-file-size ARG

   :Default: 20971520 (20M)
   :Variable: :ref:`innodb_log_file_size <innodb_log_file_size>`

   The size of the buffer which InnoDB uses to write log to the log files on disk.

.. option:: --innodb.log-files-in-group ARG

   :Default: 2
   :Variable: :ref:`innodb_log_files_in_group <innodb_log_files_in_group>`

   Number of log files in the log group. InnoDB writes to the files in a circular fashion.

.. option:: --innodb.log-group-home-dir 

   :Default: 
   :Variable: :ref:`innodb_log_group_home_dir <innodb_log_group_home_dir>`

   Path to InnoDB log files.

.. option:: --innodb.max-dirty-pages-pct ARG

   :Default: 75
   :Variable: :ref:`innodb_max_dirty_pages_pct <innodb_max_dirty_pages_pct>`

   Percentage of dirty pages allowed in bufferpool.

.. option:: --innodb.max-purge-lag 

   :Default: 0
   :Variable: :ref:`innodb_max_purge_lag <innodb_max_purge_lag>`

   Desired maximum length of the purge queue (0 = no limit)

.. option:: --innodb.mirrored-log-groups ARG

   :Default: 1
   :Variable: :ref:`innodb_mirrored_log_groups <innodb_mirrored_log_groups>`

   Number of identical copies of log groups we keep for the database. Currently this should be set to 1.

.. option:: --innodb.old-blocks-pct ARG

   :Default: 37
   :Variable: :ref:`innodb_old_blocks_pct <innodb_old_blocks_pct>`

   Percentage of the buffer pool to reserve for 'old' blocks.

.. option:: --innodb.old-blocks-time 

   :Default: 0
   :Variable: :ref:`innodb_old_blocks_time <innodb_old_blocks_time>`

   Move blocks to the 'new' end of the buffer pool if the first access
   was at least this many milliseconds ago.

.. option:: --innodb.open-files ARG

   :Default: 300
   :Variable: :ref:`innodb_open_files <innodb_open_files>`

   How many files at the maximum InnoDB keeps open at the same time.

.. option:: --innodb.purge-batch-size ARG

   :Default: 20
   :Variable: :ref:`innodb_purge_batch_size <innodb_purge_batch_size>`

   Number of UNDO logs to purge in one batch from the history list. 

.. option:: --innodb.purge-threads ARG

   :Default: 1
   :Variable: :ref:`innodb_purge_threads <innodb_purge_threads>`

   Purge threads can be either 0 or 1.

.. option:: --innodb.read-ahead ARG

   :Default: linear
   :Variable: :ref:`innodb_read_ahead <innodb_read_ahead>`

   Control read ahead activity.  Possible values are:

   * none
   * random
   * linear
   * both

.. option:: --innodb.read-ahead-threshold ARG

   :Default: 56
   :Variable: :ref:`innodb_read_ahead_threshold <innodb_read_ahead_threshold>`

   Number of pages that must be accessed sequentially for InnoDB to trigger a readahead.

.. option:: --innodb.read-io-threads ARG

   :Default: 4
   :Variable: :ref:`innodb_read_io_threads <innodb_read_io_threads>`

   Number of background read I/O threads in InnoDB.

.. option:: --innodb.replication-delay 

   :Default: 0
   :Variable: :ref:`innodb_replication_delay <innodb_replication_delay>`

   Replication thread delay (ms) on the slave server if innodb_thread_concurrency is reached (0 by default)

.. option:: --innodb.replication-log 

   :Default: false
   :Variable: :ref:`innodb_replication_log <innodb_replication_log>`

   Enable :ref:`innodb_transaction_log`.

.. option:: --innodb.spin-wait-delay ARG

   :Default: 6
   :Variable: :ref:`innodb_spin_wait_delay <innodb_spin_wait_delay>`

   Maximum delay between polling for a spin lock.

.. option:: --innodb.stats-sample-pages ARG

   :Default: 8
   :Variable: :ref:`innodb_stats_sample_pages <innodb_stats_sample_pages>`

   The number of index pages to sample when calculating statistics.

.. option:: --innodb.status-file 

   :Default: false
   :Variable: :ref:`innodb_status_file <innodb_status_file>`

   Enable SHOW INNODB STATUS output in the innodb_status.<pid> file.

.. option:: --innodb.strict-mode 

   :Default: false
   :Variable: :ref:`innodb_strict_mode <innodb_strict_mode>`

   Use strict mode when evaluating create options.

.. option:: --innodb.sync-spin-loops ARG

   :Default: 30
   :Variable: :ref:`innodb_sync_spin_loops <innodb_sync_spin_loops>`

   Count of spin-loop rounds in InnoDB mutexes.

.. option:: --innodb.thread-concurrency 

   :Default: 0
   :Variable: :ref:`innodb_thread_concurrency <innodb_thread_concurrency>`

   Helps in performance tuning in heavily concurrent environments. Sets the maximum number of threads allowed inside InnoDB. Value 0 will disable the thread throttling.

.. option:: --innodb.thread-sleep-delay ARG

   :Default: 10000
   :Variable: :ref:`innodb_thread_sleep_delay <innodb_thread_sleep_delay>`

   Time of innodb thread sleeping before joining InnoDB queue (usec). Value 0 disable a sleep.

.. option:: --innodb.use-internal-malloc 

   :Default: false
   :Variable: `innodb_use_sys_malloc <innodb_use_sys_malloc>`

   Use InnoDB's internal memory allocator instead of the system's malloc.

.. option:: --innodb.use-replicator

   :Default: default
   :Variable: `innodb_use_replicator <innodb_use_sys_malloc>`

   Use this replicator for the :ref:`innodb_transaction_log`.

.. option:: --innodb.version ARG

   :Default:
   :Variable: :ref:`innodb_version_var <innodb_version_var>`

   InnoDB version.

.. option:: --innodb.write-io-threads ARG

   :Default: 4
   :Variable: :ref:`innodb_write_io_threads <innodb_write_io_threads>`

   Number of background write I/O threads in InnoDB.

.. _innodb_variables:

Variables
---------

These variables show the running configuration of the plugin.
See `variables` for more information about querying and setting variables.

.. _innodb_adaptive_flushing:

* ``innodb_adaptive_flushing``

   :Scope: Global
   :Dynamic: No
   :Option: :option:`--innodb.disable-adaptive-flushing`

   If adaptive flushing is enabled or not.

.. _innodb_adaptive_flushing_method:

* ``innodb_adaptive_flushing_method``

   :Scope: Global
   :Dynamic: No
   :Option: :option:`--innodb.adaptive-flushing-method`

   Adaptive flushing method.  Possible values are:

   * native
   * estimate
   * keep_average

.. _innodb_adaptive_hash_index:

* ``innodb_adaptive_hash_index``

   :Scope: Global
   :Dynamic: No
   :Option: :option:`--innodb.disable-adaptive-hash-index`

   If the adapative hash index is enabled or not.

.. _innodb_additional_mem_pool_size:

* ``innodb_additional_mem_pool_size``

   :Scope: Global
   :Dynamic: No
   :Option: :option:`--innodb.additional-mem-pool-size`

   Size of a memory pool InnoDB uses to store data dictionary information and other internal data structures.

.. _innodb_auto_lru_dump:

* ``innodb_auto_lru_dump``

   :Scope: Global
   :Dynamic: No
   :Option: :option:`--innodb.auto-lru-dump`

   Time in seconds between automatic buffer pool dumps. 

.. _innodb_autoextend_increment:

* ``innodb_autoextend_increment``

   :Scope: Global
   :Dynamic: No
   :Option: :option:`--innodb.autoextend-increment`

   Data file autoextend increment in megabytes

.. _innodb_buffer_pool_size:

* ``innodb_buffer_pool_size``

   :Scope: Global
   :Dynamic: No
   :Option: :option:`--innodb.buffer-pool-size`

   The size of the memory buffer InnoDB uses to cache data and indexes of its tables.

.. _innodb_change_buffering:

* ``innodb_change_buffering``

   :Scope: Global
   :Dynamic: No
   :Option: :option:`--innodb.change-buffering`

   Buffer changes to reduce random access: OFF

.. _innodb_checkpoint_age_target:

* ``innodb_checkpoint_age_target``

   :Scope: Global
   :Dynamic: No
   :Option: :option:`--innodb.checkpoint-age-target`

   Control soft limit of checkpoint age. (0 : not control)

.. _innodb_checksums:

* ``innodb_checksums``

   :Scope: Global
   :Dynamic: No
   :Option: :option:`--innodb.disable-checksums`

   If checksums are enabled or not.

.. _innodb_commit_concurrency:

* ``innodb_commit_concurrency``

   :Scope: Global
   :Dynamic: No
   :Option: :option:`--innodb.commit-concurrency`

   Helps in performance tuning in heavily concurrent environments.

.. _innodb_concurrency_tickets:

* ``innodb_concurrency_tickets``

   :Scope: Global
   :Dynamic: No
   :Option: :option:`--innodb.concurrency-tickets`

   Number of times a thread is allowed to enter InnoDB within the same SQL query after it has once got the ticket

.. _innodb_data_file_path:

* ``innodb_data_file_path``

   :Scope: Global
   :Dynamic: No
   :Option: :option:`--innodb.data-file-path`

   Path to individual files and their sizes.

.. _innodb_data_home_dir:

* ``innodb_data_home_dir``

   :Scope: Global
   :Dynamic: No
   :Option: :option:`--innodb.data-home-dir`

   Directory for InnoDB data.

.. _innodb_doublewrite:

* ``innodb_doublewrite``

   :Scope: Global
   :Dynamic: No
   :Option: :option:`--innodb.disable-doublewrite`

   If doublewrite buffer is enabled or not.

.. _innodb_fast_shutdown:

* ``innodb_fast_shutdown``

   :Scope: Global
   :Dynamic: No
   :Option: :option:`--innodb.fast-shutdown`

   Fast shutdown method.

.. _innodb_file_format_check:

* ``innodb_file_format_check``

   :Scope: Global
   :Dynamic: No
   :Option: :option:`--innodb.file-format-check`

   Whether to perform system file format check.

.. _innodb_file_per_table:

* ``innodb_file_per_table``

   :Scope: Global
   :Dynamic: No
   :Option: :option:`--innodb.file-per-table`

   Stores each InnoDB table to an .ibd file in the database dir.

.. _innodb_file_format:

* ``innodb_file_format``

   :Scope: Global
   :Dynamic: No
   :Option: :option:`--innodb.file-format`

   File format to use for new tables in .ibd files.

.. _innodb_file_format_max:

* ``innodb_file_format_max``

   :Scope: Global
   :Dynamic: No
   :Option: :option:`--innodb.file-format-max`

   The highest file format in the tablespace.

.. _innodb_flush_method:

* ``innodb_flush_method``

   :Scope: Global
   :Dynamic: No
   :Option: :option:`--innodb.flush-method`

   Data flush method.

.. _innodb_flush_log_at_trx_commit:

* ``innodb_flush_log_at_trx_commit``

   :Scope: Global
   :Dynamic: No
   :Option: :option:`--innodb.flush-log-at-trx-commit`

   Set to 0 (write and flush once per second)

.. _innodb_flush_neighbor_pages:

* ``innodb_flush_neighbor_pages``

   :Scope: Global
   :Dynamic: No
   :Option: :option:`--innodb.flush-neighbor-pages`

   Enable/Disable flushing also neighbor pages. 0:disable 1:enable

.. _innodb_force_recovery:

* ``innodb_force_recovery``

   :Scope: Global
   :Dynamic: No
   :Option: :option:`--innodb.force-recovery`

   Helps to save your data in case the disk image of the database becomes corrupt.

.. _innodb_ibuf_accel_rate:

* ``innodb_ibuf_accel_rate``

   :Scope: Global
   :Dynamic: No
   :Option: :option:`--innodb.ibuf-accel-rate`

   Tunes amount of insert buffer processing of background

.. _innodb_ibuf_active_contract:

* ``innodb_ibuf_active_contract``

   :Scope: Global
   :Dynamic: No
   :Option: :option:`--innodb.ibuf-active-contract`

   Enable/Disable active_contract of insert buffer. 0:disable 1:enable

.. _innodb_ibuf_max_size:

* ``innodb_ibuf_max_size``

   :Scope: Global
   :Dynamic: No
   :Option: :option:`--innodb.ibuf-max-size`

   The maximum size of the insert buffer (in bytes).

.. _innodb_io_capacity:

* ``innodb_io_capacity``

   :Scope: Global
   :Dynamic: No
   :Option: :option:`--innodb.io-capacity`

   Number of IOPs the server can do. Tunes the background IO rate

.. _innodb_lock_wait_timeout:

* ``innodb_lock_wait_timeout``

   :Scope: Global
   :Dynamic: No
   :Option: :option:`--innodb.lock-wait-timeout`

   Timeout in seconds an InnoDB transaction may wait for a lock before being rolled back. Values above 100000000 disable the timeout.

.. _innodb_log_group_home_dir:

* ``innodb_log_group_home_dir``

   :Scope: Global
   :Dynamic: No
   :Option: :option:`--innodb.log-group-home-dir`

   Path to InnoDB log files.

.. _innodb_log_buffer_size:

* ``innodb_log_buffer_size``

   :Scope: Global
   :Dynamic: No
   :Option: :option:`--innodb.log-buffer-size`

   The size of the buffer which InnoDB uses to write log to the log files on disk.

.. _innodb_log_file_size:

* ``innodb_log_file_size``

   :Scope: Global
   :Dynamic: No
   :Option: :option:`--innodb.log-file-size`

   The size of the buffer which InnoDB uses to write log to the log files on disk.

.. _innodb_log_files_in_group:

* ``innodb_log_files_in_group``

   :Scope: Global
   :Dynamic: No
   :Option: :option:`--innodb.log-files-in-group`

   Number of log files in the log group. InnoDB writes to the files in a circular fashion.

.. _innodb_max_dirty_pages_pct:

* ``innodb_max_dirty_pages_pct``

   :Scope: Global
   :Dynamic: No
   :Option: :option:`--innodb.max-dirty-pages-pct`

   Percentage of dirty pages allowed in bufferpool.

.. _innodb_max_purge_lag:

* ``innodb_max_purge_lag``

   :Scope: Global
   :Dynamic: No
   :Option: :option:`--innodb.max-purge-lag`

   Desired maximum length of the purge queue (0 = no limit)

.. _innodb_mirrored_log_groups:

* ``innodb_mirrored_log_groups``

   :Scope: Global
   :Dynamic: No
   :Option: :option:`--innodb.mirrored-log-groups`

   Number of identical copies of log groups we keep for the database. Currently this should be set to 1.

.. _innodb_old_blocks_pct:

* ``innodb_old_blocks_pct``

   :Scope: Global
   :Dynamic: No
   :Option: :option:`--innodb.old-blocks-pct`

   Percentage of the buffer pool to reserve for 'old' blocks.

.. _innodb_old_blocks_time:

* ``innodb_old_blocks_time``

   :Scope: Global
   :Dynamic: No
   :Option: :option:`--innodb.old-blocks-time`

   ove blocks to the 'new' end of the buffer pool if the first access

.. _innodb_open_files:

* ``innodb_open_files``

   :Scope: Global
   :Dynamic: No
   :Option: :option:`--innodb.open-files`

   How many files at the maximum InnoDB keeps open at the same time.

.. _innodb_purge_batch_size:

* ``innodb_purge_batch_size``

   :Scope: Global
   :Dynamic: No
   :Option: :option:`--innodb.purge-batch-size`

   Number of UNDO logs to purge in one batch from the history list. 

.. _innodb_purge_threads:

* ``innodb_purge_threads``

   :Scope: Global
   :Dynamic: No
   :Option: :option:`--innodb.purge-threads`

   Purge threads can be either 0 or 1. Default is 1.

.. _innodb_read_ahead:

* ``innodb_read_ahead``

   :Scope: Global
   :Dynamic: No
   :Option: :option:`--innodb.read-ahead`

   Readahead method.

.. _innodb_read_ahead_threshold:

* ``innodb_read_ahead_threshold``

   :Scope: Global
   :Dynamic: No
   :Option: :option:`--innodb.read-ahead-threshold`

   Number of pages that must be accessed sequentially for InnoDB to trigger a readahead.

.. _innodb_read_io_threads:

* ``innodb_read_io_threads``

   :Scope: Global
   :Dynamic: No
   :Option: :option:`--innodb.read-io-threads`

   Number of background read I/O threads in InnoDB.

.. _innodb_replication_delay:

* ``innodb_replication_delay``

   :Scope: Global
   :Dynamic: No
   :Option: :option:`--innodb.replication-delay`

   Replication thread delay (ms) on the slave server if innodb_thread_concurrency is reached (0 by default)

.. _innodb_replication_log:

* ``innodb_replication_log``

   :Scope: Global
   :Dynamic: No
   :Option: :option:`--innodb.replication-log`

   If the :ref:`innodb_transaction_log` is enabled or not.

.. _innodb_spin_wait_delay:

* ``innodb_spin_wait_delay``

   :Scope: Global
   :Dynamic: No
   :Option: :option:`--innodb.spin-wait-delay`

   Maximum delay between polling for a spin lock (6 by default)

.. _innodb_stats_sample_pages:

* ``innodb_stats_sample_pages``

   :Scope: Global
   :Dynamic: No
   :Option: :option:`--innodb.stats-sample-pages`

   The number of index pages to sample when calculating statistics (default 8)

.. _innodb_status_file:

* ``innodb_status_file``

   :Scope: Global
   :Dynamic: No
   :Option: :option:`--innodb.status-file`

   Enable SHOW INNODB STATUS output in the innodb_status.<pid> file

.. _innodb_strict_mode:

* ``innodb_strict_mode``

   :Scope: Global
   :Dynamic: No
   :Option: :option:`--innodb.strict-mode`

   Use strict mode when evaluating create options.

.. _innodb_support_xa:

* ``innodb_support_xa``

   :Scope: Global
   :Dynamic: No
   :Option: :option:`--innodb.disable-xa`

   If two-phase XA commit it enabled or not.

.. _innodb_sync_spin_loops:

* ``innodb_sync_spin_loops``

   :Scope: Global
   :Dynamic: No
   :Option: :option:`--innodb.sync-spin-loops`

   Count of spin-loop rounds in InnoDB mutexes (30 by default)

.. _innodb_thread_concurrency:

* ``innodb_thread_concurrency``

   :Scope: Global
   :Dynamic: No
   :Option: :option:`--innodb.thread-concurrency`

   Helps in performance tuning in heavily concurrent environments. Sets the maximum number of threads allowed inside InnoDB. Value 0 will disable the thread throttling.

.. _innodb_thread_sleep_delay:

* ``innodb_thread_sleep_delay``

   :Scope: Global
   :Dynamic: No
   :Option: :option:`--innodb.thread-sleep-delay`

   Time of innodb thread sleeping before joining InnoDB queue (usec). Value 0 disable a sleep

.. _innodb_use_native_aio:

* ``innodb_use_native_aio``

   :Scope: Global
   :Dynamic: No
   :Option: :option:`--innodb.disable-native-aio`

   If :ref:`innodb_native_aio_support` enabled or not.

.. _innodb_use_sys_malloc:

* ``innodb_use_sys_malloc``

   :Scope: Global
   :Dynamic: No
   :Option: :option:`--innodb.use-internal-malloc`

   If system or internal malloc() is being used.

.. _innodb_use_replicator:

* ``innodb_use_replicator``

   :Scope: Global
   :Dynamic: No
   :Option: :option:`--innodb.use-replicator`

   Replicator to which the :ref:`innodb_transaction_log` is paired.

.. _innodb_version_var:

* ``innodb_version``

   :Scope: Global
   :Dynamic: No
   :Option: :option:`--innodb.version`

   InnoDB version

.. _innodb_write_io_threads:

* ``innodb_write_io_threads``

   :Scope: Global
   :Dynamic: No
   :Option: :option:`--innodb.write-io-threads`

   Number of background write I/O threads in InnoDB.

.. _innodb_authors:

Authors
-------

`Innobase Oy <http://www.innodb.com/>`_

.. _innodb_version:

Version
-------

This documentation applies to **innodb 1.1.4**.

To see which version of the plugin a Drizzle server is running, execute:

.. code-block:: mysql

   SELECT MODULE_VERSION FROM DATA_DICTIONARY.MODULES WHERE MODULE_NAME='innodb'

Changelog
---------

v1.1.4
^^^^^^
* First Drizzle version.
