Drizzledump Backup Tool
=======================

Synopsis
--------

:program:`drizzledump` [:ref:`OPTIONS <drizzledump-options-label>`] *database* [*tables*]

:program:`drizzledump` [:ref:`OPTIONS <drizzledump-options-label>`] :option:`--databases <drizzledump --databases>` *DB1* [*DB2* *DB3*...]

:program:`drizzledump` [:ref:`OPTIONS <drizzledump-options-label>`] :option:`--all-databases <drizzledump --all-databases>`

Description
-----------

:program:`drizzledump` is used for backing up and
restoring logical backups of a Drizzle database, as well as for migrating
from a more traditional *MySQL* server. 

When connecting to a Drizzle server it will do a plain dump of the server.
When connecting to a MySQL server, it will automatically detect this, and
will convert the dump of the tables and data into a Drizzle compatible format.

Any binary data in tables will be converted into hexadecimal output so that it
does not corrupt the dump file.

.. _drizzledump-options-label:

Drizzledump options
-------------------

The :program:`drizzledump` tool has several available options:

.. program:: drizzledump 

.. option:: --all-databases, -A

   Dumps all databases found on the server apart from ``information_schema`` and
   ``data_dictionary`` in Drizzle and ``information_schema``,
   ``performance_schema`` and ``mysql`` in MySQL.

.. option:: --force, -f

   Continue even if a sql-error is received.

.. option:: --help, -?

   Show a message with all the available options.

.. option:: --lock-all-tables, -x

   Locks all the tables for all databases with a global read lock.  The lock is
   released automatically when :program:`drizzledump` ends.
   Turns on :option:`--single-transaction` and :option:`--lock-tables`.

.. option:: --single-transaction

   Creates a consistent snapshot by dumping the tables in a single transaction.
   During the snapshot no other connected client should use any of the
   following as this will implicitly commit the transaction and prevent the
   consistency:

   .. code-block:: mysql

	ALTER TABLE
	DROP TABLE
	RENAME TABLE
	TRUNCATE TABLE

   Only works with InnoDB.

.. option:: --skip-opt

   A shortcut for :option:`--skip-drop-table`, :option:`--skip-create`, 
   :option:`--skip-extended-insert` and :option:`--skip-disable-keys`

.. option:: --tables t1 t2 ...

   Dump a list of tables.

.. option:: --show-progress-size rows (=10000)

   Show progress of the dump every *rows* of the dump.  Requires
   :option:`--verbose`

.. option:: --verbose, -v

   Sends various verbose information to stderr as the dump progresses.

.. option:: --skip-extended-insert

   Dump every row on an individual line.  For example:

.. code-block:: mysql

	INSERT INTO `t1` VALUES (1,'hello');
	INSERT INTO `t1` VALUES (2,'world');

   This is useful for calculating and storing diffs of dump files.

.. option:: --skip-dump-date

   Do not display the date/time at the end of the dump.

.. option:: --no-defaults

   Do not attempt to read configuration from configuration files.

.. option:: --add-drop-database

   Add ``DROP DATABASE`` statements before ``CREATE DATABASE``.

.. option:: --compact

   Gives a more compact output by disabling header/footer comments and enabling
   :option:`--skip-add-drop-table`, :option:`--skip-disable-keys` 
   and :option:`--skip-add-locks`.

.. option:: --databases, -B

   Dump several databases.  The databases do not need to follow on after this
   option, they can be anywhere in the command line.

.. option:: --skip-disable-keys, -K

   Do not dump the statements ``ALTER TABLE ... DISABLE KEYS`` and
   ``ALTER TABLE ... ENABLE KEYS``

.. option:: --ignore-table table

   Do not dump specified table, needs to be in the format ``database.table``.
   Can be specified multiple times for multiple tables.

.. option:: --insert-ignore

   Add the ``IGNORE`` keyword into every ``INSERT`` statement.

.. option:: --no-autocommit

   Make the dump of each table a single transaction by wrapping it in ``COMMIT``
   statements.

.. option:: --no-create-db, -n

   Do not dump the ``CREATE DATABASE`` statements when using
   :option:`--all-databases` or :option:`--databases`.

.. option:: --skip-create, -t

   Do not dump the ``CREATE TABLE`` statements.

.. option:: --no-data, -d

   Do not dump the data itself. Used to dump the schemas only.

.. option:: --replace

   Use ``REPLACE INTO`` statements instead of ``INSERT INTO``

.. option:: --destination-type type (=stdout)

   Destination of the data.

   **stdout**
   The default.  Output to the command line

   **database**
   Connect to another database and pipe data to that.

   .. versionadded:: Drizzle7 2010-09-27

.. option:: --destination-host hostname (=localhost)

   The hostname for the destination database.  Requires
   :option:`--destination-type` `= database`

   .. versionadded:: Drizzle7 2010-09-27

