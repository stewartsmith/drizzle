CREATE SCHEMA
==============

TODO


.. code-block:: mysql

   CREATE SCHEMA [IF NOT EXISTS] schema_name
     [engine_options] ...

engine_options
--------------

TODO

::

    engine_option [[,] engine_option] ...

collate
-------

TODO

::

  [DEFAULT] COLLATE = collation_name
  { engine_specific }


REPLICATE
---------

Specify whether or not a SCHEMA should be replicated.

::

  REPLICATE=[TRUE|FALSE] 
