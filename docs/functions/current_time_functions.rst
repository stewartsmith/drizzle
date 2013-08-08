CURRENT TIME FUNCTIONS
=======================

====================    ================================
Function                           Description
====================    ================================
NOW()                   Returns the current time (UTC)
UTC_TIMESTAMP()         Synonym for NOW()
CURRENT_TIMESTAMP()     Synonym for NOW()
====================    ================================

.. _now:

NOW
---

You can call `NOW()`, `UTC_TIMESTAMP()` or `CURRENT_TIMESTAMP()` to get the current time. All time in Drizzle is UTC.

.. code-block:: mysql

	drizzle> select NOW(), UTC_TIMESTAMP(), CURRENT_TIMESTAMP()\G
	*************************** 1. row ***************************
                      now(): 2013-08-08 06:11:41.442568
	    UTC_timestamp(): 2013-08-08 06:11:41.442568
	current_timestamp(): 2013-08-08 06:11:41.442568
	1 row in set (0.000468 sec)
