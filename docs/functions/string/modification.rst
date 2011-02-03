String Modification Functions
=============================

CONCAT
------

The SQL standard defines a concatenation operator ( || ), which joins two or more strings into one string value.
 The CONCAT(str1, str2….) function can have one or more arguments. It returns a string that is the result of concatenating the arguments. 

* If arguments are non-binary strings, the result is also a non-binary string.
* If any argument is a binary string, then the result will also be a binary string. 
* Numeric arguments are converted to their equivalent in binary string format. 
* If any argument is NULL then it also returns NULL. 

Syntax:

CONCAT(str1, str2, [,...n]) 
For example: ::       
	SELECT CONCAT('Dr', 'izzl', 'ed');

Returns: 'Drizzled' ::

	SELECT CONCAT('micro', NULL, 'nel');

Returns: NULL ::

	SELECT CONCAT(14.8);

Returns: '14.8'

CONCAT_WS
--------- 
CONCAT WS (With Separator) allows you to specify that the first argument is treated as a separator for the rest of the arguments. This argument is added between the strings to be concatenated. 

* If the separator is NULL then the result is NULL. 
Syntax:

CONCAT_WS(separator str1, str2,....) 

For example:       
	SELECT CONCAT_WS(',', ' Occupation', 'First name', 'Last Name');

Returns: 'Occupation, First name, Last Name'

	SELECT CONCAT_WS(',', 'First name', NULL, 'Last Name');

Returns: 'First name, Last Name'


TRIM()
------         

Remove leading and trailing spaces

LTRIM()
-------

Remove leading spaces


RTRIM()
-------

Remove trailing spaces

