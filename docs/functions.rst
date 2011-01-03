SQL Functions
=============

Several SQL functions are built into Drizzle--functions perform calculations on data. They can be divided into two general types: Aggregate and Scalar.

**SQL Aggregate Functions**

*SQL aggregate functions are calculated from values in a column, and return a single value.*

General syntax for aggregate functions is: ::

	SELECT "function type" ("column_name")
	FROM "table_name"

Click on the following aggregate functions to learn more:

    * AVG() - Returns the average value
    * COUNT() - Returns the number of rows
    * FIRST() - Returns the first value
    * LAST() - Returns the last value
    * MAX() - Returns the largest value
    * MIN() - Returns the smallest value
    * SUM() - Returns the sum

**SQL Scalar functions**

*SQL scalar functions are based on the input value, and return a single value.*

General syntax for scalar functions is: ::

	SELECT "function type" ("column_name")
	FROM "table_name"

Click on the following scalar functions to learn more:

    * UCASE() - Converts a field to upper case
    * LCASE() - Converts a field to lower case
    * MID() - Extract characters from a text field
    * LEN() - Returns the length of a text field
    * ROUND() - Rounds a numeric field to the number of decimals specified
    * NOW() - Returns the current system date and time
    * FORMAT() - Formats how a field is to be displayed


