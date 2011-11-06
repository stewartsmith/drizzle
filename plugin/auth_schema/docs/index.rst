.. _auth_schema_plugin:

Schema Authentication
=====================

:program:`auth_schema` is an authentication plugin that authenticates
connections using a MySQL-like table with SHA1 password hashes.  Unlike
MySQL, the auth table is not built-in and there are no default or anonymous
users.  Since a user must authenticate to create the auth table but no
users can authenticate until the auth table is created, this circular
dependency is resolved by temporarily using another authentication plugin.
See the :ref:`auth_schema_examples`.

.. note::

   Unload the :doc:`/plugins/auth_all/index` plugin before using this plugin.

.. seealso:: :doc:`/administration/authentication` 

.. _auth_schema_loading:

Loading
-------

To load this plugin, start :program:`drizzled` with::

   --plugin-add=auth_schema

Loading the plugin may not enable or configure it.  See the plugin's
:ref:`auth_schema_configuration` and :ref:`auth_schema_variables`.

.. seealso:: :ref:`drizzled_plugin_options` for more information about adding and removing plugins.

.. _auth_schema_configuration:

Configuration
-------------

These command line options configure the plugin when :program:`drizzled`
is started.  See :ref:`command_line_options` for more information about specifying
command line options.

.. program:: drizzled

.. option:: --auth-schema.table ARG

   :Default: ``auth.users``
   :Variable: :ref:`auth_schema_table <auth_schema_table>`

   Schema-qualified table with ``user`` and ``password`` columns.  Quoting the auth table
   in backticks is optional.  The auth table name can only contain one period between the
   schema name and the table name.

.. _auth_schema_variables:

Variables
---------

These variables show the running configuration of the plugin.
See `variables` for more information about querying and setting variables.

.. _auth_schema_enabled:

* ``auth_schema_enabled``

   :Scope: Global
   :Dynamic: Yes
   :Option:

   If :program:`auth_schema` is enabled or disabled.  If the plugin is
   disabled, all authentication is denied.

.. _auth_schema_table:

* ``auth_schema_table``

   :Scope: Global
   :Dynamic: Yes
   :Option: :option:`--auth-schema.table`

   Schema-qualified table with ``user`` and ``password`` columns.

.. _auth_schema_examples:

Examples
--------

Start Drizzle with the default :doc:`/plugins/auth_all/index` plugin and
create the initial auth schema and table:

.. code-block:: mysql

   CREATE SCHEMA auth;
   USE auth;
   CREATE TABLE users (
      user     VARCHAR(255) NOT NULL,
      password VARCHAR(40),
      UNIQUE INDEX user_idx (user)
   );

Create a user account called ``susan`` with password ``herpass``:

.. code-block:: mysql

   INSERT INTO auth.users (user, password) VALUES ('susan', MYSQL_PASSWORD('herpass'));

Restart Drizzle with just the :program:`auth_schema` plugin:

.. code-block:: bash

   bin/drizzled --shutdown
   sbin/drizzled               \
      --plugin-remove=auth_all \
      --plugin-add=auth_schema

Test that it works:

.. code-block:: bash

   $ drizzle
   ERROR 1045 (28000): Access denied for user 'daniel' (using password: NO)

   $ drizzle --user susan
   ERROR 1045 (28000): Access denied for user 'susan' (using password: NO)

   $ drizzle --user susan --password=wrongpass
   ERROR 1045 (28000): Access denied for user 'susan' (using password: YES)

   $ drizzle --user=susan --password=herpass
   Welcome to the Drizzle client..  Commands end with ; or \g.
   ...

.. _auth_schema_authors:

Authors
-------

Daniel Nichter

.. _auth_schema_version:

Version
-------

This documentation applies to **auth_schema 1.0**.

To see which version of the plugin a Drizzle server is running, execute:

.. code-block:: mysql

   SELECT MODULE_VERSION FROM DATA_DICTIONARY.MODULES WHERE MODULE_NAME='auth_schema'

Changelog
---------

v1.0
^^^^
* First release.
