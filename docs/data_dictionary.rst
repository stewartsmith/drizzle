DATA_DICTIONARY
===============

The DATA_DICTIONARY is a Drizzle extension that provides information
on the state of the database, and on the definitions of tables and
other objects. In other contexts this is what might be referred to as
the system catalog.

Plugins may add extra DATA_DICTIONARY tables with information specific to them.

If you wish to write portable tools you should make use of the INFORMATION_SCHEMA.

That table contains the name and value of the :doc:`/variables` that
the user has created during the current session.

.. todo::
   
   the above is specific to the VARIABLES table.
