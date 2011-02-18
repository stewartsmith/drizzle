Extract Functions
=================

**Syntax** ::

	EXTRACT(field FROM source)

The EXTRACT function retrieves subfields such as day or hour from date/time values. The source value has to be a value expression of type *timestamp*, *time*, *date*, or *interval*. 

*Field* is an identifier or string that identifies the field to extract from the source value. The extract function returns values of type *double precision*. 

**Examples:**

The following field names are available:

**day**

.. code-block:: mysql
	
	SELECT EXTRACT(DAY FROM TIMESTAMP '2001-02-16 20:38:40');

Day of the month.

*Result: 16*

**decade**

.. code-block:: mysql
	
	SELECT EXTRACT(DECADE FROM TIMESTAMP '2001-02-16 20:38:40');

The year, divided by 10.

*Result: 200*

**dow**

.. code-block:: mysql
	
	SELECT EXTRACT(DOW FROM TIMESTAMP '2001-02-16 20:38:40');

The day of the week (Sunday is 0, Saturday is 6)

*Result: 5*

**doy**

.. code-block:: mysql

	SELECT EXTRACT(DOY FROM TIMESTAMP '2001-02-16 20:38:40');

The day of the year (1 - 365/366)

*Result: 47*

**hour**

.. code-block:: mysql

	SELECT EXTRACT(HOUR FROM TIMESTAMP '2001-02-16 20:38:40');

The hour field (0 - 23)

*Result: 20*

**microseconds**

.. code-block:: mysql

	SELECT EXTRACT(MICROSECONDS FROM TIME '17:12:28.5');

The seconds field, including fractional parts, multiplied by 1 000 000; note that this includes full seconds

*Result: 28500000*

**minute**

.. code-block:: mysql

	SELECT EXTRACT(MINUTE FROM TIMESTAMP '2001-02-16 20:38:40');

The minutes field (0 - 59)	

*Result: 38*

**month**

For timestamp values, the number of the month within the year (1 - 12). 
For interval values, the number of months (0 - 11).

.. code-block:: mysql

	SELECT EXTRACT(MONTH FROM TIMESTAMP '2010-12-29 08:45:27');

*Result: 12*

.. code-block:: mysql

	SELECT EXTRACT(MONTH FROM INTERVAL '3 years 4 months');

*Result: 4*

.. code-block:: mysql

	SELECT EXTRACT(MONTH FROM INTERVAL '3 years 13 months');

*Result: 1*

**quarter**

.. code-block:: mysql

	SELECT EXTRACT(QUARTER FROM TIMESTAMP '2010-12-29 08:45:27');

The quarter of the year (1 - 4) containing the date.
	
*Result: 4*

**second**

.. code-block:: mysql

	SELECT EXTRACT(SECOND FROM TIMESTAMP '2010-12-29 08:45:27');

The seconds field, including fractional parts (0 - 59)
	
*Result: 27*

.. code-block:: mysql

	SELECT EXTRACT(SECOND FROM TIME '08:15:22.5');

*Result: 22.5*

**timezone**

The time zone offset from UTC, measured in seconds.

**week**

Returns the week number that a day is in. Weeks are numbered according to ISO 8601:1988.

ISO 8601:1988 means that if the week containing January 1 has four or more days in the new year, then it is week 1; otherwise it is the last week of the previous year, and the next week is week 1. The ISO-8601 week starts on Monday.

It's possible for early January dates to be part of the 52nd or 53rd week of the previous year. For example, 2011-01-01 was part of the 52nd week of year 2010.

.. code-block:: mysql

	SELECT EXTRACT(WEEK FROM TIMESTAMP '2010-01-25 12:44:06');

*Result: 4*

**year**

.. code-block:: mysql

	SELECT EXTRACT(YEAR FROM TIMESTAMP '2009-02-16 20:38:40');

*Result: 2009*

The valid field names for date_part are the same as for extract.

.. code-block:: mysql

	SELECT date_part('day', TIMESTAMP '2010-07-16 10:12:05');

*Result: 16*

.. code-block:: mysql

	SELECT date_part('hour', INTERVAL '5 hours 12 minutes');

*Result: 4*
