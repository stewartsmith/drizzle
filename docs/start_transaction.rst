START TRANSACTION
======================

'TODO'

.. code-block:: mysql

	START TRANSACTION [WITH CONSISTENT SNAPSHOT]
	BEGIN

.. warning::

   If you are currently already in a transaction, Drizzle will give a warning
   stating that you are in a transaction in a similar way to PostgreSQL.
   This is instead of implicitly committing the transaction in the way MySQL
   does.
