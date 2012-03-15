.. _performance_dictionary_plugin:

Performance Dictionary
======================

The :program:`peformance_dictionary` plugin provides the
DATA_DICTIONARY.SESSION_USAGE table.

.. _performance_dictionary_loading:

Loading
-------

To load this plugin, start :program:`drizzled` with::

   --plugin-add=performance_dictionary

.. seealso:: :ref:`drizzled_plugin_options` for more information about adding and removing plugins.

.. _performance_dictionary_examples:

Examples
--------

.. code-block:: mysql

   drizzle> SELECT * FROM DATA_DICTIONARY.SESSION_USAGE\G
   *************************** 1. row ***************************
                              QUERY: select schema_name from information_schema.schemata
             USER_TIME_USED_SECONDS: -1407374883553280
       USER_TIME_USED_MICRO_SECONDS: -21238467
           SYSTEM_TIME_USED_SECONDS: 0
     SYSTEM_TIME_USED_MICRO_SECONDS: -233809327
     INTEGRAL_MAX_RESIDENT_SET_SIZE: 51167232
   INTEGRAL_SHARED_TEXT_MEMORY_SIZE: -4
        INTEGRAL_UNSHARED_DATA_SIZE: -3096224743817216
       INTEGRAL_UNSHARED_STACK_SIZE: -4316623440
                      PAGE_RECLAIMS: 12783
                        PAGE_FAULTS: -1099511627776
                              SWAPS: -1
             BLOCK_INPUT_OPERATIONS: 0
            BLOCK_OUTPUT_OPERATIONS: -3096224743817216
                      MESSAGES_SENT: -4316623425
                  MESSAGES_RECEIVED: 26
                   SIGNALS_RECEIVED: -233832448
         VOLUNTARY_CONTEXT_SWITCHES: -4316635555
       INVOLUNTARY_CONTEXT_SWITCHES: 405

.. _performance_dictionary_authors:

Authors
-------

Brian Aker

.. _performance_dictionary_version:

Version
-------

This documentation applies to **performance_dictionary 1.0**.

To see which version of the plugin a Drizzle server is running, execute:

.. code-block:: mysql

   SELECT MODULE_VERSION FROM DATA_DICTIONARY.MODULES WHERE MODULE_NAME='performance_dictionary'

Changelog
---------

v1.0
^^^^
* First release.
