Drizzledump Backup Tool
=======================

Synopsis
--------

**drizzledump** [*OPTIONS*] *database* [*tables*]

**drizzledump** [*OPTIONS*] *--databases* [*OPTIONS*] *DB1* [*DB2* *DB3*...]

**drizzledump** [*OPTIONS*] *--all-databases* [*OPTIONS*]

Description
-----------

:program:`drizzledump` is used for backing up and
restoring logical backups of a Drizzle database, as well as for migrating
from *MySQL*. 

When connecting to a Drizzle server it will do a plain dump of the server.  It
will, however, automatically detect when it is connected to a *MySQL* server and
will convert the tables and data into a Drizzle compatible format.

Any binary data in tables will be converted into hexadecimal output so that it
does not corrupt the dump file.

Drizzledump options
-------------------

The :program:`drizzledump` tool has several available options:

.. option:: -A, --all-databases

Dumps all databases found on the server apart from *information_schema* and
*data_dictionary* in Drizzle and *information_schema*, *performance_schema*
and *mysql* in MySQL.

.. option:: -f, --force

Continue even if we get an sql-error.

.. option:: -?, --help

Show a message with all the available options.

.. option:: -x, --lock-all-tables

Locks all the tables for all databases with a global read lock.  The lock is
released automatically when :program:`drizzledump` ends.
Turns on :option:`--single-transaction` and :option:`--lock-tables`.

.. option:: --single-transaction

Creates a consistent snapshot by dumping the tables in a single transaction.
During the snapshot no other connected client should use any of the
following as this will implicitly commit the transaction and prevent the
consistency::

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

.. option:: -v, --verbose

Sends various verbose information to stderr as the dump progresses.

.. option:: --skip-create

Do not dump the CREATE TABLE / CREATE DATABASE statements.

.. option:: --skip-extended-insert

Dump every row on an individual line.  For example::

	INSERT INTO `t1` VALUES (1,'hello');
	INSERT INTO `t1` VALUES (2,'world');

.. option:: --skip-dump-date

Do not display the date/time at the end of the dump.

.. option:: --no-defaults

Do not attempt to read configuration from configuration files.

.. option:: --add-drop-database

Add `DROP DATABASE` statements before `CREATE DATABASE`.

.. option:: --compact

Gives a more compact output by disabling header/footer comments and enabling
:option:`--skip-add-drop-table`, :option:`--skip-disable-keys` 
and :option:`--skip-add-locks`.

.. option:: -B, --databases

Dump several databases.  The databases do not need to follow on after this
option, they can be anywhere in the command line.

.. option:: -K, --skip-disable-keys

Do not dump the statements `ALTER TABLE ... DISABLE KEYS` and
`ALTER TABLE ... ENABLE KEYS`

.. option:: --ignore-table table

Do not dump specified table, needs to be in the format `database.table`.
Can be specified multiple times for multiple tables.

.. option:: --insert-ignore

Add the `IGNORE` keyword into every `INSERT` statement.

.. option:: --no-autocommit

Make the dump of each table a single transaction by wrapping it in `COMMIT`
statements.

.. option:: -n, --no-create-db

Do not dump the `CREATE DATABASE` statements when using
:option:`--all-databases` or :option:`--databases`.

.. option:: -t, --skip-create

Do not dump the `CREATE TABLE` statements.

.. option:: -d, --no-data

Do not dump the data itself, used to dump the schemas only.

.. option:: --replace

Use `REPLACE INTO` statements instead of `INSERT INTO`

.. option:: --destination-type type (=stdout)

Destination of the data.

**stdout**
The default.  Output to the command line

**database**
Connect to another database and pipe data to that.

.. versionadded:: 2010-09-27

.. option:: --destination-host hostname (=localhost)

The hostname for the destination database.  Requires
:option:`--destination-type` `= database`

.. versionadded:: 2010-09-27

.. option:: --destination-port port (=3306)

The port number for the destination database.  Requires
:option:`--destination-type` `= database`

.. versionadded:: 2010-09-27

.. option:: --destination-user username

The username for the destinations database.  Requires
:option:`--destination-type` `= database`

.. versionadded:: 2010-09-27

.. option:: --destination-password password

The password for the destination database.  Requires
:option:`--destination-type` `= database`

.. versionadded:: 2010-09-27

.. option:: --destination-database database

The database for the destination database, for use when only dumping a
single database.  Requires
:option:`--destination-type` `= database`

.. versionadded:: 2010-09-27

.. option:: --my-data-is-mangled

If your data is UTF8 but has been stored in a latin1 table using a latin1
connection then corruption is likely and drizzledump by default will retrieve
mangled data.  This is because MySQL will convert the data to UTF8 on the way
out to drizzledump and you effectively get a double-conversion to UTF8.

This typically happens with PHP apps that do not use 'SET NAMES'.

In these cases setting this option will retrieve the data as you see it in your
application.

.. versionadded:: 2011-01-31

.. option:: -h, --host hostname (=localhost)

The hostname of the database server.

.. option:: -u, --user username

The username for the database server.

.. option:: -P, --password password

The password for the database server.

.. option:: -p, --port port (=3306,4427)

The port number of the database server.  Defaults to 3306 for MySQL protocol
and 4427 for Drizzle protocol.

.. option:: --protocol protocol (=mysql)

The protocol to use when connecting to the database server.  Options are:

**mysql**
The standard MySQL protocol.

**drizzle**
The Drizzle protocol.

Backups using Drizzledump
-------------------------

Backups of a database can be made very simply by running the following::

$ drizzledump --all-databases > dumpfile.sql

This can then be re-imported into drizzle at a later date using::

$ drizzle < dumpfile.sql

MySQL Migration using Drizzledump
---------------------------------

As of version 2010-09-27 there is the capability to migrate databases from
MySQL to Drizzle using :program:`drizzledump`.

:program:`drizzledump` will automatically detect whether it is talking to a
MySQL or Drizzle database server.  If it is connected to a MySQL server it will
automatically convert all the structures and data into a Drizzle compatible 
format.

.. note::

   :program:`drizzledump` will by default try to connect via. port 4427 so to
   connect to a MySQL server a port (such as 3306) must be specified.

So, simply connecting to a MySQL server with :program:`drizzledump` as follows
will give you a Drizzle compatible output::

$ drizzledump --all-databases --host=mysql-host --port=3306 --user=mysql-user --password > dumpfile.sql

Additionally :program:`drizzledump` can now dump from MySQL and import directly
into a Drizzle server as follows::

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
 * set -> text
 * date/datetime default 0000-00-00 -> default NULL (Currently, ALL date columns have their DEFAULT set to NULL on migration)
 * date/datetime NOT NULL columns -> NULL
 * any date data containing 0000-00-00 -> NULL
 * time -> int of the number of seconds [1]_
 * enum-> DEFAULT NULL

.. rubric:: Footnotes

.. [1] This prevents data loss since MySQL's TIME data type has a range of
       -838:59:59 - 838:59:59, and Drizzle's TIME type has a range of
       00:00:00 - 23:59:61.999999.
