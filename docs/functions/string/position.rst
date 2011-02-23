Position Functions
==================

FIELD
-----

The FIELD function returns the index (position) of string arguments (str1, str2, str3, ….) 

It returns 0 if the str value is not found.

If each argument is a string, all arguments will be compared as strings, whereas if arguments are numbers, they will be compared as numbers.

Otherwise, the arguments are compared as double.

If str is NULL, the return value is 0 because NULL fails equality comparison with any value. FIELD() is the complement of ELT(). ::

	SELECT FIELD('ej', 'Hej', 'ej', 'Heja', 'hej', 'foo');

Returns 2 ::
	
	SELECT FIELD('fo', 'Hej', 'ej', 'Heja', 'hej', 'foo');

Returns 0

FIND_IN_SET
-----------

Return the index position of the first argument within the second argument

INSTR
-----

Return the index of the first occurrence of substring

LEFT
----

Return the leftmost number of characters as specified

INSERT
------

Insert a substring at the specified position up to the specified number of characters

LOCATE
------

Return the position of the first occurrence of substring

POSITION
--------

A synonym for LOCATE()

