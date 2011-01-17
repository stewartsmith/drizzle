Date and Time Data Types
========================

Of the SQL date and time types, Drizzle supports:

**Date/Time Types**

+------------+---------------------------+-----------------------+------------------------+---------------+---------------+
|Data Type   |Lowest Value (or NULL)     |Maximum Value          |Description	          |Storage Size   |Resolution     |
+============+===========================+=======================+========================+===============+===============+
|TIMESTAMP   |'0001-01-01 00:00:00'      |'9999-12-31 23:59:59'  |both date and time      |8 bytes        |1 microsecond  |
+------------+---------------------------+-----------------------+------------------------+---------------+---------------+
|DATE        |'0001-01-01'               |'9999-12-31'           |dates only              |4 bytes        |1 day          +
+------------+---------------------------+-----------------------+------------------------+---------------+---------------+
|TIME        |'00:00:00'                 |'23:59:59'             |time of day             |8 bytes        |1 microsecond  +
+------------+---------------------------+-----------------------+------------------------+---------------+---------------+
|DATETIME    |'0001-01-01 00:00:00'      |'9999-12-31 23:59:59'  |both date and time      |8 bytes        |1 microsecond  |
+------------+---------------------------+-----------------------+------------------------+---------------+---------------+

TIMESTAMP
---------

To create a TIMESTAMP column that uses microseconds you simply need to specify TIMESTAMP in your table definition, for example: ::

	CREATE TABLE `t1` (
	`a` INT DEFAULT NULL,
	`b` TIMESTAMP NULL DEFAULT NULL
	) ENGINE=InnoDB

You can then use the following (but note that ON DEFAULT/UPDATE CURRENT_TIMESTAMP works with microseconds as well): ::

	insert into t1 values (1, '2010-01-10 07:32:43.234567');

The new table now looks like this:

+------+----------------------------+
|a     |b                           |
+------+----------------------------+
|1     |2010-01-10 07:32:43.234567  |
+------+----------------------------+

DATE
----

In Drizzle, valid date inputs begin at 0001-01-01 rather than 0000-00-00, which is not a valid date (there was no year 1, there is no month zero and there is no day zero).

TIME
----

Drizzle's TIME data type has a range of 00:00:00 - 24:59:59, while MySQL's TIME data type has a range of -838:59:59 - 838:59:59.

FIXME: should be to 23hrs. in fact, to be compliant, it should be to at a minimum 23:59:61.999999.

FIXME: should also mention that this brings Drizzle closer to the SQL standard. Negative time is not meant to be there at all.

To prevent data loss to this type when converting from MySQL -> Drizzle, the conversion changes TIME to an INT of the number of seconds. For example, 00:00:00 becomes 0, 01:00:00 becomes 3600, and -01:00:00 becomes -3600.
