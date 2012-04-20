.. program:: drizzled

.. _plugins:

Plugins
=======

Plugins provide the majority of Drizzle's features: functions, system tables,
replication, etc.  You should familiarize yourself with the
:ref:`drizzled_plugin_options` and :doc:`/plugins/list`.

.. _default_plugins:

Default Plugins
---------------

Drizzle loads these plugins by default (see :option:`--plugin-load`):

 * :ref:`ascii_plugin` - Return the ASCII value of a character (ascii)
 * :ref:`auth_all_plugin` - Data Dictionary for utility tables (auth_all)
 * :ref:`benchmark_plugin` - Measure time for repeated calls to a function. (benchmark)
 * :ref:`catalog_plugin` - Basic Catalog functions, data dictionary, and system. (catalog)
 * :ref:`charlength_plugin` - Return the number of characters in a string (charlength)
 * :ref:`collation_dictionary_plugin` - Data Dictionary for schema, table, column, indexes, etc (collation_dictionary)
 * :ref:`compression_plugin` - UDFs for compression functions (compression)
 * :ref:`connection_id_plugin` - Return the current connection_id (connection_id)
 * console_plugin - Console Client (console) (TODO: documentation missing)
 * :ref:`crc32_plugin` - CRC32 Function (crc32)
 * :ref:`default_replicator` - Default Replicator (default_replicator)
 * :ref:`drizzle_protocol_plugin` - Drizzle Protocol (drizzle_protocol)
 * :ref:`errmsg_stderr_plugin` - Error Messages to stderr (errmsg_stderr)
 * :ref:`error_dictionary_plugin` - Data Dictionary for Errors. (error_dictionary)
 * :ref:`function_engine_plugin` - Function Engine provides the infrastructure for Table Functions,etc. (function_engine)
 * :ref:`hex_functions_plugin` - Convert a string to HEX() or from UNHEX() (hex_functions)
 * :ref:`information_schema_dictionary_plugin` - Data Dictionary for ANSI information schema, etc (information_schema_dictionary)
 * :ref:`innobase_plugin` - Supports transactions, row-level locking, and foreign keys (innobase)
 * ipv6_function_plugin - IPV6() function (ipv6_function) (TODO: documentation missing)
 * :ref:`js_plugin` - Execute JavaScript code with supplied arguments (js)
 * :ref:`length_plugin` - Return the byte length of a string (length)
 * :ref:`logging_stats_plugin` - User Statistics as DATA_DICTIONARY tables (logging_stats)
 * :ref:`math_functions_plugin` - Math Functions. (math_functions)
 * :ref:`md5_plugin` - UDF for computing md5sum (md5)
 * :ref:`memory_plugin` - Hash based, stored in memory, useful for temporary tables (memory)
 * :ref:`multi_thread_plugin` - One Thread Per Session Scheduler (multi_thread)
 * :ref:`myisam_plugin` - Default engine as of MySQL 3.23, used for temporary tables (myisam)
 * :ref:`mysql_protocol_plugin` - MySQL Protocol Module (mysql_protocol)
 * :ref:`mysql_unix_socket_protocol_plugin` - MySQL Unix Socket Protocol (mysql_unix_socket_protocol)
 * :ref:`protocol_dictionary_plugin` - Provides dictionary for protocol counters. (protocol_dictionary)
 * :ref:`rand_function_plugin` - RAND Function (rand_function)
 * :ref:`registry_dictionary_plugin` - Provides dictionary for plugin registry system. (registry_dictionary)
 * :ref:`reverse_function_plugin` - reverses a string (reverse_function)
 * :ref:`schema_dictionary_plugin` - Data Dictionary for schema, table, column, indexes, etc (schema_dictionary)
 * :ref:`schema_engine_plugin` - This implements the default file based Schema engine. (schema_engine)
 * :ref:`session_dictionary_plugin` - Dictionary for session information, aka proccesslist, user defined variables, etc. (session_dictionary)
 * :ref:`show_dictionary_plugin` - Dictionary for show commands. (show_dictionary)
 * :ref:`show_schema_proto_plugin` - Shows text representation of schema definition proto (show_schema_proto)
 * :ref:`signal_handler_plugin` - Default Signal Handler (signal_handler)
 * :ref:`sleep_plugin` - SLEEP Function (sleep)
 * :ref:`status_dictionary_plugin` - Dictionary for status, statement, and variable information. (status_dictionary)
 * :ref:`string_functions_plugin` - String Functions. (string_functions)
 * :ref:`substr_functions_plugin` - SUBSTR and SUBSTR (substr_functions)
 * :ref:`syslog_plugin` - Syslog (syslog)
 * :ref:`table_cache_dictionary_plugin` - Data Dictionary for table and table definition cache. (table_cache_dictionary)
 * :ref:`user_locks_plugin` - User level locking and barrier functions (user_locks)
 * :ref:`utility_functions_plugin` - Utility Functions. (utility_functions)
 * :ref:`uuid_function_plugin` - UUID() function using libuuid (uuid_function)
 * :ref:`version_plugin` - Print Drizzle version (version)

You can list information about the loaded plugins with:

.. code-block:: mysql

    SELECT * FROM DATA_DICTIONARY.MODULES;

Note to editors: The above list of functions can be regenerated at any time with:

.. code-block:: mysql

    drizzle --silent -e "SELECT CONCAT(' * ', ':ref:\`', MODULE_LIBRARY, '_plugin\` - ', MODULE_DESCRIPTION, ' (', MODULE_LIBRARY, ')') 
    FROM DATA_DICTIONARY.MODULES ORDER BY MODULE_LIBRARY;"


