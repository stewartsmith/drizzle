.. _auth_all_plugin:

Allow All Authentication
========================

:program:`auth_all` is an authentication plugin that allows *all* connections
regardless of username or password, so it does not actually authenticate and
it does not provide any security.  This plugin is mostly used for testing.

.. seealso:: :doc:`/administration/authentication`

.. _auth_all_loading:

Loading
-------

This plugin is loaded by default, but it may need to be configured.  See
the plugin's :ref:`auth_all_configuration` and
:ref:`auth_all_variables`.

To stop the plugin from loading by default, start :program:`drizzled`
with::

   --plugin-remove=auth_all

.. seealso:: :ref:`drizzled_plugin_options` for more information about adding and removing plugins.

.. _auth_all_configuration:

Configuration
-------------

These command line options configure the plugin when :program:`drizzled`
is started.  See :ref:`command_line_options` for more information about specifying
command line options.

.. program:: drizzled

.. option:: --auth-all.allow_anonymous 

   :Default: false
   :Variable:

   Allow anonymous access.

.. _auth_all_variables:

Variables
---------

This plugin does not register any variables.

Authors
-------

Brian Aker

.. _auth_all_version:

Version
-------

This documentation applies to **auth_all 1.0**.

To see which version of the plugin a Drizzle server is running, execute:

.. code-block:: mysql

   SELECT MODULE_VERSION FROM DATA_DICTIONARY.MODULES WHERE MODULE_NAME='auth_all'

Changelog
---------

v1.0
^^^^
* First release.
