DATE TRUNC
===========

DATE_TRUNC truncates a date to a specified precision. The idea is similar to the trunc function for numbers.

The syntax is: date_trunc(text, source)

Valid values for *text* are:

* microseconds
* milliseconds
* second
* minute
* hour
* day
* week
* month
* quarter
* year
* decade
* century
* millennium

In this example, 'source' is a TIMESTAMP value with microsecond precision:

.. code-block:: mysql

	date_trunc('hour', timestamp '2011-02-26 10:35:44:712005')

Returns: 2011-02-26 10:00:00
