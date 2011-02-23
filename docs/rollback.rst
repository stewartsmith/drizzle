ROLLBACK
========

The ROLLBACK command rolls back the current transaction and causes all updates made by the transaction to be discarded. ::

	ROLLBACK [WORK] TO [SAVEPOINT] identifier

If no identifier is specified, calling ROLLBACK causes the updates that were started to be discarded. Otherwise, it rolls back to the identifier.

See :doc:`savepoints` for more information.
