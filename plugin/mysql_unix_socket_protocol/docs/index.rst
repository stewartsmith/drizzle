MySQL Unix Socket Protocol
==========================

The MySQL Unix Socket Protocol plugin allows MySQL compatible clients which
use the unix socket file to connect to Drizzle.

Configuration
-------------

There are several server variables to control the MySQL Unix Socket Protocol.

.. program:: drizzled

.. option:: --mysql-unix-socket-protocol.path (=/tmp/mysql.socket)

   The path used for the socket file

.. option:: --mysql-unix-socket-protocol.clobber

   Remove the socket file if one already exists

.. option:: --mysql-unix-socket-protocol.max-connections (=1000)

   The maximum simultaneous connections via. the unix socket protocol
