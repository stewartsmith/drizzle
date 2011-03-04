Numeric Data Types
==================

``BIGINT`` and ``INTEGER`` exist as Drizzle's two integer numerical types.
``BIGINT`` is a 64-bit integer, while ``INTEGER`` is a 32-bit integer.

:program:`Drizzle` does not support ``TINYINT``, ``SMALLINT`` or ``MEDIUMINT``.
Integer operations have been optimized around 32- and 64-bit integers.

+---------+--------------+---------------------+--------------------------------------------+
|Name     |Storage Size  |Description          |Range                                       |
+=========+==============+=====================+============================================+
|integer  |4 bytes	 |most common integer  |-2147483648 to +2147483647                  |
+---------+--------------+---------------------+--------------------------------------------+
|bigint	  |8 bytes	 |larger-range integer |-9223372036854775808 to 9223372036854775807 |
+---------+--------------+---------------------+--------------------------------------------+

``AUTO_INCREMENT`` is supported for ``INT`` and ``BIGINT``.

``DOUBLE`` is the system's native double type. ``DOUBLE`` represents
double-precision floating-point values that require eight bytes each for 
storage.
