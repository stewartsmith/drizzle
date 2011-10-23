.. _session_dictionary_plugin:

Session Dictionary
==================

The :program:`session_dictionary` plugin provides several DATA_DICTIONARY tables:

* SESSIONS
* SESSION_STATEMENTS
* SESSION_STATUS
* SESSION_VARIABLES

.. _session_dictionary_loading:

Loading
-------

This plugin is loaded by default.
To stop the plugin from loading by default, start :program:`drizzled`
with::

   --plugin-remove=session_dictionary

.. seealso:: :ref:`drizzled_plugin_options` for more information about adding and removing plugins.

.. _session_dictionary_configuration:

Authors
-------

Brian Aker

.. _session_dictionary_version:

Version
-------

This documentation applies to **session_dictionary 1.1**.

To see which version of the plugin a Drizzle server is running, execute:

.. code-block:: mysql

   SELECT MODULE_VERSION FROM DATA_DICTIONARY.MODULES WHERE MODULE_NAME='session_dictionary'

Changelog
---------

v1.1
^^^^
* First release.
