.. _auth_ldap_plugin:

LDAP Authentication
===================

:program:`auth_ldap` is an authentication plugin that authenticates connections
using an :abbr:`LDAP (Lightweight Directory Access Protocol)` server.  An
LDAP server is required to provide authentication.

Note that a typical use case for using LDAP based authentication, and the
intention with this module, is to be able to consolidate your Drizzle usernames
and passwords in cases where you are already using LDAP in your organization
(such as for Linux or Windows or other system passwords). 

If you are not currently using LDAP for any kind of authentication, you should 
be aware that this is not the simplest authentication method available. For other 
alternatives for managing Drizzle users and passwords, see 
:doc:`/administration/authentication`. A simple authentication module, whose 
behavior will be familiar to those familiar with MySQL and its method for 
storing usernames and passwords, is the :doc:`/plugins/auth_schema/index` plugin.

.. note::

   Unload the :doc:`/plugins/auth_all/index` plugin before using this plugin.

.. seealso:: :doc:`/administration/authentication` 

.. _auth_ldap_loading:

Loading
-------

To load this plugin, start :program:`drizzled` with::

   --plugin-add=auth_ldap

Or, to disable the ability to login without a password, use::

   --plugin-add=auth_pam --plugin-remove=auth_all

Just loading this plugin will not enable or configure it. To actually bind to an
LDAP directory you also need to configure it. See the plugin's
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

   Drizzle uses the ``LDAP_SCOPE_ONELEVEL`` option when searching the LDAP 
   directory. This means you must specify the full base-dn. For instance, if
   you have users defined in the dn ``ou=people,dn=example,dn=com`` authentication
   will fail if you only specify ``dn=example,dn=com``. (See 
   :ref:`auth_ldap_limitations`)

.. option:: --auth-ldap.bind-dn ARG

   :Default: 
   :Variable: :ref:`auth_ldap_bind_dn <auth_ldap_bind_dn>`

   DN to use when binding to the LDAP server.

   Until Drizzle 2011.11.29 (a Drizzle 7.1 beta release) this option was mistakenly
   called ``bind-db``. Starting with release 2011.12.30 that option will no longer
   work, the correct option is ``bind-dn``. (The corresponding variable was
   always ref:`auth_ldap_bind_dn <auth_ldap_bind_dn>` and is unchanged.)
   

.. option:: --auth-ldap.bind-password ARG

   :Default: 
   :Variable: :ref:`auth_ldap_bind_password <auth_ldap_bind_password>`

   Password to use when binding the DN, ie. your LDAP admin password.

.. option:: --auth-ldap.cache-timeout ARG

   :Default: ``600``
   :Variable: :ref:`auth_ldap_cache_timeout <auth_ldap_cache_timeout>`

   How often to empty the users cache. The default is 10 minutes.

   A value of 0 means never: if a user has once connected to Drizzle, his 
   credentials will then be cached until the next restart. Any changes to the 
   LDAP directory, such as changing the password, would not be visible in 
   drizzled as long as it wasn't restarted.

.. option:: --auth-ldap.mysql-password-attribute ARG

   :Default: ``drizzleMysqlUserPassword``
   :Variable: :ref:`auth_ldap_mysql_password_attribute <auth_ldap_mysql_password_attribute>`

   Attribute in LDAP with MySQL hashed password.

   Until Drizzle 2011.11.29 (a Drizzle 7.1 beta release) the default value of this
   option was ``mysqlUserPassword``. Beginning with release 2011.12.30
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

   Note: This variable existed until Drizzle 2011.11.29, in particular it was part
   of the Drizzle 7 stable release. For security reasons this variable has been 
   removed in Drizzle release 2011.12.30, a Drizzle 7.1 beta release. There was 
   no valid reason to expose your LDAP admin password to every Drizzle user. 

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

Setting up an LDAP directory
^^^^^^^^^^^^^^^^^^^^^^^^^^^^

Using and configuring an LDAP server is outside the scope of this manual, but 
for the purpose of showing some examples we need an LDAP server to connect to.
Below are some minimal steps you need to do to have in place first.

The following example was tried on Ubuntu Linux, version 11.04 natty. Some
earlier versions of Ubuntu require more steps to configure your empty LDAP
directory, see `this Ubuntu tutorial for more detailed 
instructions <http://https://help.ubuntu.com/11.04/serverguide/C/openldap-server.html>`_
and similarly see tutorials for your own Linux distribution if those do not work
for you.

To install OpenLDAP:

.. code-block:: bash
   
   sudo apt-get install slapd ldap-utils

The installation asks you to provide an administrator password. In this example
we've used `secret`.

Copy the following text into a file backend.example.com.ldif [1]_:

