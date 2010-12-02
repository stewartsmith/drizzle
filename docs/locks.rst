User Definied Locks
===================

A user can create a "lock" which is scoped to their user space. Multiple
session of the user can see the lock.

SELECT GET_LOCK();

SELECT GET_LOCKS();

SELECT RELEASE_LOCK();

SELECT RELEASE_LOCK();

SELECT RELEASE_LOCKS();

SELECT is_free_lock();

SELECT is_used_lock();

SELECT wait_for_lock();

SELECT release_lock_and_wait();

If a session should exit, whatever locks it was holdering will be deleted.

Please note, get_lock() was designed to be compatible with MySQL, if you
hold any locks when calling get_lock() they will be released. For this
reason you may want to consider calling get_locks() instead.

release_lock_and_wait() released the named lock, and then waits for another
session to try to obtain ownership. If it does not own the lock, it returns
with a zero.

Information on all barriers can be found in the DATA_DICTIONARY.USER_LOCKS
table;
