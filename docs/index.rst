.. Drizzle documentation master file, created by
   sphinx-quickstart on Fri Aug 27 08:33:41 2010.
   You can adapt this file completely to your liking, but it should at least
   contain the root `toctree` directive.

Welcome to Drizzle's documentation!
===================================

Drizzle is a transactional, relational, community-driven open source database that is forked from the popular MySQL database.

The Drizzle team has removed non-essential code, re-factored the remaining code and modernized the code base moving to C++.

Charter
 * A database optimized for Cloud infrastructure and Web applications
 * Design for massive concurrency on modern multi-cpu architecture
 * Optimize memory for increased performance and parallelism
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
   what_is_drizzle
   brief_history_of_drizzle
   how_to_report_a_bug
   mysql_differences

SQL Language:
-------------

.. toctree::
   :maxdepth: 2

   queries
   functions
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

Advanced Topics:
----------------

.. toctree::
   :maxdepth: 2

   storage_engines
   using_replication
   logging

Configuration:
^^^^^^^^^^^^^^
.. toctree::
   :maxdepth: 2

   configuration
   options

Plugins:
^^^^^^^^
.. toctree::
   :maxdepth: 2

Architecture:
^^^^^^^^^^^^^
.. toctree::
   :maxdepth: 2

   protocol
   replication

Clients:
^^^^^^^^
.. toctree::
   :maxdepth: 2

   clients/drizzle.rst
   clients/drizzledump.rst
   clients/drizzleadmin.rst

libdrizzle:
^^^^^^^^^^^
.. toctree::
   :maxdepth: 2

   libdrizzle/api.rst
   libdrizzle/developer.rst

Testing:

.. toctree::
   :maxdepth: 2
 
   testing/test-run.rst

Indices and tables
==================

* :ref:`genindex`
* :ref:`search`