.. code-block:: none
    
    # Load dynamic backend modules
    dn: cn=module,cn=config
    objectClass: olcModuleList
    cn: module
    olcModulepath: /usr/lib/ldap
    olcModuleload: back_hdb.la

    # Database settings
    dn: olcDatabase=hdb,cn=config
    objectClass: olcDatabaseConfig
    objectClass: olcHdbConfig
    olcDatabase: {1}hdb
    olcSuffix: dc=example,dc=com
    olcDbDirectory: /var/lib/ldap
    olcRootDN: cn=admin,dc=example,dc=com
    olcRootPW: secret
    olcDbConfig: set_cachesize 0 2097152 0
    olcDbConfig: set_lk_max_objects 1500
    olcDbConfig: set_lk_max_locks 1500
    olcDbConfig: set_lk_max_lockers 1500
    olcDbIndex: objectClass eq
    olcLastMod: TRUE
    olcDbCheckpoint: 512 30
    olcAccess: to attrs=userPassword by dn="cn=admin,dc=example,dc=com" write by anonymous auth by self write by * none
    olcAccess: to attrs=shadowLastChange by self write by * read
    olcAccess: to dn.base="" by * read
    olcAccess: to * by dn="cn=admin,dc=example,dc=com" write by * read

Copy the following text into a file frontend.example.com.ldif:

.. code-block:: none
    
    # Create top-level object in domain
    dn: dc=example,dc=com
    objectClass: top
    objectClass: dcObject
    objectclass: organization
    o: Example Organization
    dc: Example
    description: LDAP Example 

    # Admin user.                                                                                                                                                                       
    dn: cn=admin,dc=example,dc=com                                                                                                                                                      
    objectClass: simpleSecurityObject                                                                                                                                                   
    objectClass: organizationalRole                                                                                                                                                     
    cn: admin                                                                                                                                                                           
    description: LDAP administrator                                                                                                                                                     
    userPassword: secret

    dn: ou=people,dc=example,dc=com
    objectClass: organizationalUnit
    ou: people

    dn: ou=groups,dc=example,dc=com
    objectClass: organizationalUnit
    ou: groups

    dn: uid=john,ou=people,dc=example,dc=com
    objectClass: inetOrgPerson
    objectClass: posixAccount
    objectClass: shadowAccount
    uid: john
    sn: Doe
    givenName: John
    cn: John Doe
    displayName: John Doe
    uidNumber: 1000
    gidNumber: 10000
    userPassword: password
    gecos: John Doe
    loginShell: /bin/bash
    homeDirectory: /home/john
    shadowExpire: -1
    shadowFlag: 0
    shadowWarning: 7
    shadowMin: 8
    shadowMax: 999999
    shadowLastChange: 10877
    mail: john.doe@example.com
    postalCode: 31000
    l: Toulouse
    o: Example
    mobile: +33 (0)6 xx xx xx xx
    homePhone: +33 (0)5 xx xx xx xx
    title: System Administrator
    postalAddress: 
    initials: JD

    dn: cn=example,ou=groups,dc=example,dc=com
    objectClass: posixGroup
    cn: example
    gidNumber: 10000

Now we create our database and settings, along with the standard 
"inetOrgPerson" LDAP schema:

.. code-block:: none
    
    $ sudo ldapadd -Y EXTERNAL -H ldapi:/// -f backend.example.com.ldif
    SASL/EXTERNAL authentication started
    SASL username: gidNumber=0+uidNumber=0,cn=peercred,cn=external,cn=auth
    SASL SSF: 0
    adding new entry "cn=module,cn=config"

    adding new entry "olcDatabase=hdb,cn=config"

    $ sudo ldapadd -x -D cn=admin,dc=example,dc=com -W -f frontend.example.com.ldif
    Enter LDAP Password: secret
    adding new entry "dc=example,dc=com"

    adding new entry "cn=admin,dc=example,dc=com"

    adding new entry "ou=people,dc=example,dc=com"

    adding new entry "ou=groups,dc=example,dc=com"

    adding new entry "uid=john,ou=people,dc=example,dc=com"

    adding new entry "cn=example,ou=groups,dc=example,dc=com"

In the above we first created the database and defined a method to access it.
As you see, in the second ldapadd command we now need to provide the admin 
password `secret` to do further changes, and will need to use it in all further
commands too.

The second command creates a classic `inetOrgPerson` schema, with a user
"John Doe" (Common Name) who has a uid "john" and various other information
commonly part of a UNIX system account. In fact the LDAP object type is called
posixAccount! User john is part of the Organizational Unit "people" in the
domain example.com.

You can verify that everything is working so far by searching for John:

