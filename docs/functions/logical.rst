Logical and Comparison Operators
================================

==============================     ================================
Operators                           Description
==============================     ================================
:ref:`and` &&                       Logical AND
:ref:`or` (||)                      Logical OR
:ref:`xor` 	                    Logical XOR
:ref:`not` (!)                      Logical NOT
:ref:`less_than` <      	    less than
:ref:`greater_than` >   	    greater than
:ref:`less_or_equal` <=             less than or equal to
:ref:`greater_or_equal` >=          greater than or equal to
:ref:`equal` =  	            equal
:ref:`not_equal` <> or !=           not equal
==============================     ================================


.. _and:

AND
---

This logical operator AND:

* evaluates to 1 if all operands are nonzero and not NULL
* evaluates to 0 if one or more operands are 0 
* otherwise returns NULL 

::

	SELECT 1 && 1;
	        -> 1
	SELECT 1 && 0;
        	-> 0
	SELECT 1 && NULL;
        	-> NULL
	SELECT 0 && NULL;
        	-> 0
	SELECT NULL && 0;
        	-> 0

AND can be used to select rows that satisfy all the conditions given in a statement. For Example, to find the names of the students between the age 20 to 25 years, the query would be like: ::

	SELECT first_name, last_name, age
	FROM user_details
	WHERE age >= 20 AND age <= 25;

The output would be:

+---------------+------------------+-------+
|first_name 	|last_name 	   |age    |
+===============+==================+=======+
|Mary 	        |Bean   	   |20     |
+---------------+------------------+-------+
|Henry  	|Catson 	   |22     |
+---------------+------------------+-------+
|Sheila 	|Donaldson         |25     |
+---------------+------------------+-------+  

The logical "AND" operator selects rows only if the data in all relevant columns is satisfied. In this case, 'first_name' and 'last_name' simply need to have non-NULL values, and 'age' needs to be a value between 20 and 25.

.. _or:

OR
--

This logical operator OR:

* returns 1 if any operand is nonzero and both operands are non-NULL, and returns 0 otherwise
* returns 1 with a NULL operand if the other operand is nonzero, and retunes NULL otherwise
* returns NULL if both operands are NULL

::

	SELECT 1 || 1;
        	-> 1
	SELECT 1 || 0;
        	-> 1
	SELECT 0 || 0;
        	-> 0
	SELECT 1 || NULL;
        	-> 1
	SELECT 0 || NULL;
        	-> NULL

In other words, OR is used to select rows that satisfy at least one of the given conditions.

For example, the following query could be used to find the user_id for people that live in either California or Texas: ::

	SELECT user_id, state
	FROM user_location
	WHERE state = 'California' OR state = 'Texas';

The result set could be something like:

+---------------+------------------+-----------+
|user_id 	|city   	   |state      |
+===============+==================+===========+
|608            |Sacremento   	   |California |
+---------------+------------------+-----------+
|844     	|Austin 	   |Texas      |
+---------------+------------------+-----------+
|917    	|Oakland           |California |
+---------------+------------------+-----------+  


.. _xor:

XOR
---

* returns NULL if either operand is NULL
* evaluates to 1 for non-NULL operands (if an odd number of operands is nonzero)
* otherwise 0 is returned

	SELECT 1 XOR NULL;
        	-> NULL
	SELECT 1 XOR 0;
        	-> 1
	SELECT 1 XOR 1 XOR 1;
        	-> 1
	SELECT 1 XOR 1;
        	-> 0

Note that "a XOR b" is mathematically equal to "(a AND (NOT b)) OR ((NOT a) and b)". 


.. _not:

NOT
---

This logical operator NOT:

* evaluates to 1 if the operand is 0
* evaluates to 0 if the operand is nonzero
* NOT NULL returns NULL

::

	SELECT NOT 10;
        	-> 0
	SELECT NOT 0;
        	-> 1
	SELECT NOT NULL;
        	-> NULL
	SELECT ! (1+1);
        	-> 0
	SELECT ! 1+1;
        	-> 1

If you want to find rows that do not satisfy a condition, you can use the logical operator, NOT. NOT results in the reverse of a condition. That is, if a condition is satisfied, then the row is not returned.

For example: If you want to find out the user_id for people who do not practice medicine as their profession, the query would be like: ::

	SELECT user_id, title, occupation
	FROM user_occupations
	WHERE NOT occupation = 'Doctor';

The result set would be something like:

+---------------+------------------+--------------+
|user_id 	|degree   	   |occupation    |
+===============+==================+==============+
|322            |PhD    	   |Professor     |
+---------------+------------------+--------------+
|579     	|PhD    	   |Writer        |
+---------------+------------------+--------------+
|681     	|MD                |Consultant    |
+---------------+------------------+--------------+  

