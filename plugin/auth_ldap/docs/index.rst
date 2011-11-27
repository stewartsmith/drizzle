.. _auth_ldap_plugin:

LDAP Authentication
===================

:program:`auth_ldap` is an authentication plugin that authenticates connections
using an :abbr:`LDAP (Lightweight Directory Access Protocol)` server.  An
LDAP server is required to provide authentication.

.. note::

   Unload the :doc:`/plugins/auth_all/index` plugin before using this plugin.

.. seealso:: :doc:`/administration/authentication` 

.. _auth_ldap_loading:

Loading
-------

To load this plugin, start :program:`drizzled` with::

   --plugin-add=auth_ldap

Loading the plugin may not enable or configure it.  See the plugin's
:ref:`auth_ldap_configuration` and :ref:`auth_ldap_variables`.

.. seealso:: :ref:`drizzled_plugin_options` for more information about adding and removing plugins.

.. _auth_ldap_configuration:

Configuration
-------------

These command line options configure the plugin when :program:`drizzled`
is started.  See :ref:`command_line_options` for more information about specifying
command line options.

.. program:: drizzled

.. option:: --auth-ldap.base-dn ARG

   :Default: 
   :Variable: :ref:`auth_ldap_base_dn <auth_ldap_base_dn>`

   DN to use when searching.

.. option:: --auth-ldap.bind-dn ARG

   :Default: 
   :Variable: :ref:`auth_ldap_bind_dn <auth_ldap_bind_dn>`

   DN to use when binding to the LDAP server.

.. option:: --auth-ldap.bind-password ARG

   :Default: 
   :Variable: :ref:`auth_ldap_bind_password <auth_ldap_bind_password>`

   Password to use when binding the DN.

.. option:: --auth-ldap.cache-timeout ARG

   :Default: ``0``
   :Variable: :ref:`auth_ldap_cache_timeout <auth_ldap_cache_timeout>`

   How often to empty the users cache, 0 to disable.

.. option:: --auth-ldap.mysql-password-attribute ARG

   :Default: ``drizzleMysqlUserPassword``
   :Variable: :ref:`auth_ldap_mysql_password_attribute <auth_ldap_mysql_password_attribute>`

   Attribute in LDAP with MySQL hashed password.

.. note::
   Until Drizzle 2011.11.29 (a Drizzle 7.1 beta release) the default value of this
   parameter was ``mysqlUserPassword``. Beginning with release 2011.12.30
   it was changed to ``drizzleMysqlUserPassword`` to match the provided
   openldap ldif schema.

.. option:: --auth-ldap.password-attribute ARG

   :Default: ``userPassword``
   :Variable: :ref:`auth_ldap_password_attribute <auth_ldap_password_attribute>`

   Attribute in LDAP with plain text password.

.. option:: --auth-ldap.uri ARG

   :Default: ``ldap://127.0.0.1/``
   :Variable: :ref:`auth_ldap_uri <auth_ldap_uri>`

   URI of the LDAP server to contact.

.. _auth_ldap_variables:

Variables
---------

These variables show the running configuration of the plugin.
See `variables` for more information about querying and setting variables.

.. _auth_ldap_base_dn:

* ``auth_ldap_base_dn``

   :Scope: Global
   :Dynamic: No
   :Option: :option:`--auth-ldap.base-dn`

   DN to use when searching.

.. _auth_ldap_bind_dn:

* ``auth_ldap_bind_dn``

   :Scope: Global
   :Dynamic: No
   :Option: :option:`--auth-ldap.bind-dn`

   DN to use when binding to the LDAP server.

.. _auth_ldap_bind_password:

* ``auth_ldap_bind_password``

   :Scope: Global
   :Dynamic: No
   :Option: :option:`--auth-ldap.bind-password`

   Password to use when binding the DN.

.. _auth_ldap_cache_timeout:

* ``auth_ldap_cache_timeout``

   :Scope: Global
   :Dynamic: No
   :Option: :option:`--auth-ldap.cache-timeout`

   How often to empty the users cache.

.. _auth_ldap_mysql_password_attribute:

* ``auth_ldap_mysql_password_attribute``

   :Scope: Global
   :Dynamic: No
   :Option: :option:`--auth-ldap.mysql-password-attribute`

   Attribute in LDAP with MySQL hashed password.

.. _auth_ldap_password_attribute:

* ``auth_ldap_password_attribute``

   :Scope: Global
   :Dynamic: No
   :Option: :option:`--auth-ldap.password-attribute`

   Attribute in LDAP with plain text password.

.. _auth_ldap_uri:

* ``auth_ldap_uri``

   :Scope: Global
   :Dynamic: No
   :Option: :option:`--auth-ldap.uri`

   URI of the LDAP server to contact.

.. _auth_ldap_examples:

Examples
--------

Sorry, there are no examples for this plugin.

.. _auth_ldap_authors:

Authors
-------

Eric Day

.. _auth_ldap_version:

Version
-------

This documentation applies to **auth_ldap 0.1**.

To see which version of the plugin a Drizzle server is running, execute:

.. code-block:: mysql

   SELECT MODULE_VERSION FROM DATA_DICTIONARY.MODULES WHERE MODULE_NAME='auth_ldap'

Changelog
---------

v0.1
^^^^
* First release.
