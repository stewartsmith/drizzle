.. _slave_details:

Slave Details
*************

The replication slave plugin creates two worker threads, each accessing a
work queue (implemented as an InnoDB table) that contains the replication
events. This is a producer/consumer paradigm where one thread populates the
queue (the producer), and the other thread (the consumer) reads events from
the queue.

The producer thread (or I/O thread) is in charge of connecting to the master
server and pulling down replication events from the master's transaction
log and storing them locally in the slave queue. It is required that the
master use the InnoDB replication log (:option:`--innodb.replication-log <drizzled --innodb.replication-log>`).

The consumer thread (or applier thread) reads the replication events from
the local slave queue, applies them locally, and then deletes successfully
applied events from the queue.
