MySQL Protocol
==============

The MySQL Protocol plugin allows MySQL compatible clients to connect to Drizzle.

Configuration
-------------

There are several server variables to control the MySQL Protocol.

.. program:: drizzled

.. option:: --mysql-protocol.port arg (=3306)

   The port number to use with MySQL connections (0 is the same as 3306)

.. option:: --mysql-protocol.connect-timeout arg (=10)

   Connection timeout in seconds

.. option:: --mysql-protocol.read-timeout arg (=30)

   Port read timeout in seconds

.. option:: --mysql-protocol.write-timeout arg (=60)

   Port write timeout in seconds

.. option:: --mysql-protocol.retry-count arg (=10)

   Retry count for the read and write timeout before killing the connection

.. option:: --mysql-protocol.buffer-length arg (=16384)

   Buffer length

.. option:: --mysql-protocol.bind-address arg

   Address to bind to

.. option:: --mysql-protocol.max-connections arg (=1000)

   Maximum simultaneous connections

.. option:: --mysql-protocol.admin-ip-addresses arg

   A comma seprated list of IP addresses for admin tools to connect from
