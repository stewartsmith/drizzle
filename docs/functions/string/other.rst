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

Replace occurrences of a specified string

REVERSE()
---------

Reverse the characters in a string

RIGHT()
-------

Return the specified rightmost number of characters

RPAD()
------

Append string the specified number of times

SOUNDEX()
---------

Return a soundex string


SPACE() 	          
-------

Return a string of the specified number of spaces


SUBSTRING()
-----------

Returns the substring as specified

Examples that use SUBSTRING() in the SELECT clause:

The SUBSTRING() function is used to extract a character string (using a given starting position and a given length). ::

	SELECT  
        SUBSTRING(course_designater,6,3) as 'Course number'                   
	FROM Courses
	WHERE course_designater LIKE 'Excel%' 
	LIMIT 10;    

You can also format a column using SUBSTRING() in combination with functions like LOWER() and UPPER(). ::

	SELECT 
	CONCAT(UPPER(SUBSTRING(lastname,1,1)),
  	LOWER(SUBSTRING(lastname,2,29)))
	FROM Students
	LIMIT 10;


