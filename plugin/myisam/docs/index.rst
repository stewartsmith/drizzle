.. _myisam_plugin:

MyISAM Storage Engine
=====================

The :program:`myisam` plugin provides the classic MySQL MyISAM storage engine.
Unlike MySQL, however, the :doc:`/plugins/innobase/index` is the
default storage engine in Drizzle.

.. _myisam_loading:

Loading
-------

This plugin is loaded by default, but it may need to be configured.  See
the plugin's :ref:`myisam_configuration` and
:ref:`myisam_variables`.

To stop the plugin from loading by default, start :program:`drizzled`
with::

   --plugin-remove=myisam

.. seealso:: :ref:`drizzled_plugin_options` for more information about adding and removing plugins.

.. _myisam_configuration:

Configuration
-------------

These command line options configure the plugin when :program:`drizzled`
is started.  See :ref:`command_line_options` for more information about specifying
command line options.

.. program:: drizzled

.. option:: --myisam.max-sort-file-size ARG

   :Default: INT32_MAX
   :Variable: :ref:`myisam_max_sort_file_size <myisam_max_sort_file_size>`

   Don't use the fast sort index method to created index if the temporary file would get bigger than this.

.. option:: --myisam.sort-buffer-size ARG

   :Default: 8388608 (8M)
   :Variable: :ref:`myisam_sort_buffer_size <myisam_sort_buffer_size>`

   The buffer that is allocated when sorting the index when doing a REPAIR or when creating indexes with CREATE INDEX or ALTER TABLE.

.. _myisam_variables:

Variables
---------

These variables show the running configuration of the plugin.
See `variables` for more information about querying and setting variables.

.. _myisam_max_sort_file_size:

* ``myisam_max_sort_file_size``

   :Scope: Global
   :Dynamic: No
   :Option: :option:`--myisam.max-sort-file-size`

   Don't use the fast sort index method to created index if the temporary file would get bigger than this.

.. _myisam_sort_buffer_size:

* ``myisam_sort_buffer_size``

   :Scope: Global
   :Dynamic: No
   :Option: :option:`--myisam.sort-buffer-size`

   The buffer that is allocated when sorting the index when doing a REPAIR or when creating indexes with CREATE INDEX or ALTER TABLE.

.. _myisam_examples:

.. _myisam_authors:

Authors
-------

MySQL AB

.. _myisam_version:

Version
-------

This documentation applies to **myisam 2.0**.

To see which version of the plugin a Drizzle server is running, execute:

.. code-block:: mysql

   SELECT MODULE_VERSION FROM DATA_DICTIONARY.MODULES WHERE MODULE_NAME='myisam'

Changelog
---------

v2.0
^^^^
* First Drizzle version.
