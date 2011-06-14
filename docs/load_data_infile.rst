LOAD DATA INFILE
=================

While the INSERT statement loads one record at a time into a table, LOAD DATA INFILE imports data from an external text file into a table, and does so very rapidly. The file name must be given as a literal string.

For example:

.. code-block:: mysql

	LOAD DATA LOCAL INFILE '/home/user/names.txt' INTO TABLE names;

Then check that your data was loaded correctly:

.. code-block:: mysql

	SELECT * FROM names;

Options
--------

LOAD DATA INFILE has some options that can be used to specify the format for the text file and how the data is imported. Above, the LOCAL option specifies the client machine as the location of the text file. When connecting to a Drizzle server, the file will be read directly from the server as long as the LOCAL option is omitted.

The REPLACE option replaces table rows with the same primary key in the text file. For example:

.. code-block:: mysql

	LOAD DATA LOCAL INFILE '/home/user/names.txt' REPLACE INTO TABLE names;

The IGNORE option says to skip any rows that duplicate existing rows with the same primary key, and follows the same syntax as REPLACE. The IGNORE number LINES option can be used to ignore lines at the start of the file. For example, you can use IGNORE 1 LINES to skip over a row containing column names:

.. code-block:: mysql

	LOAD DATA LOCAL INFILE '/home/user/names.txt' INTO TABLE names IGNORE 1 LINES;

The FIELDS TERMINATED BY option can be used when importing from a comma separated value (CSV) file. (It specifies that the fields will be separated by a character other than a tab, such as a comma.) For example:

.. code-block:: mysql

	LOAD DATA LOCAL INFILE '/home/user/names.csv' REPLACE INTO TABLE names FIELDS TERMINATED BY ',';
