Data and Time Data Types
========================

Drizzle supports a complete set of SQL date and time types; operations available on these data types are described in the next section, on Functions.

**Date/Time Types**

+------------+-----------------------+---------------------+-------------+
|Data Type   |"Zero" Value           |Description	   |Storage Size |
+============+=======================+=====================+=============+
|TIMESTAMP   |'0000-00-00 00:00:00'  |both date and time   |8 bytes      |
+------------+-----------------------+---------------------+-------------+
|DATE        |'0000-00-00'           |dates only           |4 bytes      |
+------------+-----------------------+---------------------+-------------+
|TIME        |'00:00:00'             |time of day          |8 bytes      |
+------------+-----------------------+---------------------+-------------+
|DATETIME    |'0000-00-00 00:00:00'  |both date and time   |8 bytes      |
+------------+-----------------------+---------------------+-------------+
