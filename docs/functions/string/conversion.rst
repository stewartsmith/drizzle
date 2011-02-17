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


BIN
---
 The BIN string function returns a string value that represents the binary value of N, where N is a longlong(BIGINT) number. This function is equivalent to CONV(N, 10 , 0). If the function return the null then N is null. 

Syntax:

BIN (N);

For exempt: ::

	SELECT BIN(12);

Returns: '1100'


CHAR
----
 SQL CHAR function is the opposite of the ASCII function. It converts an integer in range 0-255 into a ASCII character. It returns a string (the character), given the integer value. This function skips NULL values.    
For example: ::

	SELECT CHAR(65) AS ch_65;

Returns "A" 

HEX()
-----

This string function returns the hexadecimal (base-16) representation of a string or decicmal argument. Each character in the string argument is converted to two hexadecimal digits. If the argument is numeric, HEX() returns a hexadecimal string representation of the value as a BIGINT number.

Using HEX for numeric values: ::

	SELECT HEX(255);

Returns: FF

Using HEX for string values: ::
 
	SELECT HEX('Drizzle');

Returns: 4452495A5AHc45

(To better understand this output, you can use an :doc:`../../resources/ascii_chart` that includes both Hexadecimal and character values.)


UNHEX()
-------

UNHEX converts each pair of hexadecimal digits to a character. For a string argument, UNHEX() is the inverse operation of HEX(str).

Instead of converting each character in the string argument to hex digits, it interprets each pair of characters in the argument as a hexadecimal number and converts it to the character represented by the number. The return value is a binary string. ::

	SELECT UNHEX('4452495A5AHc45');

Returns 'drizzle' ::

	SELECT UNHEX(HEX('string'));

Returns 'string' ::

	SELECT HEX(UNHEX('1267'));

Returns '1267'

The characters in the argument string must be legal hexadecimal digits: '0' .. '9', 'A' .. 'F', 'a' .. 'f'. If the argument contains any non-hexadecimal digits, the result is NULL: ::

	SELECT UNHEX('GG');

Returns NULL



LOWER() 	          
Return the argument in lowercase


LCASE() 	          
Synonym for LOWER()


UCASE() 	          
Synonym for UPPER()


UPPER() 	          
Convert to uppercase
