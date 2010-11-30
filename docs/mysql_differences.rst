=========================
Notable MySQL Differences
=========================

Drizzle was forked from the (now defunct) MySQL 6.0 tree in 2008. Since then there
have been a lot of changes. Some areas are similar, others unrecognizable.

This section aims to explore some of the notable differences between MySQL
and Drizzle.

This section was originally adapted from the Drizzle Wiki.

Usage
-----
 * There is no embedded server. The Drizzle Server is not loadable as a shared
   library.
 * Drizzle is optimized for massively concurrent environments. If we have the
   choice of improving performance for 1024 simultaneous connections to the
   detriment of performance with only 64 connections, we will take that choice.
 * Designed for modern POSIX systems
 * Microsoft Windows is not a supported platform (neither is HP-UX or IRIX).
 * No timezones. Everything is UTC.

Installation
------------

 * No scripts/mysql_install_db or similar. A "just works" installation
   mentality without administrative overhead.
 * No system database that needs upgrading between versions.
 * Drizzle can listen on the Drizzle port (4427) and/or MySQL port (3306)
   and speak the respective protocols.

Architecture
------------

Drizzle is designed around the concept of being a micro-kernel. There should
be a small core of the server with most functionality being provided through
small, efficient and hard to misuse plugin interfaces. The goal is a small,
light weight kernel which is easy to maintain, understand and extend.

Drizzle is written in C++ making use of the Standard Template Library (STL)
and Boost. Only where performance or correctness proves to be inadequate will
we consider rolling our own, and our preference is to fix the upstream library
instead.

Network protocol
----------------

Pluggable network protocols allow Drizzle to speak one (or more) of several
protocols. Currently we support the MySQL protocol (compatible with existing
MySQL client libraries) and the Drizzle protocol which is still under
development but aims for several important differences:

 * Client sends first packet instead of server
 * Built in sharding
 * Multi statement support (without using a semicolon to separate them)
 * Room for expansion to include NoSQL type commands inline with SQL.

There is also a console plugin that instead of providing access over a network
socket, allows access from the current tty.

Plugin API
----------

Existing plugin APIs inherited from MySQL have been reworked.

 * User Defined Functions (UDFs) now follow the same API as within the
   server instead of a different C API. This means that UDFs are on the
   exact same level as built-in functions.
 * Storage Engine API has had some parts extensively reworked, especially
   around transactions and DDL.
 * Logging is now pluggable
 * Authentication is pluggable
 * Replication is pluggable
 * INFORMATION_SCHEMA plugins have been replaced by the function_engine, which
   is a lot more space and time efficient.
 * Network protocols are pluggable
 * Scheduler is pluggable (multi_thread, pool_of_threads etc)
 * Plugin points for manipulating rows before/after operations: used for
   replication and the PBMS Blob Streaming plugin.

Stored Procedures
-----------------

Drizzle does not currently have any plugins implement stored procedures. We
viewed the implementation in MySQL to be non-optimal, bloating the parser
and only supporting one language (SQL2003 stored procedures) which was not
well known.

Fundamentally, stored procedures usually are not the correct architectural
decision for applications that need to scale. Pushing more computation down
into the database (which is the trickiest layer to scale) isn't a good idea.

We do recognize that the ability to reduce the time row locks are held
by using stored procedures is valuable, but think the same advantage can
be gotten with improved batching of commands over the wire instead of adding
administering stored procedures to the list of things that can go wrong in
administering the database.

Triggers
--------

Drizzle does not currently have any plugin that provides SQL triggers. We
have some hooks for callbacks inside the server so that plugins can hook
into points that triggers could.

Views
-----

SQL Views are not currently supported in Drizzle. We believe they should be
implemented via a query rewrite plugin. See the `Query Rewrite Blueprint <https://blueprints.launchpad.net/Drizzle/+spec/query-rewrite>`_ on launchpad.

Partitioning
------------

INFORMATION_SCHEMA
------------------

The INFORMATION_SCHEMA in Drizzle is strictly ANSI compliant. If you write
a query to any of the tables in the INFORMATION_SCHEMA in Drizzle, you can
directly run these on any other ANSI compliant system.

For information that does not fit into the standard, there is also the
DATA_DICTIONARY schema. Use of tables in DATA_DICTIONARY is non-portable.

This allows developers to easily know if the query is portable or not.

Authentication, Authorization and Access
----------------------------------------

Plugins. Currently there are PAM and HTTP AUTH plugins for authentication.
Through the PAM plugin, you can use any PAM module (such as LDAP).

Command line clients
--------------------

We've stopped the confusion: -p means port and -P means password.

No gotcha of using the unix socket when localhost is specified and then
connecting you to the wrong database server.

There is no Drizzle admin command.

Storage Engines
---------------

 * MERGE storage engine has been removed
 * FEDERATED storage engine has been removed (all current development is
   focused on FederatedX, so having FEDERATED made no sense).
 * CSV engine is now for temporary tables only. See the filesystem_engine for
   the future of reading files as database tables.
 * MyISAM is for temporary tables only.
 * ARCHIVE is fully supported
 * PBXT is merged

FRM Files
---------

There are no FRM files in Drizzle. Engines now own their own metadata.
Some choose to still store these in files on disk. These are now in a
documented file format (using the google protobuf library).

SHOW commands
-------------

Several SHOW commands have been removed, replaced with INFORMATION_SCHEMA
or DATA_DICTIONARY views. All SHOW commands are aliases to INFORMATION_SCHEMA
queries. Our INFORMATION_SCHEMA implementation does not have the drawbacks
of the MySQL implementation.

 * SHOW ENGINES: use DATA_DICTIONARY

Removed commands
----------------

 * ALTER TABLE UPGRADE
 * REPAIR TABLE
 * CREATE FUNCTION
 * CONVERT
 * SET NAMES

Operators Removed
-----------------

Bit operators: &&, >>, <<, ~, ^, |, &

Removed functions
-----------------

 * crypt()
 * bit_length()
 * bit_count()

Keywords removed
----------------
 * BIT_AND
 * BIT_OR
 * BIT_XOR
 * CIPHER
 * CLIENT
 * CODE
 * CONTRIBUTORS
 * CPU
 * DEFINER
 * DES_KEY_FILE
 * ENGINES
 * EVERY
 * IO
 * IPC
 * ISSUSER

Objects Removed
---------------

 * There is no requirement for a 'mysql' schema.
 * There is no SET datatype, use ENUM.
 * There is no SET NAMES command, UTF-8 by default
 * There is no CHARSET or CHARACTER SET commands, everything defaults to UTF8
 * There is no TIME type, use DATETIME or INT.
 * There is no TINYINT, SMALLINT or MEDIUMINT. Integer operations have been optimized around 32 and 64 bit integers.
 * There are no TINYBLOB, MEDIUMBLOB and LONGBLOB datatypes. We have optimized a single BLOB container.
 * There are no TINYTEXT, MEDIUMTEXT and LONGTEXT datatypes. Use TEXT or BLOB.
 * There is no UNSIGNED (as per the standard).
 * There are no spatial data types GEOMETRY, POINT, LINESTRING & POLYGON (go use `Postgres <http://www.postgresql.org>`_).
 * No YEAR field type.
 * There are no FULLTEXT indexes for the MyISAM storage engine (the only engine FULLTEXT was supported in). Look at either Lucene, Sphinx, or Solr.
 * No "dual" table.
 * The "LOCAL" keyword in "LOAD DATA LOCAL INFILE" is not supported
