Comparative Functions
=====================

LIKE 	        
----

Returns values that match a simple pattern

The LIKE operator is used to check if field values match a specified pattern, and searches for less-than-exact but similar values. The LIKE operator supports the use of two wildcards:

    * Percentage (%): Represents zero or more values.
    * Underscore (_): Represents exactly one value.

The following SELECT statement includes a WHERE clause expression that searches for film_title that start with "FARGO", by using the percentage wildcard after the lookup value.

For example: ::

	SELECT title, category_name
	FROM film_detail
	WHERE title LIKE 'FARGO%'
	ORDER BY category_name, title;


REGEXP
------

Returns values that match a regular expression pattern; they are commonly used for creating complex searches. Here is an example of using a REGEXP (Regular Expression) match: ::

	SELECT title, category_name
	FROM film_detail
	WHERE title REGEXP '^AIRP[LO]'
	ORDER BY title;

Other REGEXP examples: :;

	SELECT 'abcabc' REGEXP 'abc',    'abcabc' REGEXP 'cb';

The search pattern may describe only a part of string. To match entire string, use ^ and $ in the search: ::

	SELECT 'abc' REGEXP '^abc$', 'abcabc' REGEXP '^abc$';


SELECT 'cde' REGEXP '[a-c]+', 'efg' REGEXP '[a-c]+';


SELECT 'abcabc' REGEXP 'ABC', 'abcabc' REGEXP BINARY 'ABC';

