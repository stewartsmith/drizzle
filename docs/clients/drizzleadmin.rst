Drizzle Admin Commandline Client
================================

The :program:`drizzleadmin` command line client is the primary program for
connecting to and manipulating a Drizzle database with an administrative user rights.

The :program:`drizzleadmin` tool can only connect using the 'root' user and
only on IP addresses defined by drizzle-protocol.admin-ip-addresses or 
mysql-protocol.admin-ip-addresses.  At the moment the :program:`drizzleadmin` 
tool is only to be used to bypass the protocol's max-connections setting to do
tasks such as killing queries or clients.  Eventually more administrative
features will be added.

.. seealso::
   :doc:`drizzle`
