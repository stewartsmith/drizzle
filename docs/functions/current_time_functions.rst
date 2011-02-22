CURRENT TIME FUNCTIONS
=======================


current_date
-------------

Returns the current date as a value in 'YYYY-MM-DD' or YYYYMMDD format, depending on whether the function is used in a string or numeric context. ::

	SELECT CURDATE();
        	-> '2008-06-13'
	SELECT CURDATE() + 0;
        	-> 20080613

current_time
--------------

Returns the current time as a value in 'HH:MM:SS' or HHMMSS.uuuuuu format, depending on whether the function is used in a string or numeric context. The value is expressed in the current time zone. ::

	SELECT CURTIME();
        	-> '23:50:26'
	SELECT CURTIME() + 0;
        	-> 235026.000000


current_timestamp
------------------

See :ref:`now`

localtime
-----------

See :ref:`now`

localtimestamp	                   
---------------

See :ref:`now`

.. _now:

now()	                            
------

Returns the current date and time as a value in 'YYYY-MM-DD HH:MM:SS' or YYYYMMDDHHMMSS.uuuuuu format, depending on whether the function is used in a string or numeric context. The value is expressed in the current time zone. ::

	SELECT NOW();
        	-> '2007-12-15 23:50:26'
	SELECT NOW() + 0;
        	-> 20071215235026.000000

NOW() returns a constant time that indicates the time at which the statement began to execute. (Within a stored function or trigger, NOW() returns the time at which the function or triggering statement began to execute.) This differs from the behavior for SYSDATE(), which returns the exact time at which it executes. ::

	SELECT NOW(), SLEEP(2), NOW();

Returns:

+---------------------+----------+---------------------+
| NOW()               | SLEEP(2) | NOW()               |
+---------------------+----------+---------------------+
| 2006-04-12 13:47:36 |        0 | 2006-04-12 13:47:36 |
+---------------------+----------+---------------------+

::

	SELECT SYSDATE(), SLEEP(2), SYSDATE();

Returns:

+---------------------+----------+---------------------+
| SYSDATE()           | SLEEP(2) | SYSDATE()           |
+---------------------+----------+---------------------+
| 2006-04-12 13:47:44 |        0 | 2006-04-12 13:47:46 |
+---------------------+----------+---------------------+

In addition, the SET TIMESTAMP statement affects the value returned by NOW() but not by SYSDATE(). This means that timestamp settings in the binary log have no effect on invocations of SYSDATE(). 

statement_timestamp()	           
----------------------

See :ref:`now`



