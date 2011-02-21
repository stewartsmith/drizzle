Drizzle Client Errors
=====================

Various errors can occur when trying to use the Drizzle command line clients,
this is a list of some of these and how to deal with them:

.. _old-passwords-label:

Old Password Error
------------------

::

   drizzle_state_handshake_result_read:old insecure authentication mechanism not supported

This error happens because the Drizzle client is trying to connect to a MySQL
server which has the password stored in the Old Password (or pre-MySQL-4.1)
format.  This is typically seen when connecting to a stock RedHat or CentOS
MySQL installation.

To resolve this, look for the old-passwords option in your MySQL configuration
and disable it.  Then update the password for the user you are trying to connect
with using the ``SET PASSWORD`` syntax so that it can be re-recorded in the
newer password format.
