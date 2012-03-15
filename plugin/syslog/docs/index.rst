.. _syslog_plugin:

Syslog
======

Syslog interface for query log, error messages, and functions.

.. _syslog_loading:

Loading
-------

This plugin is loaded by default, but it may need to be configured.  See
the plugin's :ref:`syslog_configuration` and
:ref:`syslog_variables`.

To stop the plugin from loading by default, start :program:`drizzled`
with::

   --plugin-remove=syslog

.. seealso:: :ref:`drizzled_plugin_options` for more information about adding and removing plugins.

.. _syslog_configuration:

Configuration
-------------

These command line options configure the plugin when :program:`drizzled`
is started.  See :ref:`command_line_options` for more information about specifying
command line options.

.. program:: drizzled

.. option:: --syslog.errmsg-enable 

   :Default: false
   :Variable: :ref:`syslog_errmsg_enable <syslog_errmsg_enable>`

   Enable logging to syslog of the error messages

.. option:: --syslog.errmsg-priority ARG

   :Default: warning
   :Variable: :ref:`syslog_errmsg_priority <syslog_errmsg_priority>`

   Syslog Priority of error messages

.. option:: --syslog.facility ARG

   :Default: local0
   :Variable: :ref:`syslog_facility <syslog_facility>`

   Syslog Facility

.. option:: --syslog.ident ARG

   :Default: drizzled
   :Variable:

   Syslog Ident

.. option:: --syslog.logging-enable 

   :Default: false
   :Variable: :ref:`syslog_logging_enable <syslog_logging_enable>`

   Enable logging to syslog of the query log

.. option:: --syslog.logging-priority ARG

   :Default: warning
   :Variable: :ref:`syslog_logging_priority <syslog_logging_priority>`

   Syslog Priority of query logging

.. option:: --syslog.logging-threshold-big-examined 

   :Default: 0
   :Variable: :ref:`syslog_logging_threshold_big_examined <syslog_logging_threshold_big_examined>`

   Threshold for logging big queries

.. option:: --syslog.logging-threshold-big-resultset 

   :Default: 0
   :Variable: :ref:`syslog_logging_threshold_big_resultset <syslog_logging_threshold_big_resultset>`

   Threshold for logging big queries

.. option:: --syslog.logging-threshold-slow 

   :Default: 0
   :Variable: :ref:`syslog_logging_threshold_slow <syslog_logging_threshold_slow>`

   Threshold for logging slow queries

.. _syslog_variables:

Variables
---------

These variables show the running configuration of the plugin.
See `variables` for more information about querying and setting variables.

.. _syslog_errmsg_enable:

* ``syslog_errmsg_enable``

   :Scope: Global
   :Dynamic: No
   :Option: :option:`--syslog.errmsg-enable`

   Enable logging to syslog of the error messages

.. _syslog_errmsg_priority:

* ``syslog_errmsg_priority``

   :Scope: Global
   :Dynamic: No
   :Option: :option:`--syslog.errmsg-priority`

   Syslog Priority of error messages

.. _syslog_facility:

* ``syslog_facility``

   :Scope: Global
   :Dynamic: No
   :Option: :option:`--syslog.facility`

   Syslog Facility

.. _syslog_logging_enable:

* ``syslog_logging_enable``

   :Scope: Global
   :Dynamic: No
   :Option: :option:`--syslog.logging-enable`

   Enable logging to syslog of the query log

.. _syslog_logging_priority:

* ``syslog_logging_priority``

   :Scope: Global
   :Dynamic: No
   :Option: :option:`--syslog.logging-priority`

   Syslog Priority of query logging

.. _syslog_logging_threshold_big_examined:

* ``syslog_logging_threshold_big_examined``

   :Scope: Global
   :Dynamic: No
   :Option: :option:`--syslog.logging-threshold-big-examined`

   Threshold for logging big queries

.. _syslog_logging_threshold_big_resultset:

* ``syslog_logging_threshold_big_resultset``

   :Scope: Global
   :Dynamic: No
   :Option: :option:`--syslog.logging-threshold-big-resultset`

   Threshold for logging big queries

.. _syslog_logging_threshold_slow:

* ``syslog_logging_threshold_slow``

   :Scope: Global
   :Dynamic: No
   :Option: :option:`--syslog.logging-threshold-slow`

   Threshold for logging slow queries

.. _syslog_examples:

Examples
--------

Sorry, there are no examples for this plugin.

.. _syslog_authors:

Authors
-------

Mark Atwood

.. _syslog_version:

Version
-------

This documentation applies to **syslog 0.3**.

To see which version of the plugin a Drizzle server is running, execute:

.. code-block:: mysql

   SELECT MODULE_VERSION FROM DATA_DICTIONARY.MODULES WHERE MODULE_NAME='syslog'

Changelog
---------

v0.3
^^^^
* First release.
