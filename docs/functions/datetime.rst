Date and Time Functions
=======================

.. toctree::
   :hidden: 
   
   current_time_functions
   extract_date_functions
   date_trunc

Current
-------

For examples of the following, see :doc:`current_time_functions`.

+-----------------------------------+-------------------------------------+-----------------------------------------------------------------+
|Name                               |Return Type                          |Description                                                      |
+===================================+=====================================+=================================================================+
|current_date                       |date                                 |Current date                                                     |
+-----------------------------------+-------------------------------------+-----------------------------------------------------------------+
|current_time                       |time with time zone                  |Current time of day	                                            |
+-----------------------------------+-------------------------------------+-----------------------------------------------------------------+
|current_timestamp                  |timestamp with time zone	          |Current date and time (start of current transaction)             |
+-----------------------------------+-------------------------------------+-----------------------------------------------------------------+
|localtime	                    |time	                          |Current time of day	 	                                    |
+-----------------------------------+-------------------------------------+-----------------------------------------------------------------+
|localtimestamp	                    |timestamp	                          |Current date and time (start of current transaction)             |
+-----------------------------------+-------------------------------------+-----------------------------------------------------------------+
|now()	                            |timestamp with time zone	          |Current date and time (start of current transaction)             |
+-----------------------------------+-------------------------------------+-----------------------------------------------------------------+
|statement_timestamp()	            |timestamp with time zone	          |Current date and time (start of current statement)               |
+-----------------------------------+-------------------------------------+-----------------------------------------------------------------+


Extract
-------

For examples of the following, see :doc:`extract_date_functions`.

+--------------------------------+-----------------------+---------------------------------------+
|Name                            |Return Type            |Description                            |
+================================+=======================+=======================================+
|extract(field from timestamp)	 |double precision       |Get subfield 	                         |
+--------------------------------+-----------------------+---------------------------------------+
|extract(field from interval)	 |double precision       |Get subfield	                         |
+--------------------------------+-----------------------+---------------------------------------+
|date_part(text, timestamp)	 |double precision       |Get subfield (equivalent to extract)   |
+--------------------------------+-----------------------+---------------------------------------+
|date_part(text, interval)	 |double precision	 |Get subfield (equivalent to extract)   |
+--------------------------------+-----------------------+---------------------------------------+


DATE TRUNC
-----------

For an example of the following, see :doc:`date_trunc`.

+--------------------------------+-----------------------+---------------------------------------+
|Name                            |Return Type            |Description                            |
+================================+=======================+=======================================+
|date_trunc(text, timestamp)     |timestamp	         |Truncate to specified precision	 |
+--------------------------------+-----------------------+---------------------------------------+





