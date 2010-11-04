Innobase
========

The Innobase plugin provides the InnoDB storage engine. It is almost identical
to the innodb_plugin, but adapted to Drizzle. We plan to move to having InnoDB
provided by the HailDB plugin, which will allow for easier maintenance and
upgrades.

InnoDB is the default storage engine for Drizzle. It is a fully transactional
MVCC storage engine.

innodb_plugin origins
---------------------

We maintain the Innobase plugin in Drizzle as a downstream project of the
innodb_plugin for MySQL. We try and keep it up to date with innodb_plugin
releases.

Differences from innodb_plugin
------------------------------

 * AUTO_INCREMENT behaves the standard way (as in MyISAM)
 * Supports four byte UTF-8 with the same index size

Compatibility with MySQL
------------------------

Although the innobase plugin is near identical to the innodb_plugin in MySQL,
the on disk formats are slightly incompatible (to allow for the same index
length for the four byte UTF-8 that Drizzle supports) and the table definitions
(FRM for MySQL, .dfe for Drizzle) are completely different. This means that you
cannot directly share InnoDB tablespaces between MySQL and Drizzle. Use the
drizzledump tool to migrate data from MySQL to Drizzle.
