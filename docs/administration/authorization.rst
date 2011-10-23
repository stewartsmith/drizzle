Authorization
=============

Authorization is finding out if the person, once identified, is permitted to
have the resource. [1]_  Drizzle authorization is handled by plugins; there
are no grant or privilege tables.

The following authorization plugins are included with Drizzle:

* :doc:`/plugins/regex_policy/index`
* :doc:`/plugins/simple_user_policy/index`

-------------------------------------------------------------------------------

.. rubric:: Footnotes

.. [1] `Authentication, Authorization, and Access Control <http://httpd.apache.org/docs/1.3/howto/auth.html>`_
