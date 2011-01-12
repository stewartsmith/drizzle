Drizzle Protocol
================

The Drizzle Protocol plugin allows Drizzle compatible clients to connect to Drizzle (currently this is identical to the MySQL Protocol).

Configuration
-------------

There are several server variables to control the Drizzle Protocol.

**drizzle-protocol.port** arg (=4427)
  The port number to use with Drizzle connections (0 is the same as 4427)

**drizzle-protocol.connect-timeout** arg (=10)
  Connection timeout in seconds

**drizzle-protocol.read-timeout** arg (=30)
  Port read timeout in seconds

**drizzle-protocol.write-timeout** arg (=60)
  Port write timeout in seconds

**drizzle-protocol.retry-count** arg (=10)
  Retry count for the read and write timeout before killing the connection

**drizzle-protocol.buffer-length** arg (=16384)
  Buffer length

**drizzle-protocol.bind-address** arg
  Address to bind to

**drizzle-protocol.max-connections** arg (=1000)
  Maximum simultaneous connections

**drizzle-protocol.admin-ip-addresses** arg
  A comma separated list of IP addresses for admin tools to connect from
