.. _auth_file_plugin:

File-based Authentication
=========================

.. warning::

   :program:`auth_file` is a security risk!  Do not use this plugin with production servers!

:program:`auth_file` is an :doc:`/administration/authorization` plugin that authenticates connections
using a list of ``username:password`` entries in a plain text file. When :program:`drizzled` is started with  ``--plugin-add=auth_file``, the file based authorization plugin is enabled with the default users file. Users file can be specified by either specifying ``--auth-file.users=<users file>`` at the time of server startup or by changing the ``auth_file_users`` with ``SET GLOBAL``.

.. note::

   Unload the :doc:`/plugins/auth_all/index` plugin before using this plugin.

.. seealso:: :doc:`/administration/authentication` 

.. _auth_file_loading:

Loading
-------

To load this plugin, start :program:`drizzled` with::

   --plugin-add=auth_file

Loading the plugin may not enable or configure it.  See the plugin's
:ref:`auth_file_configuration` and :ref:`auth_file_variables`.

.. seealso:: :ref:`drizzled_plugin_options` for more information about adding and removing plugins.

.. _auth_file_configuration:

Configuration
-------------

These command line options configure the plugin when :program:`drizzled`
is started.  See :ref:`command_line_options` for more information about specifying
command line options.

.. program:: drizzled

.. option:: --auth-file.users ARG

   :Default: :file:`BASEDIR/etc/drizzle.users`
   :Variable: :ref:`auth_file_users <auth_file_users>`

   File to load for usernames and passwords.

.. _auth_file_variables:

Variables
---------

These variables show the running configuration of the plugin.
See `variables` for more information about querying and setting variables.

.. _auth_file_users:

* ``auth_file_users``

   :Scope: Global
   :Dynamic: Yes
   :Option: :option:`--auth-file.users`

   File to load for usernames and passwords.

.. _auth_file_examples:

Examples
--------

First, create a :file:`users` file with one ``user:pass`` entry per line, like::

   user1:password1
   user2:password2

Then start :program:`drizzled` like::

   sbin/drizzled --plugin-remove=auth_all \  
                 --plugin-add=auth_file   \
                 --auth-file.users=/path/to/my/users

Test that it works::

   $ drizzle
   ERROR 1045 (28000): Access denied for user 'daniel' (using password: NO)

   $ drizzle --user=user1
   ERROR 1045 (28000): Access denied for user 'user1' (using password: NO)

   $ drizzle --user=user1 --password=password1
   Welcome to the Drizzle client..  Commands end with ; or \g.
   ...

Changing users file at runtime
-------------------------------

Users file can be reloaded by::

   SET GLOBAL auth_file_users=@@auth_file_users

Moreover, the users file can be changed by::

   SET GLOBAL auth_file_users=/path/to/new/users/file

.. _auth_file_authors:

Authors
-------

Eric Day

.. _auth_file_version:

Version
-------

This documentation applies to **auth_file 0.1**.

To see which version of the plugin a Drizzle server is running, execute:

.. code-block:: mysql

   SELECT MODULE_VERSION FROM DATA_DICTIONARY.MODULES WHERE MODULE_NAME='auth_file'

Changelog
---------

v0.1
^^^^
* First release.

