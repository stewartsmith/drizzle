User Locks
==========

User level locking and barrier functions.

.. _user_locks_loading:

Loading
-------

This plugin is loaded by default, but it may need to be configured.  See
the plugin's :ref:`user_locks_configuration` and
:ref:`user_locks_variables`.

To stop the plugin from loading by default, start :program:`drizzled`
with::

   --plugin-remove=user_locks

.. seealso:: :doc:`/options` for more information about adding and removing plugins.

.. _user_locks_configuration:

Configuration
-------------

These command line options configure the plugin when :program:`drizzled`
is started.  See :doc:`/configuration` for more information about specifying
command line options.

.. program:: drizzled

.. _user_locks_variables:

Variables
---------

These variables show the running configuration of the plugin.
See `variables` for more information about querying and setting variables.

.. _user_locks_examples:

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

