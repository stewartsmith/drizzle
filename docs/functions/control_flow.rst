Control Flow Functions
======================

There are four control flow functions: 

* CASE
* IF/ELSE
* IFNULL
* NULLIF

Control flow functions return a value for each row processed, which represents the result of the comparison or condition specified. They can be used in SELECT, WHERE, ORDER BY, GROUP BY (will be covered in aggregate functions section) clause.


There are two basic examples of the CASE statment:

1. CASE value WHEN [compare_value] THEN result [WHEN [compare_value] THEN result ...] [ELSE result] END

In this version, result is returned when value is equal to compare_value. If nothing is matched, the result after ELSE is returned, or NULL is returned if there is no ELSE part.

Practice #1-1: Get each supplier's continent from Suppliers table based on the supplier's country.

Practice #1-2: Get each supplier's continent from Suppliers table and use the CASE statement's column alias in ORDER BY clause.

Version 2:

CASE WHEN [condition] THEN result [WHEN [condition] THEN result ...] [ELSE result] END

In this version, if condition is true, result is returned. If nothing is matched, the result after ELSE is returned, or NULL is returned if there is no ELSE part.

Version 2 returns the same result as Version 1 when condition is for equal comparison (=). See Practice #1-3 which returns the same result as Practice #1-1.

condition can use any of the comparison operators covered in Using Comparison Operators, Part I and Part II. See Practice #1-4 where IN and = operator are used.