.. _logging_stats_plugin:

User Statistics
===============

The :program:`logging_stats` plugin provides user statistics as
DATA_DICTIONARY tables.

.. _logging_stats_loading:

Loading
-------

This plugin is loaded by default, but it may need to be configured.  See
the plugin's :ref:`logging_stats_configuration` and
:ref:`logging_stats_variables`.

To stop the plugin from loading by default, start :program:`drizzled`
with::

   --plugin-remove=logging_stats

.. seealso:: :ref:`drizzled_plugin_options` for more information about adding and removing plugins.

.. _logging_stats_configuration:

Configuration
-------------

These command line options configure the plugin when :program:`drizzled`
is started.  See :ref:`command_line_options` for more information about specifying
command line options.

.. program:: drizzled

.. option:: --logging-stats.bucket-count ARG

   :Default: 10
   :Variable: :ref:`logging_stats_bucket_count <logging_stats_bucket_count>`

   Max number of range locks to use for Scoreboard.

.. option:: --logging-stats.disable 

   :Default: 
   :Variable:

   Disable logging statistics collection.

.. option:: --logging-stats.max-user-count ARG

   :Default: 500
   :Variable: :ref:`logging_stats_max_user_count <logging_stats_max_user_count>`

   Max number of users to log.

.. option:: --logging-stats.scoreboard-size ARG

   :Default: 2000
   :Variable: :ref:`logging_stats_scoreboard_size <logging_stats_scoreboard_size>`

   Max number of concurrent sessions to log.

.. _logging_stats_variables:

Variables
---------

These variables show the running configuration of the plugin.
See `variables` for more information about querying and setting variables.

.. _logging_stats_bucket_count:

* ``logging_stats_bucket_count``

   :Scope: Global
   :Dynamic: No
   :Option: :option:`--logging-stats.bucket-count`

   Max number of range locks to use for Scoreboard.

.. _logging_stats_enable:

* ``logging_stats_enable``

   :Scope: Global
   :Dynamic: No
   :Option:

   If logging stats is enabled or not.

.. _logging_stats_max_user_count:

* ``logging_stats_max_user_count``

   :Scope: Global
   :Dynamic: No
   :Option: :option:`--logging-stats.max-user-count`

   Max number of users to log.

.. _logging_stats_scoreboard_size:

* ``logging_stats_scoreboard_size``

   :Scope: Global
   :Dynamic: No
   :Option: :option:`--logging-stats.scoreboard-size`

   Max number of concurrent sessions to log.

.. _logging_stats_examples:

Examples
--------

Sorry, there are no examples for this plugin.

.. _logging_stats_authors:

Authors
-------

Joseph Daly

.. _logging_stats_version:

Version
-------

This documentation applies to **logging_stats 0.1**.

To see which version of the plugin a Drizzle server is running, execute:

.. code-block:: mysql

   SELECT MODULE_VERSION FROM DATA_DICTIONARY.MODULES WHERE MODULE_NAME='logging_stats'


Changelog
---------

v0.1
^^^^
* First release.
