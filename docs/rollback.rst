ROLLBACK
========

The ROLLBACK command rolls back the current transaction and causes all updates made by the transaction to be discarded. ::

	ROLLBACK [WORK] TO [SAVEPOINT] identifier

Calling ROLLBACK causes the updates that were started to be discarded if no identifier is specified; otherwise, it rolls back to the identifier.

