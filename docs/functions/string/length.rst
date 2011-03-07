Length and Size Functions
=========================

BIT_LENGTH
----------
The BIT_LENGTH(str) function return the String str length in bits. Here are the some example of the BIT_LENGTH(str) function: 
For example:

.. code-block:: mysql

	SELECT BIT_LENGTH('a');

Returns 8

CHAR_LENGTH
-----------
The CHAR_LENGTH(str) function returns string length measured in characters. 

A multi-byte character counts as single character such as a string contains 5 two-byte characters, then LENGTH() function returns 10, but the CHAR_LENGTH() returns 5. ::

	CHARACTER_LENGTH(str)

This function is same as CHAR_LENGTH().


LENGTH()
--------

The LENGTH function returns the length of the string argument in bytes. A multi-byte character counts as multiple bytes. This means that for a string containing a three-byte character, LENGTH() returns 3, whereas CHAR_LENGTH() returns 1. For example:

.. code-block:: mysql

	select length(_utf8 '€');

Returns 3

The is because the Euro sign is encoded as 0xE282AC in UTF-8 and thereby occupies 3 bytes.

OCTET_LENGTH()
---------------

A synonym for LENGTH()
