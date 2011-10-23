.. _http_functions_plugin:

HTTP Functions
==============

The ``http_functions`` plugin provides functions to get and post HTTP
data:

* ``HTTP_GET``
* ``HTTP_POST``

.. _http_functions_loading:

Loading
-------

To load this plugin, start :program:`drizzled` with::

   --plugin-add=http_functions

.. seealso:: :ref:`drizzled_plugin_options` for more information about adding and removing plugins.

.. _http_functions_authors:

Examples
--------

Start :program:`drizzled` with the plugin::

   sbin/drizzled --plugin-add=http_functions

Get `www.drizzle.org <http://www.drizzle.org>`_:

.. code-block:: mysql

   drizzle> SELECT HTTP_GET('www.drizzle.org')\G
   *************************** 1. row ***************************
   HTTP_GET('www.drizzle.org'): <!DOCTYPE html PUBLIC "-//W3C//DTD XHTML+RDFa 1.0//EN"
   "http://www.w3.org/MarkUp/DTD/xhtml-rdfa-1.dtd">
   <html xmlns="http://www.w3.org/1999/xhtml" xml:lang="en" version="XHTML+RDFa 1.0" dir="ltr"
   ...

Authors
-------

Stewart Smith

.. _http_functions_version:

Version
-------

This documentation applies to **http_functions 1.0**.

To see which version of the plugin a Drizzle server is running, execute:

.. code-block:: mysql

   SELECT MODULE_VERSION FROM DATA_DICTIONARY.MODULES WHERE MODULE_NAME='http_functions'

Changelog
---------

v1.0
^^^^
* First release.
