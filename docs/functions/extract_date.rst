Extract Functions
=================

EXTRACT(field FROM source)
The extract function retrieves subfields such as year or hour from date/time values. source must be a value expression of type timestamp, time, or interval. (Expressions of type date are cast to timestamp and can therefore be used as well.) field is an identifier or string that selects what field to extract from the source value. The extract function returns values of type double precision. The following are valid field names:

day
The day (of the month) field (1 - 31)
SELECT EXTRACT(DAY FROM TIMESTAMP '2001-02-16 20:38:40');
Result: 16
decade
The year field divided by 10
SELECT EXTRACT(DECADE FROM TIMESTAMP '2001-02-16 20:38:40');
Result: 200
dow
The day of the week as Sunday(0) to Saturday(6)
SELECT EXTRACT(DOW FROM TIMESTAMP '2001-02-16 20:38:40');
Result: 5
Note that extract's day of the week numbering differs from that of the to_char(..., 'D') function.
doy
The day of the year (1 - 365/366)
SELECT EXTRACT(DOY FROM TIMESTAMP '2001-02-16 20:38:40');
Result: 47
epoch
For date and timestamp values, the number of seconds since 1970-01-01 00:00:00 UTC (can be negative); for interval values, the total number of seconds in the interval
SELECT EXTRACT(EPOCH FROM TIMESTAMP WITH TIME ZONE '2001-02-16 20:38:40.12-08');
Result: 982384720.12

SELECT EXTRACT(EPOCH FROM INTERVAL '5 days 3 hours');
Result: 442800
Here is how you can convert an epoch value back to a time stamp:
SELECT TIMESTAMP WITH TIME ZONE 'epoch' + 982384720.12 * INTERVAL '1 second';
(The to_timestamp function encapsulates the above conversion.)
hour
The hour field (0 - 23)
SELECT EXTRACT(HOUR FROM TIMESTAMP '2001-02-16 20:38:40');
Result: 20
isodow
The day of the week as Monday(1) to Sunday(7)

microseconds
The seconds field, including fractional parts, multiplied by 1 000 000; note that this includes full seconds
SELECT EXTRACT(MICROSECONDS FROM TIME '17:12:28.5');
Result: 28500000
millennium
The millennium
SELECT EXTRACT(MILLENNIUM FROM TIMESTAMP '2001-02-16 20:38:40');
Result: 3
Years in the 1900s are in the second millennium. The third millennium started January 1, 2001.
PostgreSQL releases before 8.0 did not follow the conventional numbering of millennia, but just returned the year field divided by 1000.

milliseconds
The seconds field, including fractional parts, multiplied by 1000. Note that this includes full seconds.
SELECT EXTRACT(MILLISECONDS FROM TIME '17:12:28.5');
Result: 28500
minute
The minutes field (0 - 59)
SELECT EXTRACT(MINUTE FROM TIMESTAMP '2001-02-16 20:38:40');
Result: 38
month
For timestamp values, the number of the month within the year (1 - 12) ; for interval values the number of months, modulo 12 (0 - 11)
SELECT EXTRACT(MONTH FROM TIMESTAMP '2001-02-16 20:38:40');
Result: 2

SELECT EXTRACT(MONTH FROM INTERVAL '2 years 3 months');
Result: 3

SELECT EXTRACT(MONTH FROM INTERVAL '2 years 13 months');
Result: 1
quarter
The quarter of the year (1 - 4) that the date is in
SELECT EXTRACT(QUARTER FROM TIMESTAMP '2001-02-16 20:38:40');
Result: 1
second
The seconds field, including fractional parts (0 - 59[1])
SELECT EXTRACT(SECOND FROM TIMESTAMP '2001-02-16 20:38:40');
Result: 40

SELECT EXTRACT(SECOND FROM TIME '17:12:28.5');
Result: 28.5
timezone
The time zone offset from UTC, measured in seconds. Positive values correspond to time zones east of UTC, negative values to zones west of UTC.

timezone_hour
The hour component of the time zone offset

timezone_minute
The minute component of the time zone offset

week
The number of the week of the year that the day is in. By definition (ISO 8601), the first week of a year contains January 4 of that year. (The ISO-8601 week starts on Monday.) In other words, the first Thursday of a year is in week 1 of that year.
Because of this, it is possible for early January dates to be part of the 52nd or 53rd week of the previous year. For example, 2005-01-01 is part of the 53rd week of year 2004, and 2006-01-01 is part of the 52nd week of year 2005.
SELECT EXTRACT(WEEK FROM TIMESTAMP '2001-02-16 20:38:40');
Result: 7
year
The year field. Keep in mind there is no 0 AD, so subtracting BC years from AD years should be done with care.
SELECT EXTRACT(YEAR FROM TIMESTAMP '2001-02-16 20:38:40');
Result: 2001

The extract function is primarily intended for computational processing. For formatting date/time values for display, see Section 9.8.
The date_part function is modeled on the traditional Ingres equivalent to the SQL-standard function extract:
date_part('field', source)
Note that here the field parameter needs to be a string value, not a name. The valid field names for date_part are the same as for extract.
SELECT date_part('day', TIMESTAMP '2001-02-16 20:38:40');
Result: 16

SELECT date_part('hour', INTERVAL '4 hours 3 minutes');
Result: 4
