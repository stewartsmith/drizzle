.. _auth_all@:


.. Drizzle documentation master file, created by
   sphinx-quickstart on Fri Aug 27 08:33:41 2010.
   You can adapt this file completely to your liking, but it should at least
   contain the root `toctree` directive.

Welcome to Drizzle's documentation!
===================================

.. image:: logo.png

Drizzle is a transactional, relational, community-driven open-source database that is forked from the popular MySQL database.

The Drizzle team has removed non-essential code, has re-factored the remaining code, and has converted the code to modern C++ and modern libraries.

Charter
 * A database optimized for Cloud infrastructure and Web applications
 * Design for massive concurrency on modern multi-CPU architectures
 * Optimize memory use for increased performance and parallelism
 * Open source, open community, open design

Scope
 * Re-designed modular architecture providing plugins with defined APIs
 * Simple design for ease of use and administration
 * Reliable, ACID transactional

Introduction
------------
.. toctree::
   :maxdepth: 2
   
   preface
   license
   what_is_drizzle
   brief_history_of_drizzle
   how_to_report_a_bug
   help
   versioning
   mysql_differences

Contributing
------------
.. toctree::
   :maxdepth: 2

   contributing/getting_started
   contributing/code
   contributing/documentation
   contributing/more_ways

Installing
----------
.. toctree::
   :maxdepth: 2

   installing/requirements
   installing/ubuntu
   installing/redhat
   installing/from_source

Configuration
-------------
.. toctree::
   :maxdepth: 2

   configuration/index
   configuration/drizzled

Administration
--------------
.. toctree::
   :maxdepth: 2
   
   administration/index
   administration/drizzled
   administration/authentication
   administration/authorization
   administration/logging
   administration/plugins
   administration/storage_engines

Replication
-----------
.. toctree::
   :maxdepth: 2
   :glob:

   replication/index
   replication/replicators/index
   replication/appliers/index
   replication/messages/index
   replication/examples/index

SQL Language
------------
.. toctree::
   :maxdepth: 2

   queries
   functions/overview
   data_types
   ddl
   dml
   columntypes 
   variables 
   locks 
   barriers 
   dynamic 
   getting_information 
   transactional 
   administrative
   resources/index

Clients
-------
.. toctree::
   :maxdepth: 2

   clients/drizzle.rst
   clients/drizzledump.rst

Plugins
-------
.. toctree::
   :maxdepth: 2

   plugins/list
   
Release Notes
-------------

.. toctree::
   :maxdepth: 2

   release_notes/drizzle-7.0

libdrizzle
----------
.. toctree::
   :maxdepth: 2

   libdrizzle/api.rst
   libdrizzle/developer.rst
   protocol

Testing
-------
.. toctree::
   :maxdepth: 2
 
   testing/test-run.rst
   testing/kewpie.rst
   testing/randgen.rst
   testing/sql-bench.rst
   testing/sysbench.rst
   testing/writing_tests.rst
   testing/dbqp.rst
   testing/drizzletest_commands.rst

Indices and tables
==================

* :ref:`genindex`
* :ref:`search`

