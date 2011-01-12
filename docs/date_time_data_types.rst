Date and Time Data Types
========================

Drizzle supports a complete set of SQL date and time types; operations available on these data types are described in the next section, on Functions.

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

Drizzle's TIME data type has a range of 00:00:00 - 24:59:59, while MySQL's TIME data type has a range of -838:59:59 - 838:59:59.
To prevent data loss to this type when converting from MySQL -> Drizzle, the conversion changes TIME to an INT of the number of seconds. For example, 00:00:00 becomes 0, 01:00:00 becomes 3600, and -01:00:00 becomes -3600.