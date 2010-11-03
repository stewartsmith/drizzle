ROLLBACK
========

ROLLBACK [WORK] TO [SAVEPOINT] identifier

Calling ROLLBACK causes the current transaction to undo the transaction that
has begun if no identifier is specified, otherwise it rolls back to the
identifier.

