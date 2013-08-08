EXTRACT DATE FUNCTION
======================

Syntax
------

.. code-block:: mysql

	EXTRACT(field FROM source)

You can extract a field from a datetime. The fields you can extract are:
 * YEAR
 * YEAR_MONTH (year and month)
 * QUARTER
 * MONTH
 * WEEK
 * DAY
 * DAY_HOUR (day and hour)
 * DAY_MINUTE (day and minute)
 * DAY_SECOND (day and second)
 * HOUR
 * HOUR_MINUTE (hour and minute)
 * HOUR_SECOND (hour and second)
 * MINUTE
 * MINUTE_SECOND (minute and second)
 * SECOND
 * MICROSECOND
 * DAY_MICROSECOND
 * HOUR_MICROSECOND
 * MINUTE_MICROSECOND
 * SECOND_MICROSECOND

Examples
--------

.. code-block:: mysql

	drizzle> SELECT EXTRACT(YEAR FROM '1982-01-29');
	+---------------------------------+
	| EXTRACT(YEAR FROM '1982-01-29') |
	+---------------------------------+
	|                            1982 | 
	+---------------------------------+
	1 row in set (0.000494 sec)

.. code-block:: mysql

	drizzle> SELECT EXTRACT(MONTH FROM '1982-01-29');
	+----------------------------------+
	| EXTRACT(MONTH FROM '1982-01-29') |
	+----------------------------------+
	|                                1 | 
	+----------------------------------+
	1 row in set (0.000484 sec)

.. code-block:: mysql

	drizzle> SELECT EXTRACT(YEAR_MONTH FROM '1982-01-29');
	+---------------------------------------+
	| EXTRACT(YEAR_MONTH FROM '1982-01-29') |
	+---------------------------------------+
	|                                198201 | 
	+---------------------------------------+
	1 row in set (0.000492 sec)

.. code-block:: mysql

	drizzle> SELECT EXTRACT(SECOND_MICROSECOND FROM NOW());
	+----------------------------------------+
	| EXTRACT(SECOND_MICROSECOND FROM NOW()) |
	+----------------------------------------+
	|                               13761098 | 
	+----------------------------------------+
	1 row in set (0.000499 sec)
