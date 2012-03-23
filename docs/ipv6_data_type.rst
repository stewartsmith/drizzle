IPV6 Data Type
==============

The data type IPV6 stores Internet Protocol version(IPv) 4 & 6 addresses. [1]_

About IPv6
----------

IPv6 address are written in preferred form as x:x:x:x:x:x:x:x, where the 'x's 
are the hexadecimal values of the eight 16 bit pieces of the address separated 
by colons. Leading zeros in a group are allowed to be dropped, upper and lower 
case are equivalent. For example:

         1080:0:0:0:8:800:200C:417A  a unicast address
         FF01:0:0:0:0:0:0:101        a multicast address
         0:0:0:0:0:0:0:1             the loopback address
         0:0:0:0:0:0:0:0             the unspecified addresses

may be represented as:

         1080::8:800:200C:417A       a unicast address
         FF01::101                   a multicast address
         ::1                         the loopback address
         ::                          the unspecified addresses

The IPV6 data type supports storing also IPv4 address in both formats. 

For example:

::192.168.1.10 or 192.168.1.10


Example usage
-------------

Creating a database schema and changing to using the schema as default.

.. code-block:: mysql

        CREATE SCHEMA ipaddress;
        use ipaddress;


Creating a table with a IPV6 column:

.. code-block:: mysql

        CREATE TABLE ipaddress_table (addr IPV6);

Adding data to the table:

.. code-block:: mysql

        INSERT INTO ipaddress_table (addr) 
        VALUES ("fe8::b3ff:fe1a:8329");

The above command adds one row to the database. IPv4 addresses can be inserted 
to the same column as well. The IPV6 data type handles the distinction between 
IPv4 & IPv6 addresses internally:

.. code-block:: mysql

        INSERT INTO ipaddress_table (address) 
        VALUES ("192.168.100.10");

Querying the table:

.. code-block:: mysql

        SELECT * FROM ipaddress_table;         

        address                      
        0fe8:0000:0000:0000:0000:beff:fe1a:8329
        000:000:000:000:000:000:192.168.100.010

Authors
-------

:Code: Muhammad Umair, Mark Atwood
:Documentation: Muhammad Umair, Henrik Ingo


.. rubric:: Footnotes

.. [1] IPv6 is documented as part of the IETF Standard Track `RFC 2373 <http://www.ietf.org/rfc/rfc2373.txt>`_.

