Conversion Functions
====================

ASCII
-----
 The ASCII(str) function returns the numeric value of the leftmost character of the string ’str’. It returns NULL if str is NULL. It works for 8-bit characters.

For example:  ::

	SELECT ASCII('0');

Returns 48 ::

	SELECT ASCII('d');

Returns 100

CHAR
----
 SQL CHAR function is the opposite of the ASCII function. It converts an integer in range 0-255 into a ASCII character. It returns a string (the character), given the integer value. This function skips NULL values.    
For example: ::

	SELECT CHAR(65) AS ch_65;

Returns "A"

CHAR_LENGTH
-----------
 The CHAR_LENGTH(str) function returns string length measured in characters. 

A multi-byte character counts as single character such as a string contains 5 two-byte characters, then LENGTH() function returns 10, but the CHAR_LENGTH() returns 5. ::        
	CHARACTER_LENGTH(str) 
This function is same as CHAR_LENGTH().  

BIN
---
 The BIN string function returns a string value that represents the binary value of N, where N is a longlong(BIGINT) number. This function is equivalent to CONV(N, 10 , 0). If the function return the null then N is null. 

Syntax:

BIN (N);

For exempt: ::

	SELECT BIN(12);

Returns: '1100'
