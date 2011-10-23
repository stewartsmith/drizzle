Authentication
==============

Authentication is any process by which you verify that someone is who they
claim they are. [1]_  Drizzle authentication is handled by plugins; there
are no grant or privilege tables.

One or more authentication plugin must be loaded, else no connections can
be made to Drizzle.  On most systems, the :doc:`/plugins/auth_all/index`
plugin is loaded by default which, as its name suggests, allows all
connections regardless of username or password.  (Some distributions enable
the :doc:`/plugins/auth_file/index` plugin by default instead).

`Choosing an authentication plugin, configuring it, and disabling all other
authentication plugins should be one of your first administrative tasks.`

The following authentication plugins are included with Drizzle:

* :doc:`/plugins/auth_all/index`
* :doc:`/plugins/auth_file/index`
* :doc:`/plugins/auth_http/index`
* :doc:`/plugins/auth_ldap/index`
* :doc:`/plugins/auth_pam/index`

Protocols
---------

The :ref:`drizzle_command_line_client` supports three authentication
protocols: ``mysql`` (default), ``mysql-plugin-auth``, and ``drizzle``.
The default ``mysql`` protocol sends the client's password as an
encrypted hash.

-------------------------------------------------------------------------------

.. rubric:: Footnotes

.. [1] `Authentication, Authorization, and Access Control <http://httpd.apache.org/docs/1.3/howto/auth.html>`_
