.. _function_dictionary_plugin:

Function Dictionary
===================

:program:`function_dictionary` creates the ``FUNCTIONS`` dictionary which lists
available functions.

.. _function_dictionary_loading:

Loading
-------

To load this plugin, start :program:`drizzled` with::

   --plugin-add=function_dictionary

.. seealso:: :ref:`drizzled_plugin_options` for more information about adding and removing plugins.

.. _function_dictionary_examples:

Examples
--------

List all available functions:

.. code-block:: mysql

   drizzle> SELECT * FROM DATA_DICTIONARY.FUNCTIONS;
   +-----------------------+
   | FUNCTION_NAME         |
   +-----------------------+
   | LOWER                 | 
   | LAST_INSERT_ID        | 
   | LTRIM                 | 
   | GREATEST              | 
   ...

.. _function_dictionary_authors:

Authors
-------

Brian Aker

.. _function_dictionary_version:

Version
-------

This documentation applies to **function_dictionary 1.0**.

To see which version of the plugin a Drizzle server is running, execute:

.. code-block:: mysql

   SELECT MODULE_VERSION FROM DATA_DICTIONARY.MODULES WHERE MODULE_NAME='function_dictionary'

Changelog
---------

v1.0
^^^^
* First release.