.. code-block:: none
    
    $ ldapsearch -xLLL -b "ou=people,dc=example,dc=com" uid=john
    dn: uid=john,ou=people,dc=example,dc=com
    objectClass: inetOrgPerson
    objectClass: posixAccount
    objectClass: shadowAccount
    uid: john
    sn: Doe
    givenName: John
    cn: John Doe
    displayName: John Doe
    uidNumber: 1000
    gidNumber: 10000
    gecos: John Doe
    loginShell: /bin/bash
    homeDirectory: /home/john
    shadowExpire: -1
    shadowFlag: 0
    shadowWarning: 7
    shadowMin: 8
    shadowMax: 999999
    shadowLastChange: 10877
    mail: john.doe@example.com
    postalCode: 31000
    l: Toulouse
    o: Example
    mobile: +33 (0)6 xx xx xx xx
    homePhone: +33 (0)5 xx xx xx xx
    title: System Administrator
    postalAddress:
    initials: JD

If you look closely you see that the userPassword field is not shown. Don't 
worry! It is stored in the directory, it is just not shown in search results for
security reasons.

.. _auth_ldap_examples_add_user:

Adding a Drizzle user to LDAP
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

You could just setup Drizzle to authenticate against standard LDAP accounts like
John Doe above. But the recommended way is to add a specific Drizzle schema.
You will find this in ``$DRIZZLE_ROOT/share/drizzle/drizzle_openldap.ldif``.
You can add it to your LDAP schema like this:

.. code-block:: none
    
    $ sudo ldapadd -Y EXTERNAL -H ldapi:/// -f share/drizzle/drizzle_openldap.ldif 
    SASL/EXTERNAL authentication started
    SASL username: gidNumber=0+uidNumber=0,cn=peercred,cn=external,cn=auth
    SASL SSF: 0
    adding new entry "cn=drizzle,cn=schema,cn=config"

Now we can add a Drizzle user to our directory. At this point we will need to 
store the users Drizzle password. Note that Drizzle, just like MySQL, will
prefer to store and use a doubly hashed version of the user password. Other 
Drizzle authentication plugins, like auth_schema, do the same. (But some plugins
do not and Drizzle can use either, since it supports two different 
authentication protocols for this purpose). 

Drizzle 7.1 ships with a nice utility to calculate those hashes called
``drizzle_password_hash``. You simply give it the password and it outputs
the doubly hashed string:

.. code-block:: none
    
    $ bin/drizzle_password_hash secret
    14E65567ABDB5135D0CFD9A70B3032C179A49EE7

We will use this utility when creating the LDAP entry for our Drizzle user.

Note that the above value is different from what the LDAP directory as the
userPassword entry. The Unix or Posix way to store passwords is to just hash
them once. You can have a look in your ``/etc/shadow`` file to see what they
look like. Anyway, for this reason our Drizzle schema that we just added has
an additional field ``drizzleUserPassword`` to store the Drizzle encoded form
of the same password. (Or the passwords can also be different, but we will
assume most people like to use the same password.)

Since Drizzle 7.1 there is also a nice helper script included to create the ldif
records you need to add new Drizzle users to your LDAP. Using this script is
of course voluntary and you can use any LDAP manager tool you want. But we will
use it for this tutorial.

Let's create the user hingo:

.. code-block:: none
    
    $ share/drizzle/drizzle_create_ldap_user -p secret -b bin/drizzle_password_hash -u hingo -n 1 -l "ou=people,dc=example,dc=com" > hingo.example.com.ldif
    $ cat hingo.example.com.ldif 
    dn: uid=hingo,ou=people,dc=example,dc=com                                                                                                                                           
    objectclass: top                                                                                                                                                                    
    objectclass: posixAccount
    objectclass: account
    objectclass: drizzleUser
    drizzleUserMysqlPassword: 14E65567ABDB5135D0CFD9A70B3032C179A49EE7
    uidNumber: 500
    gidNumber: 500
    uid: hingo
    homeDirectory: /home/hingo
    loginshell: /sbin/nologin
    userPassword: secret
    cn: hingo

If you want, you could use this as a template to further edit the entry.
Drizzle will only care about the `drizzleUserMySQLPassword`, `uid` and sometimes
(at your option) the `userPassword`. So you can freely edit the rest of the 
entries to suit you. For instance if this user will also be a user on your Linux 
system, make sure to set the loginshell to ``/bin/bash`` and check the uid and
gid numbers. The ``cn`` field is often used to store the full name of the person,
like "Henrik Ingo". (But this is not used by Drizzle.)

We now add the above user to the directory:

