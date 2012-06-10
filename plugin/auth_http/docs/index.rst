.. _auth_http_plugin:

HTTP Authentication
===================

:program:`auth_http` is an :doc:`/administration/authorization` plugin that authenticates connections using web-based HTTP authentication through a URL. 
When :program:`drizzled` is started with  ``--plugin-add=auth_http``, the http based authorization plugin is loaded. To enable the plugin, it is required to provide the server url against which the authentication is being made using ``--auth-http.url=<server url>``. The authetication server url can be dynamically changed using ``SET GLOBAL auth_http_url="<new server url>"``.
A web server is required to provide authentication.  For example, see Apache's documentation
for `Authentication, Authorization and Access Control <http://httpd.apache.org/docs/2.0/howto/auth.html>`_.
Currently, SSL connections are not supported.

.. note::

   Unload the :doc:`/plugins/auth_all/index` plugin before using this plugin.

.. seealso:: :doc:`/administration/authentication` 

.. _auth_http_loading:

Loading
-------

To load this plugin, start :program:`drizzled` with::

   --plugin-add=auth_http

Loading the plugin may not enable or configure it.  See the plugin's
:ref:`auth_http_configuration` and :ref:`auth_http_variables`.

.. seealso:: :ref:`drizzled_plugin_options` for more information about adding and removing plugins.

.. _auth_http_configuration:

Configuration
-------------

These command line options configure the plugin when :program:`drizzled`
is started.  See :ref:`command_line_options` for more information about specifying
command line options.

.. program:: drizzled

.. option:: --auth-http.url ARG

   :Default: 
   :Variable: :ref:`auth_http_url <auth_http_url>`
   :Required: **This option must be specified**

   URL for http authentication (required).  If this plugin is loaded, then
   this option must be specified, else :program:`drizzled` will not start.

.. _auth_http_variables:

Variables
---------

These variables show the running configuration of the plugin.
See `variables` for more information about querying and setting variables.

.. _auth_http_url:

* ``auth_http_url``

   :Scope: Global
   :Dynamic: Yes
   :Option: :option:`--auth-http.url`

   URL for HTTP authentication.

.. _auth_http_examples:

Examples
--------

Sorry, there are no examples for this plugin.

.. _auth_http_authors:

Authors
-------

Mark Atwood

.. _auth_http_version:

Version
-------

This documentation applies to **auth_http 0.1**.

To see which version of the plugin a Drizzle server is running, execute:

.. code-block:: mysql

   SELECT MODULE_VERSION FROM DATA_DICTIONARY.MODULES WHERE MODULE_NAME='auth_http'

Chanagelog
----------

v0.1
^^^^
* First release.
