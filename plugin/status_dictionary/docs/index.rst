.. _status_dictionary_plugin:

Status Dictionary
=================

The :program:`status_dictionary` plugin provides the DATA_DICTIONARY.VARIABLES and DATA_DICTIONARY.GLOBAL_VARIABLES tables which make the ``SHOW [GLOBAL] VARIABLES`` command work.

.. _status_dictionary_loading:

Loading
-------

This plugin is loaded by default.
To stop the plugin from loading by default, start :program:`drizzled`
with::

   --plugin-remove=status_dictionary

.. seealso:: :ref:`drizzled_plugin_options` for more information about adding and removing plugins.

Authors
-------

Brian Aker

.. _status_dictionary_version:

Version
-------

This documentation applies to **status_dictionary 1.0**.

To see which version of the plugin a Drizzle server is running, execute:

.. code-block:: mysql

   SELECT MODULE_VERSION FROM DATA_DICTIONARY.MODULES WHERE MODULE_NAME='status_dictionary'

Changelog
---------

v1.0
^^^^
* First release.