.. code-block:: none

    $ sudo ldapadd -x -D cn=admin,dc=example,dc=com -W -f hingo.example.com.ldif 
    Enter LDAP Password: 
    adding new entry "uid=hingo,ou=people,dc=example,dc=com"

.. _auth_ldap_examples_start_server:

Starting Drizzle Server and binding to the LDAP server
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

It is now time to start the Drizzle server with the needed options so that we
can use the LDAP directory for authentication services:

.. code-block:: none

    $ sbin/drizzled --plugin-remove=auth_all 
                    --plugin-add=auth_ldap 
                    --auth-ldap.bind-password=secret 
                    --auth-ldap.bind-dn="cn=admin,dc=example,dc=com" 
                    --auth-ldap.base-dn="ou=people,dc=example,dc=com"

`(Give all options on one line.)`

``bind-password`` and ``bind-dn`` are used by drizzled to bind to the LDAP
server. ``base-dn`` is the DN where our Drizzle users are stored.

.. _auth_ldap_examples_connect:

Connecting to Drizzle with the client
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

We can now use a username and password from the LDAP directory when connecting 
to Drizzle:

.. code-block:: none

    $ bin/drizzle --user=hingo --password
    Enter password: 
    Welcome to the Drizzle client..  Commands end with ; or \g.
    Your Drizzle connection id is 2
    Connection protocol: mysql
    Server version: 2011.10.28.2459 Source distribution (drizzle-auth_ldap-fix-and-docs)

    Type 'help;' or '\h' for help. Type '\c' to clear the buffer.

    drizzle> 

.. _auth_ldap_examples_connect_clear_password:

Using the userPassword system password with Drizzle
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

It is also possible to use the password from the userPassword field when 
connecting with Drizzle. This could be beneficial or necessary to allow
all users who already exist in the directory, but didn't have a 
drizzleUserPassword set for them, to connect to Drizzle.

To do this, you have to give the extra option ``--protocol mysql-plugin-auth``
to the drizzle client. This will tell the drizzle client to send the password
in cleartext to the server, using the MySQL old-password protocol.

We could use this to connect to Drizzle with the username john, that
we added in the beginning of this tutorial.

.. code-block:: none

    $ drizzle --password --protocol mysql-plugin-auth --user=john
    Enter password: 
    Welcome to the Drizzle client..  Commands end with ; or \g.
    Your Drizzle connection id is 2
    Connection protocol: mysql-plugin-auth
    Server version: 2011.10.28.2459 Source distribution (drizzle-auth_ldap-fix-and-docs)

    Type 'help;' or '\h' for help. Type '\c' to clear the buffer.

    drizzle> 

.. note::

   Using cleartext passwords is **not recommended**. Please note that
   the connection between drizzle client and drizzled server is completely
   unencrypted, so other people on your network could easily find out the
   password if this method is used.


.. _auth_ldap_limitations:

Limitations
-----------

The option ``LDAP_SCOPE_ONELEVEL`` option is used when searching the LDAP 
directory. This means you must specify the full base-dn. For instance, if
you have users defined in the dn ``ou=people,dn=example,dn=com`` authentication
will fail if you only specify ``dn=example,dn=com``. A consequence of this is
that all your Drizzle users must belong to the same LDAP organizationalUnit.

This is currently a fixed option and can only be changed by editing source code. 
However, there is no reason why it couldn't be a configurable option to also
allow multi level searches. Please contact the Drizzle developers if you have
such needs. (See :doc:`/help`)


.. _auth_ldap_authors:

Authors
-------

:Code: Eric Day, Edward "Koko" Konetzko, Henrik Ingo
:Documentation: Henrik Ingo

.. _auth_ldap_version:

Version
-------

This documentation applies to **auth_ldap 0.2**.

To see which version of the plugin a Drizzle server is running, execute:

.. code-block:: mysql

   SELECT MODULE_VERSION FROM DATA_DICTIONARY.MODULES WHERE MODULE_NAME='auth_ldap'

Changelog
---------

v0.2
^^^^
* Add proper documentation.
* Fix various bugs found while documenting, including:
* drizzle_create_ldap_user would append a counter at the end of each username, such as hingo0. Now it's just the username.
* LDAP directory is now searched for uid field, not cn.
* Change default value of --auth-ldap.mysql-password-attribute to drizzleMysqlUserPassword.
* --auth-ldap.bind-db was changed to --auth-ldap.bind-dn
* Variable auth_ldap_bind_password is no longer shown in SHOW VARIABLES.


v0.1
^^^^
* First release.

.. [1] Configuration scripts courtesy of `Ubuntu OpenLDAP server tutorial <https://help.ubuntu.com/11.04/serverguide/C/openldap-server.html>`_