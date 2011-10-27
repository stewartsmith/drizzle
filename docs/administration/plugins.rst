.. program:: drizzled

Plugins
=======

Plugins provide the majority of Drizzle's features: functions, system tables,
replication, etc.  You should familiarize yourself with the
:ref:`drizzled_plugin_options` and :doc:`/plugins/list`.

.. _default_plugins:

Default Plugins
---------------

Drizzle loads these plugins by default (see :option:`--plugin-load`):

* auth_all (:ref:`auth_all_plugin`)
* ascii (:ref:`ascii_plugin`)
* benchmark (:ref:`benchmark_plugin`) 
* charlength (:ref:`charlength_plugin`) 
* compression (:ref:`compression_plugin`) 
* connection_id (:ref:`connection_id_plugin`) 
* crc32 (:ref:`crc32_plugin`) 
* default_replicator (:ref:`default_replicator_plugin`) 
* drizzle_protocol (:ref:`drizzle_protocol_plugin`)
* errmsg_stderr (:ref:`errmsg_stderr_plugin`) 
* filtered_replicator (:ref:`filtered_replicator_plugin`) 
* hex_functions (:ref:`hex_functions_plugin`) 
* innobase (:ref:`innobase_plugin`)
* length (:ref:`length_plugin`) 
* logging_stats (:ref:`logging_stats_plugin`)
* math_functions (:ref:`math_functions_plugin`)
* md5 (:ref:`md5_plugin`)
* memory (:ref:`memory_plugin`)
* multi_thread (:ref:`multi_thread_plugin`)
* myisam (:ref:`myisam_plugin`)
* mysql_protocol (:ref:`mysql_protocol_plugin`)
* mysql_unix_socket_protocol (:ref:`mysql_unix_socket_protocol_plugin`)
* rand_function (:ref:`rand_function_plugin`)
* reverse_function (:ref:`reverse_function_plugin`)
* sleep (:ref:`sleep_plugin`)
* show_schema_proto (:ref:`show_schema_proto_plugin`)
* substr_functions (:ref:`substr_functions_plugin`)
* syslog (:ref:`syslog_plugin`)
* transaction_log (:ref:`transaction_log_plugin`)
* uuid_function (:ref:`uuid_function_plugin`)
* version (:ref:`version_plugin`)
