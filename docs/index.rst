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


Introduction:
-------------
.. toctree::
   :maxdepth: 2
   
   preface
   license
   what_is_drizzle
   brief_history_of_drizzle
   how_to_report_a_bug
   versioning
   mysql_differences

Compiling and Installing:
-------------------------
.. toctree::
   :maxdepth: 2

   installing/requirements
   installing/from_source
   installing/ubuntu
   installing/redhat

Contributing:
-------------
.. toctree::
   :maxdepth: 2

   contributing/introduction
   contributing/code
   contributing/documentation

SQL Language:
-------------
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

Replication:
------------
.. toctree::
   :maxdepth: 2

   replication

Configuration:
--------------
.. toctree::
   :maxdepth: 2

   configuration
   options

Administrative:
---------------
.. toctree::
   :maxdepth: 2

   logging
   authentication
   storage_engines

Plugins:
^^^^^^^^
.. toctree::
   :maxdepth: 2

   plugins/list

Clients:
--------
.. toctree::
   :maxdepth: 2

   clients/drizzle.rst
   clients/drizzledump.rst
   clients/errors.rst

libdrizzle:
-----------
.. toctree::
   :maxdepth: 2

   libdrizzle/api.rst
   libdrizzle/developer.rst
   protocol

Testing:
--------
.. toctree::
   :maxdepth: 2
 
   testing/test-run.rst
   testing/dbqp.rst
   testing/randgen.rst
   testing/sql-bench.rst
   testing/sysbench.rst
   testing/writing_tests.rst

Indices and tables
==================

* :ref:`genindex`
* :ref:`search`

