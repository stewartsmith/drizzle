Configuration
=============

Kernel Options
--------------

:option:`--transaction-message-threshold`
    Controls the size, in bytes, of the Transaction messages. When a Transaction
    message exceeds this size, a new Transaction message with the same
    transaction ID will be created to continue the replication events.
    See :ref:`bulk-operations` below.


:option:`--replicate-query`
    Controls whether the originating SQL query will be included within each
    Statement message contained in the enclosing Transaction message. The
    default global value is FALSE which will not include the query in the
    messages. It can be controlled per session, as well. For example:

    .. code-block:: mysql

       drizzle> set @@replicate_query = 1;

    The stored query should be used as a guide only, and never executed
    on a slave to perform replication as this will lead to incorrect results.

Kernel Variables
----------------

**transaction_message_threshold**

**replicate_query**
