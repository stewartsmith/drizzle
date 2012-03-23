.. _authentication:

Authentication
==============

Authentication is any process by which you verify that someone is who they
claim they are. [1]_  

Drizzle authentication is handled by plugins; by default there is no single
source where users are defined, such as a system user table, but each
authentication plugin will use different sources to verify the usernames
and passwords. (The plugin auth_schema does however keep users in a table inside
Drizzle, much like the familiar MySQL way of authenticating users works.). 
*Choosing an authentication plugin, configuring it, and disabling all other 
authentication plugins should be one of your first administrative tasks.*

One or more authentication plugins must be loaded, else no connections can
be made to Drizzle.  On most systems, the :doc:`/plugins/auth_all/index`
plugin is loaded by default which, as its name suggests, allows all
connections regardless of username or password.  (Some distributions enable
the :doc:`/plugins/auth_file/index` plugin by default instead).

The :doc:`/plugins/auth_schema/index` plugin first shipped with ``Drizzle 7.1 
Beta 2011.10.28``. This plugin provides an authentication method that is
both secure and easy to use, and it is similar to how MySQL authentication
works so will be familiar to many users. If you don't know which authentication
plugin to use, you should start with configuring 
:doc:`/plugins/auth_schema/index`. Likewise we warmly recommend distributors to
consider enabling this plugin by default.

The following authentication plugins are included with Drizzle:

* :doc:`/plugins/auth_all/index` - Allow all connections without checking username or password.
* :doc:`/plugins/auth_file/index` - Define users and passwords in a text file that is read on startup.
* :doc:`/plugins/auth_http/index` - Authenticate the username and password against a http server.
* :doc:`/plugins/auth_ldap/index` - Define users and passwords in a LDAP directory.
* :doc:`/plugins/auth_pam/index` - Authenticate the username and password against system user accounts via PAM.
* :doc:`/plugins/auth_schema/index` - Define users and passwords in a system table.

Protocols
---------

Drizzle has three protocols which affect how clients send passwords to MySQL:

================== =============
Protocol           Password
================== =============
mysql              Encrypted
mysql-plugin-auth  Plaintext
drizzle            (Not used)
================== =============

These protocols correspond to the :ref:`drizzle_command_line_client`
--protocol option.

The mysql protocol is default, but some authentication plugins require
the mysql-plugin-auth protocol:

=========================  ==================
Plugin                     Protocol
=========================  ==================
:ref:`auth_all_plugin`     Any
:ref:`auth_file_plugin`    mysql
:ref:`auth_http_plugin`    mysql-plugin-auth
:ref:`auth_ldap_plugin`    Any
:ref:`auth_pam_plugin`     mysql-plugin-auth
:ref:`auth_schema_plugin`  mysql
=========================  ==================

-------------------------------------------------------------------------------

.. rubric:: Footnotes

.. [1] `Authentication, Authorization, and Access Control <http://httpd.apache.org/docs/1.3/howto/auth.html>`_
