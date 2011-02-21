Control Flow Functions
======================

There are four control flow functions: 

* CASE
* IF/ELSE
* IFNULL
* NULLIF

Control flow functions return a value for each row processed, which represents the result of the comparison or condition specified. They can be used in SELECT, WHERE, ORDER BY, and GROUP BY statements.

There are two basic examples of the CASE statment:

1. ::

	CASE value WHEN [compare_value] THEN result [WHEN [compare_value] THEN result ...] [ELSE result] END

In this version, result is returned when value is equal to compare_value. If nothing is matched, the result after ELSE is returned, or NULL is returned if there is no ELSE part.

2. ::

	CASE WHEN [condition] THEN result [WHEN [condition] THEN result ...] [ELSE result] END

In this version, if [condition] is true, result is returned. If nothing is matched, the result after ELSE is returned, or NULL is returned if there is no ELSE part.

When [condition] is for equal comparison (=), this example syntax returns the same result as the first example.