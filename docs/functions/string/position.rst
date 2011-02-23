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

INSTR(str, substr) returns the index of the first occurrence of substring str in string substr. Note that this works like LOCATE except the order of the arguments is reversed: ::

	SELECT INSTR('tacosalad', 'salad');
        	-> 4
	SELECT INSTR('burger', 'salad');
        	-> 0

LEFT
----

Return the leftmost number of characters as specified

INSERT
------

This function inserts a substring at the specified position up to the specified number of characters.

INSERT(str,pos,len,newstr)

It returns str (a string), with the substring beginning at pos (position) and len (how many characters long) replaced by the newstr. 

* INSERT returns the original string if pos is not within the length of the string
* It replaces the rest of the string from position pos if len is not within the length of the rest of the string
* It returns NULL if any argument is NULL

.. code-block:: mysql

	SELECT INSERT('Aquatic', 3, 2, 'Me');
       		-> 'AqMetic'
	SELECT INSERT('Aquatic', -1, 4, 'This');
        	-> 'Aquatic'
	SELECT INSERT('Aquatic', 3, 100, 'This');
        	-> 'AqThis'


LOCATE
------

Return the position of the first occurrence of substring.

POSITION
--------

A synonym for LOCATE()

