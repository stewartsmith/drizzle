CURRENT TIME FUNCTIONS
=======================


current_date
-------------

Returns the current date as a value in 'YYYY-MM-DD' or YYYYMMDD format, depending on whether the function is used in a string or numeric context. ::

	SELECT CURDATE();
        	-> '2011-02-13'
	SELECT CURDATE() + 0;
        	-> 20110213

current_time
--------------

Returns the current time as a value in 'HH:MM:SS' or HHMMSS.uuuuuu format, depending on whether the function is used in a string or numeric context. The value is expressed in the current time zone. ::

	SELECT CURTIME();
        	-> '10:30:09'
	SELECT CURTIME() + 0;
        	-> 103009.000000


current_timestamp
------------------

See :ref:`now`

CURRENT_TIMESTAMP() is a synonym for NOW(). 

localtime
-----------

See :ref:`now`

LOCALTIME() is a synonym for NOW(). 

localtimestamp	                   
---------------

See :ref:`now`

LOCALTIMESTAMP() is a synonym for NOW(). 

.. _now:

now()	                            
------

NOW returns the current date and time. The return value will be expressed as 'YYYY-MM-DD HH:MM:SS' or YYYYMMDDHHMMSS.uuuuuu, depending on whether the function is used in a string or numeric context. The value is expressed in the current time zone. ::

	SELECT NOW();
        	-> '2011-02-15 13:40:06'
	SELECT NOW() + 0;
        	-> 20110215134006.000000

NOW returns a constant time that indicates the time at which the statement began to execute. 

.. code-block:: mysql

	SELECT NOW(), SLEEP(2), NOW();

Returns:

+---------------------+----------+---------------------+
| NOW()               | SLEEP(2) | NOW()               |
+---------------------+----------+---------------------+
| 2011-02-20 20:15:09 |        0 | 2011-02-20 20:15:09 |
+---------------------+----------+---------------------+

SYSDATE, however, returns the exact time at which the function was invoked.

.. code-block:: mysql

	SELECT SYSDATE(), SLEEP(2), SYSDATE();

Returns:

+---------------------+----------+---------------------+
| SYSDATE()           | SLEEP(2) | SYSDATE()           |
+---------------------+----------+---------------------+
| 2011-02-20 20:15:09 |        0 | 2011-02-20 20:15:11 |
+---------------------+----------+---------------------+

When using replication, the binary log will include SET TIMESTAMP entries so that a database can be restored from the binary log. In doing this, values from NOW will be adjusted to the same times as when the original SQL statements were executed. SYSDATE entries will be unaffected by SET TIMESTAMP entries.


statement_timestamp()	           
----------------------

See :ref:`now`

STATEMENT_TIMESTAMP() is a synonym for NOW(). 




