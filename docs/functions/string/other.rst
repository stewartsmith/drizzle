Other String Functions
======================

LOAD_FILE()
-----------

Load the named file

ELT()
-----

Return string at index number

EXPORT_SET()
------------

Return a string

FORMAT()
--------

Return a number formatted to specified number of decimal places

LOAD_FILE()
-----------

Load the named file

LPAD()
-------

Return the string argument, left-padded with the specified string

MAKE_SET()
----------

Return a set of comma-separated strings that have the corresponding bit in bits set

MATCH()
-------

Perform full-text search

MID()
-----

Return a substring starting from the specified position

ORD()
-----

Return character code for leftmost character of the argument

QUOTE()
-------

Escape the argument for use in an SQL statement

REPEAT()
--------

Repeat a string the specified number of times

REPLACE()
---------

The REPLACE() function returns a string with all occurrences of the 'from_str' replaced by 'to_str'. REPLACE is case-sensitive when searching for 'from_str'.

Syntax:

REPLACE(str,from_str,to_str)

For example:

.. code-block:: mysql
	
	SELECT REPLACE('wwww.google.com', 'w', 'v');

Returns: vvv.google.com

REVERSE()
---------

This function returns a string argument with the characters in reverse order.

.. code-block:: mysql

	SELECT REVERSE('abcd');

Returns: dcba

RIGHT()
-------

Return the specified rightmost number of characters

RPAD()
------

Append string the specified number of times

SOUNDEX()
---------

Return a soundex string


SUBSTRING()
-----------

Returns the substring as specified

Examples that use SUBSTRING() in the SELECT clause:

The SUBSTRING() function is used to extract a character string (using a given starting position and a given length).

.. code-block:: mysql

	SELECT  
        SUBSTRING(course_designater,6,3) as 'Course number'                   
	FROM Courses
	WHERE course_designater LIKE 'Excel%' 
	LIMIT 10;    

You can also format a column using SUBSTRING() in combination with functions like LOWER() and UPPER().

.. code-block:: mysql

	SELECT 
	CONCAT(UPPER(SUBSTRING(lastname,1,1)),
  	LOWER(SUBSTRING(lastname,2,29)))
	FROM Students
	LIMIT 10;


