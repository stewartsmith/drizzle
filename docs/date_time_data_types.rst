Date and Time Data Types
========================

Of the SQL date and time types, Drizzle supports:

**Date/Time Types**

+------------+------------------------------+------------------------------+------------------------+---------------+---------------+
|Data Type   |Lowest Value (or NULL)        |Maximum Value                 |Description             |Storage Size   |Resolution     |
+============+==============================+==============================+========================+===============+===============+
|TIMESTAMP   |'0001-01-01 00:00:00'         |'9999-12-31 23:59:59'         |both date and time      |4 bytes        |1 second       |
+------------+------------------------------+------------------------------+------------------------+---------------+---------------+
|TIMESTAMP(6)|'0001-01-01 00:00:00.000000'  |'9999-12-31 23:59:59.999999'  |both date and time      |8 bytes        |1 microsecond  |
+------------+------------------------------+------------------------------+------------------------+---------------+---------------+
|DATE        |'0001-01-01'                  |'9999-12-31'                  |dates only              |4 bytes        |1 day          |
+------------+------------------------------+------------------------------+------------------------+---------------+---------------+
|TIME        |'00:00:00'                    |'23:59:59'                    |time of day             |4 bytes        |1 second       |
+------------+------------------------------+------------------------------+------------------------+---------------+---------------+
|DATETIME    |'0001-01-01 00:00:00'         |'9999-12-31 23:59:59'         |both date and time      |4 bytes        |1 second       |
+------------+------------------------------+------------------------------+------------------------+---------------+---------------+

TIMESTAMP and TIMESTAMP(6)
--------------------------

The regular TIMESTAMP data type does not store fractional seconds, and uses 4 bytes of storage.

To create a TIMESTAMP column that uses microseconds you simply need to specify TIMESTAMP(6) in your table definition. The (6) stands for microsecond granularity (since a microsecond is one millionth of a second). This means that fractional seconds are stored and returned with the field, and it uses 4 more bytes of storage than TIMESTAMP.

For example:

.. code-block:: mysql

	CREATE TABLE `t1` (
	`a` INT DEFAULT NULL,
	`b` TIMESTAMP(6) NULL DEFAULT NULL
	) ENGINE=InnoDB

You can then use the following (but note that ON DEFAULT/UPDATE CURRENT_TIMESTAMP works with microseconds as well):

.. code-block:: mysql

	insert into t1 values (1, '2010-01-10 07:32:43.234567');

The new table now looks like this:

+------+----------------------------+
|a     |b                           |
+======+============================+
|1     |2010-01-10 07:32:43.234567  |
+------+----------------------------+

DATE
----

In Drizzle, valid date inputs begin at 0001-01-01 rather than 0000-00-00, which is not a valid date (there was no year 1, there is no month zero and there is no day zero).

TIME
----

Drizzle's TIME data type has a range of 00:00:00 - 23:59:59, while MySQL's TIME data type has a range of -838:59:59 - 838:59:59.

This brings Drizzle closer to the SQL standard. Negative time is not meant to be supported.

To prevent data loss to this type when converting from MySQL -> Drizzle, the conversion process changes TIME to an INT of the number of seconds. For example, 00:00:00 becomes 0, 01:00:00 becomes 3600, and -01:00:00 becomes -3600.

More information on this can be found in the :ref:`drizzledump-migration-label`
section of this documentation.

DATETIME
--------

DATETIME defines a date that is combined with a time of day, based on 24-hour time. Unlike TIMESTAMP in that it does not support microseconds.


