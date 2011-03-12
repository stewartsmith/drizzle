ALTER SCHEMA
============

ALTER SCHEMA changes the definition of a schema.

You must own the schema to use ALTER SCHEMA. To rename a schema you
must also have the CREATE privilege for the database. To alter the
owner, you must also be a direct or indirect member of the new owning
role, and you must have the CREATE privilege for the database:

.. code-block:: mysql

	ALTER SCHEMA name RENAME TO new_name
	ALTER SCHEMA name OWNER TO new_owner

name

    The name of an existing schema. 

new_name

    The new name of the schema.

new_owner

    The new owner of the schema. 


.. seealso::

   :doc:`create_schema` and :doc:`drop_schema`
