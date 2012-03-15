.. _utility_dictionary_plugin:

Utility Dictionary
==================

The :program:`utility_dictionary` plugin provides several DATA_DICTIONARY tables:

* COUNTER
* ENVIRONMENTAL
* RANDOM_NUMBER
* RANDOM_STRING


.. _utility_dictionary_loading:

Loading
-------

To load this plugin, start :program:`drizzled` with::

   --plugin-add=utility_dictionary

.. seealso:: :ref:`drizzled_plugin_options` for more information about adding and removing plugins.

Examples
--------

.. code-block:: mysql

   drizzle> SELECT * FROM DATA_DICTIONARY.COUNTER;
   +-------+
   | VALUE |
   +-------+
   |     1 | 
   +-------+

   drizzle> SELECT * FROM DATA_DICTIONARY.COUNTER;
   +-------+
   | VALUE |
   +-------+
   |     2 | 
   +-------+

   drizzle> SELECT * FROM DATA_DICTIONARY.COUNTER;
   +-------+
   | VALUE |
   +-------+
   |     3 | 
   +-------+

   drizzle> SELECT * FROM DATA_DICTIONARY.ENVIRONMENTAL;
   +---------------+----------------------------+
   | VARIABLE_NAME | VARIABLE_VALUE             |
   +---------------+----------------------------+
   | SHELL         | /bin/bash                  | 
   | TERM          | xterm-color                | 
   | USER          | daniel                     | 
   | EDITOR        | vi                         | 
   | HOME          | /Users/daniel              | 
   | LOGNAME       | daniel                     | 
   | DISPLAY       | /tmp/launch-ccfH4l/org.x:0 | 
   | _             | ./sbin/drizzled            | 
   +---------------+----------------------------+

   drizzle> SELECT * FROM DATA_DICTIONARY.RANDOM_NUMBER;
   +------------+
   | VALUE      |
   +------------+
   | 1804289383 | 
   +------------+

   drizzle> SELECT * FROM DATA_DICTIONARY.RANDOM_STRING;
   +----------------------+
   | VALUE                |
   +----------------------+
   | 8FJid3GaHpRC2L6jX3i9 |
   +----------------------+

.. _utility_dictionary_authors:

Authors
-------

Brian Aker

.. _utility_dictionary_version:

Version
-------

This documentation applies to **utility_dictionary 1.0**.

To see which version of the plugin a Drizzle server is running, execute:

.. code-block:: mysql

   SELECT MODULE_VERSION FROM DATA_DICTIONARY.MODULES WHERE MODULE_NAME='utility_dictionary'

Changelog
---------

v1.0
^^^^
* First release.
