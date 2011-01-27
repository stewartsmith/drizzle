String Functions and Operators
==============================

Comparative functions:

==========     ===========================================
Operator         Description
==========     ===========================================
LIKE 	        Simple pattern matching
NOT LIKE 	Negation of simple pattern matching
STRCMP() 	Compare two strings
==========     ===========================================

Regular Expressions:

===================     =============================================================================================================
NOT REGEXP 	          Negation of REGEXP
REGEXP 	                  Pattern matching using regular expressions
RLIKE 	                  Synonym for REGEXP
===================     =============================================================================================================


Other string functions:

===================     =============================================================================================================
ASCII() 	          Return numeric value of left-most character
BIN() 	                  Return a string representation of the argument
BIT_LENGTH() 	          Return length of argument in bits
CHAR_LENGTH()             Return number of characters in argument
CHAR() 	                  Return the character for each integer passed
CHARACTER_LENGTH()        A synonym for CHAR_LENGTH()
CONCAT() 	          Return concatenated string
ELT() 	                  Return string at index number
EXPORT_SET() 	          Return a string such that for every bit set in the value bits, you get an on string and for every unset bit, you get an off string
FIELD() 	          Return the index (position) of the first argument in the sequent arguments
FIND_IN_SET() 	          Return the index position of the first argument within the second argument
FORMAT() 	          Return a number formatted to specified number of decimal places
HEX()  	                  Return a hexadecimal representation of a decimal or string value
INSERT() 	          Insert a substring at the specified position up to the specified number of characters
INSTR() 	          Return the index of the first occurrence of substring
LCASE() 	          Synonym for LOWER()
LEFT() 	                  Return the leftmost number of characters as specified
LENGTH() 	          Return the length of a string in bytes
LOAD_FILE() 	          Load the named file
LOCATE() 	          Return the position of the first occurrence of substring
LOWER() 	          Return the argument in lowercase
LPAD() 	                  Return the string argument, left-padded with the specified string
LTRIM() 	          Remove leading spaces
MAKE_SET() 	          Return a set of comma-separated strings that have the corresponding bit in bits set
MATCH 	                  Perform full-text search
MID() 	                  Return a substring starting from the specified position
OCTET_LENGTH() 	          A synonym for LENGTH()
ORD() 	                  Return character code for leftmost character of the argument
POSITION() 	          A synonym for LOCATE()
QUOTE() 	          Escape the argument for use in an SQL statement
REPEAT() 	          Repeat a string the specified number of times
REPLACE() 	          Replace occurrences of a specified string
REVERSE() 	          Reverse the characters in a string
RIGHT() 	          Return the specified rightmost number of characters
RPAD() 	                  Append string the specified number of times
RTRIM() 	          Remove trailing spaces
SOUNDEX() 	          Return a soundex string
SOUNDS LIKE 	          Compare sounds
SPACE() 	          Return a string of the specified number of spaces
SUBSTR() 	          Return the substring as specified
SUBSTRING_INDEX() 	  Return a substring from a string before the specified number of occurrences of the delimiter
SUBSTRING() 	          Return the substring as specified
TRIM() 	                  Remove leading and trailing spaces
UCASE() 	          Synonym for UPPER()
UNHEX() 	          Convert each pair of hexadecimal digits to a character
UPPER() 	          Convert to uppercase
===================     =============================================================================================================

