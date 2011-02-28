Comparative Functions
=====================

LIKE 	        
----

The LIKE operator is used to check if field values match a specified pattern, and searches for less-than-exact but similar values.

An initial example is:

.. code-block:: mysql

	SELECT 'ä' LIKE 'ae' COLLATE latin1_german2_ci;

Returns 0

Whereas:

.. code-block:: mysql

	SELECT 'ä' = 'ae' COLLATE latin1_german2_ci;

Returns 1

The LIKE operator supports the use of two wildcards. (Wildcards provide more flexibility by allowing any character or group of characters in a string to be acceptable as a match for another string):

    * Percentage (%): Represents zero or more values.
    * Underscore (_): Matches exactly one character value.

In accordance the SQL standard, LIKE performs matching on a per-character basis. It therefore provides results different from the = comparison operator.

The following SELECT statement includes a WHERE clause in order to search for job titles that start with "DIRECTOR", by using the percentage wildcard after the lookup value.

For example:

.. code-block:: mysql

	SELECT title, field
	FROM job_detail
	WHERE title LIKE 'DIRECTOR%'
	ORDER BY field, title;


REGEXP
------

Returns values that match a regular expression pattern; they are commonly used for creating complex searches. Here is an example of using a REGEXP (Regular Expression) match:

.. code-block:: mysql

	SELECT title, category_name
	FROM film_detail
	WHERE title REGEXP '^AIRP[LO]'
	ORDER BY title;

Other REGEXP examples:

.. code-block:: mysql

	SELECT 'abcabc' REGEXP 'abc',    
	'abcabc' REGEXP 'cb';

The search pattern may describe only a part of string. To match entire string, use ^ and $ in the search:

.. code-block:: mysql

	SELECT 'abc' REGEXP '^abc$', 'abcabc' REGEXP '^abc$';

	SELECT 'cde' REGEXP '[a-c]+', 'efg' REGEXP '[a-c]+';

	SELECT 'abcabc' REGEXP 'ABC', 'abcabc' REGEXP BINARY 'ABC';


STRCMP()
--------

The purpose of STRCMP is also to compare two strings. This function returns 0 if two strings are the same, -1 if the first argument is smaller than the second according to the current sort order, and 1 otherwise.
