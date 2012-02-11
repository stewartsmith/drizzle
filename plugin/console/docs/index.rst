.. _console_plugin:

Console Plugin
==============

:program:`console` is a protocol plugin that instead of providing access to Drizzle through a network port or UNIX socket provides a direct interactive text console via stdin and stdout. It allows you to use Drizzle in a similar way that the sqlite command line client allows you to use sqlite.

.. _console_loading:

Loading
-------

To load this plugin, start :program:`drizzled` with::

   --plugin-add=console

.. seealso::

   :ref:`drizzled_plugin_options` for more information about adding and removing plugins.

.. _console_configuration:

Configuration
-------------

.. option:: --console.enable

   :Default: ``false``

   Enables the console.

.. option:: --consoe.debug

   :Default: ``false``

   Turns on extra debugging output

.. option:: --console.username ARG

   :Default: ``current logged in user``

   The username to authenticate to Drizle as. This option works the same way as connecting as a username over other protocols.

.. option:: --console.password ARG

   The password to use to authenticate with.

.. option:: --console.catalog ARG

   :Default: ``LOCAL``

   Which catalog to connect to. In current released versions of Drizzle, only the LOCAL catalog is supported, althoug in the future other catalogs will be usable.

.. option:: --console.schema ARG

   Which schema to connect to.

.. _console_variables:

Variables
---------

This plugin does not register any variables.

.. _console_examples:

Examples
--------

To use the Drizzle console, start Drizzle like:

.. code-block:: bash

   $ sbin/drizzled --plugin-add=console --console.enable

You can now interact with Drizzle in pretty much the same way you can as through the command line client.

You probably never want to enable the console plugin in the configuration file.

.. _console_limitations:

Limitations
-----------

There is currently no way to change your authentication credentials once you've launched Drizzle with the console plugin.

.. _console_authors:

Authors
-------

:Code: Eric Day
:Documentation: Stewart Smith

.. _console_version:

Version
-------

This documentation applies to **console 0.2**.

To see which version of the plugin a Drizzle server is running, execute:

.. code-block:: mysql

   SELECT MODULE_VERSION FROM DATA_DICTIONARY.MODULES WHERE MODULE_NAME='console'

Changelog
---------

v0.1
^^^^
* First release.

v0.2
^^^^
* Support connecting to a CATALOG
* Rename db parameter to schema

