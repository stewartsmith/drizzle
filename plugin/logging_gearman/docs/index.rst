.. _logging_gearman_plugin:

Gearman Logging
===============

The :program:`logging_gearman` plugin logs queries to a Gearman server.

.. _logging_gearman_loading:

Loading
-------

To load this plugin, start :program:`drizzled` with::

   --plugin-add=logging_gearman

If this plugin is loaded without passing out the arguments ``logging_gearman_host`` and ``logging_gearman_function``, default values will be used. Desired values for these can either be set at server startup by passing out these arguments or setting these values dynamically at the runtime using ``SET GLOBAL logging_gearman_host=<newhost>`` and ``SET GLOBAL logging_gearman_function=<newfunction>``.

Loading the plugin may not enable or configure it.  See the plugin's
:ref:`logging_gearman_configuration` and :ref:`logging_gearman_variables`.

.. seealso:: :ref:`drizzled_plugin_options` for more information about adding and removing plugins.

.. _logging_gearman_configuration:

Configuration
-------------

These command line options configure the plugin when :program:`drizzled`
is started.  See :ref:`command_line_options` for more information about specifying
command line options.

.. program:: drizzled

.. option:: --logging-gearman.function ARG

   :Default: drizzlelog
   :Variable: :ref:`logging_gearman_function <logging_gearman_function>`

   Gearman function to send logging to.

.. option:: --logging-gearman.host ARG

   :Default: localhost
   :Variable: :ref:`logging_gearman_host <logging_gearman_host>`

   Hostname for logging to a Gearman server.

.. _logging_gearman_variables:

Variables
---------

These variables show the running configuration of the plugin.
See `variables` for more information about querying and setting variables.

.. _logging_gearman_function:

* ``logging_gearman_function``

   :Scope: Global
   :Dynamic: Yes
   :Option: :option:`--logging-gearman.function`

   Gearman Function to send logging to

.. _logging_gearman_host:

* ``logging_gearman_host``

   :Scope: Global
   :Dynamic: Yes
   :Option: :option:`--logging-gearman.host`

   Hostname for logging to a Gearman server

.. _logging_gearman_examples:

Examples
--------

Sorry, there are no examples for this plugin.

.. _logging_gearman_authors:

Authors
-------

Mark Atwood

.. _logging_gearman_version:

Version
-------

This documentation applies to **logging_gearman 0.1**.

To see which version of the plugin a Drizzle server is running, execute:

.. code-block:: mysql

   SELECT MODULE_VERSION FROM DATA_DICTIONARY.MODULES WHERE MODULE_NAME='logging_gearman'

Changelog
---------

v0.1
^^^^
* First release.
