Control Flow Functions
======================

There are four control flow functions: 

* CASE
* IF/ELSE
* IFNULL
* NULLIF

Control flow functions return a value for each row processed, which represents the result of the comparison or condition specified. They can be used in ``SELECT``, ``WHERE``, ``ORDER BY``, and ``GROUP BY`` statements.

CASE
----

There are two basic examples of the ``CASE`` statment:

 1. ::

	CASE value WHEN [compare_value] THEN result [WHEN [compare_value] THEN result ...] [ELSE result] END

In this version, result is returned when value is equal to compare_value. If nothing is matched, the result after ``ELSE`` is returned, or ``NULL`` is returned if there is no ``ELSE`` part.

 2. ::

	CASE WHEN [condition] THEN result [WHEN [condition] THEN result ...] [ELSE result] END

In this version, if [condition] is true, result is returned. If nothing is matched, the result after ``ELSE`` is returned, or ``NULL`` is returned if there is no ``ELSE`` part.

When [condition] is for equal comparison (=), this example syntax returns the same result as the first example.

IF/ELSE
---------

This type of control flow function checks IF a condition is satisfied (i.e. the Boolean expression returns TRUE), the IF SQL statement will execute. IF the condition is not satisfied (i.e. the Boolean expression returns FALSE), the ELSE SQL statement is executed.

Syntax for IFÉELSE: ::

	IF ( Boolean_expression ) 
	BEGIN
	SQL statement
	END
	ELSE
	BEGIN
	SQL statement
	END


IFNULL
-------

Given IFNULL(arg1,arg2), if arg1 is not NULL, IFNULL() returns arg1; it otherwise returns arg2. IFNULL() returns a numeric or string value, depending on how it's used. ::

.. code-block:: mysql

	SELECT IFNULL(2,0);
        -> 2
	
	SELECT IFNULL(NULL,1);
        -> 1
	
	SELECT IFNULL(1/0,10);
        -> 10
	
	SELECT IFNULL(1/0,'yes');
        -> 'yes'

NULLIF
-------

NULLIF(arg1,arg2) returns NULL if arg1 = arg2 is true, otherwise returns arg1. ::

.. code-block:: mysql

	SELECT NULLIF(1,1);
	-> NULL

	SELECT NULLIF(1,2);
        -> 1
