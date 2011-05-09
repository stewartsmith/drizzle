User Defined Locks
===================

A user can create a "lock" which is scoped to their user space. Multiple
session of the user can see the lock.

.. code-block:: mysql
   
   SELECT GET_LOCK();

   SELECT GET_LOCKS();

   SELECT RELEASE_LOCK();

   SELECT RELEASE_LOCK();

   SELECT RELEASE_LOCKS();

   SELECT is_free_lock();

   SELECT is_used_lock();

If a session should exit, whatever locks it was holding will be released.

.. todo::

	are locks recursive?

Please note, get_lock() was designed to be compatible with MySQL. If you
hold any locks when calling get_lock() they will be released. For this
reason you may want to consider calling get_locks() instead.

Information on all barriers can be found in the DATA_DICTIONARY.USER_LOCKS
table;
