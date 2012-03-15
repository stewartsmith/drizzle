.. _logging_query_plugin:

Query Logging
=============

The :program:`logging_query` plugin logs queries to a CSV file.

.. _logging_query_loading:

Loading
-------

To load this plugin, start :program:`drizzled` with::

   --plugin-add=logging_query

Loading the plugin may not enable or configure it.  See the plugin's
:ref:`logging_query_configuration` and :ref:`logging_query_variables`.

.. seealso:: :ref:`drizzled_plugin_options` for more information about adding and removing plugins.

.. _logging_query_configuration:

Configuration
-------------

These command line options configure the plugin when :program:`drizzled`
is started.  See :ref:`command_line_options` for more information about specifying
command line options.

.. program:: drizzled

.. option:: --logging-query.enable 

   :Default: false
   :Variable: :ref:`logging_query_enable <logging_query_enable>`

   Enable logging to CSV file.

.. option:: --logging-query.filename 

   :Default: 
   :Variable: :ref:`logging_query_filename <logging_query_filename>`

   File to log to.

.. option:: --logging-query.pcre ARG

   :Default: 
   :Variable: :ref:`logging_query_pcre <logging_query_pcre>`

   PCRE to match the query against.

.. option:: --logging-query.threshold-big-examined 

   :Default: 0
   :Variable: :ref:`logging_query_threshold_big_examined <logging_query_threshold_big_examined>`

   Threshold for logging big queries by rows examined.

.. option:: --logging-query.threshold-big-resultset 

   :Default: 0
   :Variable: :ref:`logging_query_threshold_big_resultset <logging_query_threshold_big_resultset>`

   Threshold for logging big queries by result set size.

.. option:: --logging-query.threshold-slow 

   :Default: 0
   :Variable: :ref:`logging_query_threshold_slow <logging_query_threshold_slow>`

   Threshold for logging slow queries by execution time.

.. _logging_query_variables:

Variables
---------

These variables show the running configuration of the plugin.
See `variables` for more information about querying and setting variables.

.. _logging_query_enable:

* ``logging_query_enable``

   :Scope: Global
   :Dynamic: No
   :Option: :option:`--logging-query.enable`

   Enable logging to CSV file.

.. _logging_query_filename:

* ``logging_query_filename``

   :Scope: Global
   :Dynamic: No
   :Option: :option:`--logging-query.filename`

   File to log to.

.. _logging_query_pcre:

* ``logging_query_pcre``

   :Scope: Global
   :Dynamic: No
   :Option: :option:`--logging-query.pcre`

   PCRE to match the query against.

.. _logging_query_threshold_big_examined:

* ``logging_query_threshold_big_examined``

   :Scope: Global
   :Dynamic: No
   :Option: :option:`--logging-query.threshold-big-examined`

   Threshold for logging big queries by rows examined.

.. _logging_query_threshold_big_resultset:

* ``logging_query_threshold_big_resultset``

   :Scope: Global
   :Dynamic: No
   :Option: :option:`--logging-query.threshold-big-resultset`

   Threshold for logging big queries by result set size.

.. _logging_query_threshold_slow:

* ``logging_query_threshold_slow``

   :Scope: Global
   :Dynamic: No
   :Option: :option:`--logging-query.threshold-slow`

   Threshold for logging slow queries by execution time.

.. _logging_query_examples:

Examples
--------

Sorry, there are no examples for this plugin.

.. _logging_query_authors:

Authors
-------

Mark Atwood

.. _logging_query_version:

Version
-------

This documentation applies to **logging_query 0.2**.

To see which version of the plugin a Drizzle server is running, execute:

.. code-block:: mysql

   SELECT MODULE_VERSION FROM DATA_DICTIONARY.MODULES WHERE MODULE_NAME='logging_query'

Changelog
---------

v0.2
^^^^
* First release.
