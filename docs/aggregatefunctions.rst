Aggregate Functions
===================

SQL group (aggregate) functions operate on sets of values. If you use an aggregate function in a statement containing no GROUP BY clause, it is equivalent to grouping on all rows.

General syntax for aggregate functions is: ::

	SELECT "function type" ("column_name")
	FROM "table_name";

The following are examples of aggregate functions:

**AVG**:  Return the average value of the argument. (Does not work with temporal values unless first converted to numeric values.)

:doc:`count`
(DISTINCT):  Return the count of a number of different values

:doc:`count`:  Return a count of the number of rows returned
	
**GROUP_CONCAT**:  Return a concatenated string

**MAX**:  Return the maximum value

**MIN**:  Return the minimum value

**STD**:  Return the population standard deviation

**STDDEV_POP**:  Return the population standard deviation

**STDDEV_SAMP**:  Return the sample standard deviation

**STDDEV**:  Return the population standard deviation

**SUM**:  Return the sum. (Does not work with temporal values unless first converted to numeric values.)

**VAR_POP**:  Return the population standard variance

**VAR_SAMP**:  Return the sample variance

**VARIANCE**:  Return the population standard variance

.. toctree::
   :hidden: 

   count
