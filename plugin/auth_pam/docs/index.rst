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

.. note:: Unload the :doc:`/plugins/auth_all/index` plugin before using this plugin.
.. seealso:: :doc:`/administration/authentication` 

.. _auth_pam_loading:

Loading
-------

To load this plugin, start :program:`drizzled` with::

   --plugin-add=auth_pam

Or, to disable the ability to login without a password, use::

   --plugin-add=auth_pam --plugin-remove=auth_all

.. seealso:: :doc:`/options` for more information about adding and removing plugins.

.. _auth_pam_configuration:

Configuration
-------------

This plugin does not have any command line options.

.. _auth_pam_example_configuration:

Example configuration file
--------------------------

As an alternative to using command line parameters, you can enable auth_pam
by adding the following to your ``drizzle.cnf``:

.. code-block:: ini

   plugin-remove=auth_all
   plugin-add=auth_pam

Most Linux distributions should have PAM configured in a way that it will just
work when Drizzle tries to authenticate you. The default PAM configuration is
typically found in ``/etc/pam.d/other``. [1]_ However, if you'd want to 
specifically configure the way PAM will be used by Drizzle, this would be done 
by putting something like the following into a file ``/etc/pam.d/drizzle``:

.. code-block:: ini

   auth       required     pam_unix.so
   account    required     pam_unix.so

.. _auth_pam_variables:

Variables
---------

This plugin does not register any variables.

.. _auth_pam_examples:

Examples
--------

Start Drizzle server with::

   sbin/drizzled --no-defaults --mysql-protocol.port=3306 --basedir=$PWD --datadir=$PWD/var --plugin-remove=auth_all --plugin-add=auth_pam

Then connect with::

   $ bin/drizzle -P --protocol mysql-plugin-auth
   Enter password: [Enter your system password here]
   Welcome to the Drizzle client..  Commands end with ; or \g.
   Your Drizzle connection id is 3
   Connection protocol: mysql-plugin-auth
   Server version: 2011.09.26.2426 Source distribution (drizzle-docs71)
   
   Type 'help;' or '\h' for help. Type '\c' to clear the buffer.
   
   drizzle> 

You must use ``--protocol mysql-plugin-auth`` for auth_pam to work. This 
protocol variant sends the password in plaintext to the Drizzle server, which
is required for PAM based authentication.

Note that you don't need to specify the ``-u`` or ``--user`` argument, since
drizzle will default to using your system username, which is exactly what we
want when using auth_pam!


.. _auth_pam_security:

Security
--------

When using auth_pam, your Drizzle password is sent unencrypted from the client 
to the server. See Limitations section for details. Note that this will almost
always be your Linux system password too!

Arguably, this is not a problem when you are connecting to drizzled from
localhost and sharing your system username and password for Drizzle can
be quite convenient. 

**Using auth_pam user accounts when connecting over the network is strongly 
discouraged!** We recommend you disable auth_pam on networked Drizzle servers
and instead use :doc:`/plugins/auth_schema/index` or alternatively
:doc:`/plugins/auth_ldap/index` if you are interested in managing usernames
outside of Drizzle.


.. _auth_pam_limitations:

Limitations
-----------

Most Drizzle authentication plugins will use a challenge response protocol
for authentication. In such schemes the client and the server each compute a 
hash that they compare with each other. Thanks to this, the password itself is
never sent over the network and therefore cannot be seen by an eavesdropping
attacker. The auth_pam plugin however needs to use the password in plaintext
format. This limitation is due to the typical configuration of PAM. For 
instance, also when you log in via SSH to your system, the password is sent in 
plaintext from the client to the server. Of course, in the case of SSH the 
communication channel itself is encrypted, so it cannot be eavesdropped.

Which leads us to the next limitation: the :program:`drizzle` client does not
support SSL connections. This means communication between client and server
is sent in unencrypted cleartext over the network - including your password. 
Hopefully a future version of the :program:`drizzle` client will support SSL 
encrypted connections, making auth_pam authentication more useful.

You must use the following parameters to the :program:`drizzle` client to make
sure your password is sent in plaintext to the server::

   drizzle -P --protocol mysql-plugin-auth

The ``-P`` or ``--password`` switch will make drizzle ask for your password 
interactively. The ``--protocol mysql-plugin-auth`` will use a protocol that
sends the password in plaintext.




.. _auth_pam_authors:

Authors
-------

Brian Aker

Documentation by Henrik Ingo.

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

