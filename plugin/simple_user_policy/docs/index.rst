.. _simple_user_policy_plugin:

User-based Authorization
========================

:program:`simple_user_policy` is an :doc:`/administration/authorization` plugin
that only allows users to access schemas with their exact name.  For example,
user "robert" can only access schema "robert".  User "root" can access all
schemas.  The plugin does not require a policy file or any configuration;
the policiy is as simple as described here.

.. _simple_user_policy_loading:

Loading
-------

To load this plugin, start :program:`drizzled` with::

   --plugin-add=simple_user_policy

.. seealso:: :ref:`drizzled_plugin_options` for more information about adding and removing plugins.

Configuration
-------------

These command line options configure the plugin when :program:`drizzled`
is started.  See :ref:`command_line_options` for more information about specifying command line options.

.. program:: drizzled

.. option:: --simple-user-policy.remap-dot-to ARG

   :Default: '.'

   Since using a period (dot) in a schema name requires quoting, we support remapping this to another character. When set to an underscore, this enables user 'first.last' to connect to the 'first_last' schema, a schema name which does not require quoting.

Examples
--------


.. _simple_user_policy_authors:

Authors
-------

Monty Taylor, Stewart Smith

.. _simple_user_policy_version:

Version
-------

This documentation applies to **simple_user_policy 1.0**.

To see which version of the plugin a Drizzle server is running, execute:

.. code-block:: mysql

   SELECT MODULE_VERSION FROM DATA_DICTIONARY.MODULES WHERE MODULE_NAME='simple_user_policy'

Changelog
---------

v1.1
^^^^
* Added ``remap-dot-to`` option.

v1.0
^^^^
* First release.
