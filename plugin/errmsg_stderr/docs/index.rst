.. _errmsg_stderr_plugin:

Error Messages to STDERR
========================

:program:`errmsg_stderr` is an error message plugin that prints all
:program:`drizzled`: error messages to ``STDERR``.  Only server errors are
printed; SQL errors, replication errors, etc. are not captured by error
message plugins.

.. _errmsg_stderr_loading:

Loading
-------

This plugin is loaded by default, but it may need to be configured.  See
the plugin's :ref:`errmsg_stderr_configuration` and
:ref:`errmsg_stderr_variables`.

To stop the plugin from loading by default, start :program:`drizzled`
with::

   --plugin-remove=errmsg_stderr

.. seealso:: :ref:`drizzled_plugin_options` for more information about adding and removing plugins.

.. _errmsg_stderr_configuration:

Configuration
-------------

This plugin does not have any command line options.

.. _errmsg_stderr_variables:

Variables
---------

This plugin does not register any variables.

.. _errmsg_stderr_examples:

Examples
--------

Redirect ``STDOUT`` and ``STDERR`` to :file:`drizzled.err`::

   sbin/drizzled >> drizzled.err 2>&1

.. _errmsg_stderr_authors:

Authors
-------

Mark Atwood

.. _errmsg_stderr_version:

Version
-------

This documentation applies to **errmsg_stderr 0.1**.

To see which version of the plugin a Drizzle server is running, execute:

.. code-block:: mysql

   SELECT MODULE_VERSION FROM DATA_DICTIONARY.MODULES WHERE MODULE_NAME='errmsg_stderr'

Changelog
---------

v0.1
^^^^
* First release.
