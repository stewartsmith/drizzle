.. _auth_pam_plugin:

PAM Authenication
=================

:program:`auth_pam` is an authentication plugin that authenticates connections
using :abbr:`PAM (Pluggable Authentication Module)`. PAM is effectively your 
current Linux based user security. This means you can setup Drizzle so that you 
can use your Linux system username and password to connect. System user and 
password files are typically stored in files ``/etc/passwd`` and 
``/etc/shadow``. However, PAM can also be setup to use other sources, such as an 
LDAP directory, as a user database. All of these options are transparently 
available to Drizzle via this module.

.. note::

   Unload the :doc:`/plugins/auth_all/index` plugin before using this plugin.

.. seealso:: :doc:`/administration/authentication` 

.. _auth_pam_loading:

Loading
-------

To load this plugin, start :program:`drizzled` with::

   --plugin-add=auth_pam

Or, to disable the ability to login without a password, use::

   --plugin-add=auth_pam --plugin-remove=auth_all

.. seealso::

   :ref:`drizzled_plugin_options` for more information about adding and removing plugins.

.. _auth_pam_configuration:

Configuration
-------------

This plugin does not have any command line options.

.. _auth_pam_variables:

Variables
---------

This plugin does not register any variables.

.. _auth_pam_examples:

Examples
--------

Most Linux distributions should have PAM configured in a way that it will just
work with Drizzle.  The default PAM configuration is typically found in
:file:`/etc/pam.d/other`. [1]_ However, if you want to  specifically configure
the way PAM will be used by Drizzle, then put something like the following
in :file:`/etc/pam.d/drizzle`:

.. code-block:: ini

   auth       required     pam_unix.so
   account    required     pam_unix.so

To enable auth_pam, start Drizzle like:

.. code-block:: bash

   $ sbin/drizzled --plugin-remove=auth_all --plugin-add=auth_pam

As an alternative to using command line options, you can enable auth_pam
by adding the following to :file:`/etc/drizzle/drizzled.cnf`:

.. code-block:: ini

   plugin-remove=auth_all
   plugin-add=auth_pam

Then connect to Drizzle like:

.. code-block:: none

   $ bin/drizzle -P --protocol mysql-plugin-auth
   Enter password: [Enter your system password here]

   Welcome to the Drizzle client..  Commands end with ; or \g.
   Your Drizzle connection id is 3
   Connection protocol: mysql-plugin-auth
   Server version: 2011.09.26.2426 Source distribution (drizzle-docs71)

   Type 'help;' or '\h' for help. Type '\c' to clear the buffer.
   
   drizzle> 

You must use ``--protocol mysql-plugin-auth`` for auth_pam to work. This 
protocol sends the password in plaintext to Drizzle, which
is required for PAM based authentication.

Note that you typically don't need to specify the ``-u`` or ``--user`` argument, 
since Drizzle will default to using your system username, which is exactly what 
we want when using auth_pam.

.. _auth_pam_security:

Security
--------

When using auth_pam, your Drizzle password is sent unencrypted from the client 
to the server. See :ref:`auth_pam_limitations` for details.
Note that this will almost always be your Linux system password too!

Arguably, this is not a problem when you are connecting to Drizzle from
localhost and sharing your system username and password for Drizzle can
be quite convenient. 

.. warning::

   Using auth_pam when connecting over a public or insecure network is strongly discouraged!

We recommend you disable auth_pam on networked Drizzle servers
and instead use the :ref:`auth_schema_plugin` plugin or alternatively
the :ref:`auth_ldap_plugin` plugin if you are interested in managing
usernames outside of Drizzle.

.. _auth_pam_limitations:

Limitations
-----------

Most Drizzle authentication plugins will use a challenge-response protocol
for authentication. In such schemes the client and the server each compute a 
hash that they compare with each other. Thanks to this, the password itself is
never sent over the network and therefore cannot be seen by an eavesdropping
attacker. The auth_pam plugin however needs to use the password in plaintext
format. This limitation is due to the typical configuration of PAM. For 
instance, also when you log in via SSH to your system, the password is sent in 
plaintext from the client to the server. Of course, in the case of SSH the 
communication channel itself is encrypted, so it cannot be eavesdropped.

Which leads us to the next limitation: the :ref:`drizzle_command_line_client`
does not support SSL connections. This means communication between client and server
is sent in unencrypted cleartext over the network, including your password. 
Hopefully a future version of the :ref:`drizzle_command_line_client` will support SSL 
encrypted connections, making auth_pam authentication more useful.

You must use the following parameters to the :ref:`drizzle_command_line_client`
to make sure your password is sent in plaintext to the server:

.. code-block:: bash

   $ drizzle -P --protocol mysql-plugin-auth

The ``-P`` or ``--password`` switch will make :program:`drizzle` ask for your
password  interactively. The ``--protocol mysql-plugin-auth`` will use a
protocol that sends the password in plaintext.

.. _auth_pam_authors:

Authors
-------

:Code: Brian Aker
:Documentation: Henrik Ingo, Daniel Nichter

.. _auth_pam_version:

Version
-------

This documentation applies to **auth_pam 0.1**.

To see which version of the plugin a Drizzle server is running, execute:

.. code-block:: mysql

   SELECT MODULE_VERSION FROM DATA_DICTIONARY.MODULES WHERE MODULE_NAME='auth_pam'

Changelog
---------

v0.1
^^^^
* First release.

-------------------------------------------------------------------------------

.. rubric:: Footnotes

.. [1] For more details about configuring PAM, see `The Linux Documentation Project: User Authentication HOWTO <http://tldp.org/HOWTO/User-Authentication-HOWTO/x263.html>`_

