.. _connection_id_plugin:

CONNECTION_ID Function
======================

The ``connection_id`` plugin provides these :doc:`/functions/overview`:

* :ref:`connection-id-function`

.. _connection_id_loading:

Loading
-------

This plugin is loaded by default.  To stop the plugin from loading by
default, start :program:`drizzled` with::

   --plugin-remove=connection_id

.. seealso:: :ref:`drizzled_plugin_options` for more information about adding and removing plugins.

.. _connection_id_authors:

Authors
-------

Devananda van der Veen

.. _connection_id_version:

Version
-------

This documentation applies to **connection_id 1.0**.

To see which version of the plugin a Drizzle server is running, execute:

.. code-block:: mysql

   SELECT MODULE_VERSION FROM DATA_DICTIONARY.MODULES WHERE MODULE_NAME='connection_id'

Changelog
---------

v1.0
^^^^
* First release.
