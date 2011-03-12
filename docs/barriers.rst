User Defined Barriers
=====================

.. code-block:: mysql

  SELECT create_barrier();

  SELECT release_barrier();

  SELECT wait();

  SELECT wait_until();

  SELECT signal();

A barrier is a synchronization object which can be used to synchronize
a group of sessions to a specific rendezvous by calling wait(). When
wait() is called, any session of the user may call signal(), allowing
all sessions being held by wait() to proceed.

Barriers can optionally be created with a limit so that once a set
number of sessions have called wait() that all "waiters" are then
allowed to proceed.

The session that creates the barrier via create_barrier() is not
allowed to call either wait() or wait_until().

The scope of barriers is to the given username.

Beyond waiters, you can also create observers by using the
wait_until() function. Observers are released not only when signal()
or release_barrier() is called, but also when their definitive
predicate happens. You can use wait_until() to have a session wait for
a certain number of waiters to occur, and then do some body of work
before the waiters() are signaled to continue.

All waiters and observers are released if release_barrier() is called
by the session which created the barrier. Also, if the session that
created the barrier disconnects, all waiters and observers are
notified.

Information on all barriers can be found in the DATA_DICTIONARY.USER_BARRIERS table.

