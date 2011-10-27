.. _replication_dictionary_plugin:

Replication Dictionary
======================

The :program:`replication_dictionary` plugin provides the DATA_DICTIONARY.REPLICATION_STREAMS table.

.. _replication_dictionary_loading:

Loading
-------

This plugin is loaded by default.
To stop the plugin from loading by default, start :program:`drizzled`
with::

   --plugin-remove=replication_dictionary

.. seealso:: :ref:`drizzled_plugin_options` for more information about adding and removing plugins.

Authors
-------

Jay Pipes

.. _replication_dictionary_version:

Version
-------

Unknown