.. option:: --destination-port port (=3306)

   The port number for the destination database.  Requires
   :option:`--destination-type` `= database`

   .. versionadded:: Drizzle7 2010-09-27

.. option:: --destination-user username

   The username for the destinations database.  Requires
   :option:`--destination-type` `= database`

   .. versionadded:: Drizzle7 2010-09-27

.. option:: --destination-password password

   The password for the destination database.  Requires
   :option:`--destination-type` `= database`

   .. versionadded:: Drizzle7 2010-09-27

.. option:: --destination-database database

   The database for the destination database, for use when only dumping a
   single database.  Requires
   :option:`--destination-type` `= database`

   .. versionadded:: Drizzle7 2010-09-27

.. option:: --my-data-is-mangled

   If your data is UTF8 but has been stored in a latin1 table using a latin1
   connection then corruption is likely and drizzledump by default will retrieve
   mangled data.  This is because MySQL will convert the data to UTF8 on the way
   out to drizzledump and you effectively get a double-conversion to UTF8.

   This typically happens with PHP apps that do not use ``SET NAMES``.

   In these cases setting this option will retrieve the data as you see it in
   your application.

   .. versionadded:: Drizzle7 2011-01-31

.. option:: --host, -h hostname (=localhost)

   The hostname of the database server.

.. option:: --user, -u username

   The username for the database server.

.. option:: --password, -P password

   The password for the database server.

.. option:: --port, -p port (=4427)

   The port number of the database server.

.. option:: --protocol protocol (=mysql)

   The protocol to use when connecting to the database server.  Options are:

   **mysql**
   The standard MySQL protocol.

   **drizzle**
   The Drizzle protocol.

Backups using Drizzledump
-------------------------

Backups of a database can be made very simply by running the following:

.. code-block:: bash

  $ drizzledump --all-databases > dumpfile.sql

This can then be re-imported into drizzle at a later date using:

.. code-block:: bash

  $ drizzle < dumpfile.sql

.. _drizzledump-migration-label:

MySQL Migration using Drizzledump
---------------------------------

As of version 2010-09-27 there is the capability to migrate databases from
MySQL to Drizzle using :program:`drizzledump`.

:program:`drizzledump` will automatically detect whether it is talking to a
MySQL or Drizzle database server.  If it is connected to a MySQL server it will
automatically convert all the structures and data into a Drizzle compatible 
format.

.. warning::

   :program:`drizzledump` will by default try to connect via. port 4427 so to
   connect to a MySQL server a port (such as 3306) must be specified.

So, simply connecting to a MySQL server with :program:`drizzledump` as follows
will give you a Drizzle compatible output:

.. code-block:: bash

  $ drizzledump --all-databases --host=mysql-host --port=3306 --user=mysql-user --password > dumpfile.sql

Additionally :program:`drizzledump` can now dump from MySQL and import directly
into a Drizzle server as follows:

.. code-block:: bash

  $ drizzledump --all-databases --host=mysql-host --port=3306 --user=mysql-user --password --destination-type=database --desination-host=drizzle-host

.. note::

   Please take special note of :ref:`old-passwords-label` if you have connection
   issues from :program:`drizzledump` to your MySQL server.

.. note::
   If you find your VARCHAR and TEXT data does not look correct in a drizzledump
   output, it is likely that you have UTF8 data stored in a non-UTF8 table.  In
   which case please check the :option:`--my-data-is-mangled` option.

When you migrate from MySQL to Drizzle, the following conversions are required:

 * MyISAM -> InnoDB
 * FullText -> drop it (with stderr warning)
 * int unsigned -> bigint
 * tinyint -> int
 * smallint -> int
 * mediumint -> int
 * tinytext -> text
 * mediumtext -> text
 * longtext -> text
 * tinyblob -> blob
 * mediumblob -> blob
 * longblob -> blob
 * year -> int
 * set -> text [1]_
 * date/datetime default 0000-00-00 -> default NULL [2]_
 * date/datetime NOT NULL columns -> NULL [2]_
 * any date data containing 0000-00-00 -> NULL [2]_
 * time -> int of the number of seconds [3]_
 * enum-> DEFAULT NULL [4]_

.. rubric:: Footnotes

.. [1] There is currently no good alternative to SET, this is simply to preserve
       the data in the column.  There is a new alternative to SET to be included
       at a later date.

.. [2] Currently, ALL date columns have their DEFAULT set to NULL on migration.
       This is so that any rows with 0000-00-00 dates can convert to NULL.

.. [3] This prevents data loss since MySQL's TIME data type has a range of
       -838:59:59 - 838:59:59, and Drizzle's TIME type has a range of
       00:00:00 - 23:59:59.

.. [4] This is so that empty entries such as '' will convert to NULL.
