.. _user_locks_plugin:

User Locks
==========

User level locking and barrier functions.

.. _user_locks_loading:

Loading
-------

This plugin is loaded by default.
To stop the plugin from loading by default, start :program:`drizzled`
with::

   --plugin-remove=user_locks

.. seealso:: :ref:`drizzled_plugin_options` for more information about adding and removing plugins.

Examples
--------

Sorry, there are no examples for this plugin.

.. _user_locks_authors:

Authors
-------

Brian Aker

.. _user_locks_version:

Version
-------

This documentation applies to **user_locks 1.1**.

To see which version of the plugin a Drizzle server is running, execute:

.. code-block:: mysql

   SELECT MODULE_VERSION FROM DATA_DICTIONARY.MODULES WHERE MODULE_NAME='user_locks'

Changelog
---------

v1.1
^^^^
* First release.
