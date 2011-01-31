String Functions and Operators
===============================

.. toctree::
   :maxdepth: 2

   string/comparative
   string/conversion
   string/length 
   string/modification
   string/position
   string/other


List of string operators and descriptions by type of function:


Comparative and regular functions
---------------------------------

===================     ====================================================================================================
Operator                  Description
===================     ==================================================================================================== 
LIKE 	                  Returns values that match a simple pattern
NOT LIKE 	          Negation of simple pattern matching
STRCMP() 	          Compare two strings
NOT REGEXP 	          Negation of REGEXP
REGEXP 	                  Returns values that match a regular expression pattern
RLIKE 	                  Synonym for REGEXP
===================     ====================================================================================================


Conversion and translation functions
------------------------------------
===================     ====================================================================================================
Operator                  Description
===================     ====================================================================================================
ASCII() 	          Return numeric value of left-most character
BIN() 	                  Return a string representation of the argument
CHAR() 	                  Return the character for each integer passed
HEX()  	                  Return a hexadecimal representation of a decimal or string value
LOWER() 	          Return the argument in lowercase
LCASE() 	          Synonym for LOWER()
UCASE() 	          Synonym for UPPER()
UNHEX() 	          Convert each pair of hexadecimal digits to a character
UPPER() 	          Convert to uppercase
===================     ====================================================================================================


Length and size functions
-------------------------
===================     ====================================================================================================
Operator                  Description
===================     ====================================================================================================
BIT_LENGTH() 	          Return length of argument in bits
CHAR_LENGTH()             Return number of characters in argument
LENGTH() 	          Return the length of a string in bytes
OCTET_LENGTH() 	          A synonym for LENGTH()
===================     ====================================================================================================


String modificaion functions
----------------------------
===================     ====================================================================================================
Operator                  Description
===================     ====================================================================================================
CONCAT() 	          Returns a concatenated string
TRIM() 	                  Remove leading and trailing spaces
LTRIM() 	          Remove leading spaces
RTRIM() 	          Remove trailing spaces
===================     ====================================================================================================


Position functions
-------------------

===================     ====================================================================================================
Operator                  Description
===================     ==================================================================================================== 
FIELD() 	          Return the index (position) of the first argument in the sequent arguments
FIND_IN_SET() 	          Return the index position of the first argument within the second argument
INSTR() 	          Return the index of the first occurrence of substring
LEFT() 	                  Return the leftmost number of characters as specified
INSERT() 	          Insert a substring at the specified position up to the specified number of characters
LOCATE() 	          Return the position of the first occurrence of substring
POSITION() 	          A synonym for LOCATE()
===================     ==================================================================================================== 


Other string functions
----------------------

===================     ====================================================================================================
Operator                  Description
===================     ==================================================================================================== 
ELT() 	                  Return string at index number
EXPORT_SET() 	          Return a string
FORMAT() 	          Return a number formatted to specified number of decimal places
LOAD_FILE() 	          Load the named file
LPAD() 	                  Return the string argument, left-padded with the specified string
MAKE_SET() 	          Return a set of comma-separated strings that have the corresponding bit in bits set
MATCH 	                  Perform full-text search
MID() 	                  Return a substring starting from the specified position
ORD() 	                  Return character code for leftmost character of the argument
QUOTE() 	          Escape the argument for use in an SQL statement
REPEAT() 	          Repeat a string the specified number of times
REPLACE() 	          Replace occurrences of a specified string
REVERSE() 	          Reverse the characters in a string
RIGHT() 	          Return the specified rightmost number of characters
RPAD() 	                  Append string the specified number of times
SOUNDEX() 	          Return a soundex string
SOUNDS LIKE 	          Compare sounds
SPACE() 	          Return a string of the specified number of spaces
SUBSTR() 	          Return the substring as specified
SUBSTRING_INDEX() 	  Return a substring from a string before the specified number of occurrences of the delimiter
SUBSTRING() 	          Return the substring as specified
===================     ====================================================================================================

