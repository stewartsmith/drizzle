.. _authorization:

Authorization
=============

Authorization is finding out if the person, once identified, is permitted to
have the resource. [1]_  

Drizzle authorization is handled by plugins. There is no single
source where users or access rights are defined, such as a system user table, 
but each auhtorization plugin will use different sources to define or store
access rights. By default no authorization plugin is loaded, this means that
any logged in user is authorized to access all database objects and do anything
he wants (everyone is super user).

The following authorization plugins are included with Drizzle:

* :doc:`/plugins/regex_policy/index` - ALLOW or REJECT access by matching a regular expression against the table name.
* :doc:`/plugins/simple_user_policy/index` - Allow a user to only access a schema that matches their username.

Limitations
-----------

At the moment there doesn't exist a plugin which would implement anything 
resembling the traditional SQL standard GRANT and REVOKE type of authorization.
You are invited to share your opinion on whether that level of authorization
control is necessary in a modern database.

Note that at the moment there also is no plugin that would distinguish between
read and write operations, rather access is always granted to schemas and tables
in an all or nothing fashion.

-------------------------------------------------------------------------------

.. rubric:: Footnotes

.. [1] `Authentication, Authorization, and Access Control <http://httpd.apache.org/docs/1.3/howto/auth.html>`_
