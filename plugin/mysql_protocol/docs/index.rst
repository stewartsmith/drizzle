MySQL Protocol
==============

The MySQL Protocol plugin allows MySQL compatible clients to connect to Drizzle.

Configuration
-------------

There are several server variables to control the MySQL Protocol.

**mysql-protocol.port** arg (=3306)
  The port number to use with MySQL connections (0 is the same as 3306)

**mysql-protocol.connect-timeout** arg (=10)
  Connection timeout in seconds

**mysql-protocol.read-timeout** arg (=30)
  Port read timeout in seconds

**mysql-protocol.write-timeout** arg (=60)
  Port write timeout in seconds

**mysql-protocol.retry-count** arg (=10)
  Retry count for the read and write timeout before killing the connection

**mysql-protocol.buffer-length** arg (=16384)
  Buffer length

**mysql-protocol.bind-address** arg
  Address to bind to

**mysql-protocol.max-connections** arg (=1000)
  Maximum simultaneous connections

**mysql-protocol.admin-ip-addresses** arg
  A comma seprated list of IP addresses for admin tools to connect from
