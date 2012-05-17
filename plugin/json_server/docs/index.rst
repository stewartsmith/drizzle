.. _json_server_plugin:

JSON Server
===========

JSON HTTP interface.

.. _json_server_loading:

Loading
-------

To load this plugin, start :program:`drizzled` with::

   --plugin-add=json_server

Loading the plugin may not enable or configure it.  See the plugin's
:ref:`json_server_configuration` and :ref:`json_server_variables`.

.. seealso:: :ref:`drizzled_plugin_options` for more information about adding and removing plugins.

.. _json_server_configuration:

Configuration
-------------

These command line options configure the plugin when :program:`drizzled`
is started.  See :ref:`command_line_options` for more information about specifying
command line options.

.. program:: drizzled

.. option:: --json-server.port ARG

   :Default: 8086
   :Variable: :ref:`json_server_port <json_server_port>`

   Port number to use for connection or 0 for default (port 8086) 

.. _json_server_variables:

Variables
---------

These variables show the running configuration of the plugin.
See `variables` for more information about querying and setting variables.

.. _json_server_port:

* ``json_server_port``

   :Scope: Global
   :Dynamic: No

   Port number to use for connection or 0 for default (port 8086) 

.. _json_server_examples:

Examples
--------
The idea is here to store a document which can be of any type and it will identify by a key `_id` it should be big integer.

The `localhost:8086` will point you to a demo GUI. The first query window uses SQL API 0.1 ans send request to `http://localhost:8086/sql` .
The second query window uses pure JSON key-value API and send request to `http://localhost:8086/json` .

Posting a document:
-------------------

Send a json request using second query box

.. http:post:: /json
.. code-block:: http
	
	POST /json?schema=test&table=jsontable HTTP/1.1
	Host: localhost:8086
	Content-Type: application/xml
        Accept: */*

	{
		"_id" : 2, 
		"document" : { "firstname" : "Henrik", "lastname" : "Ingo", "age" : 35}
	}
	
.. code-block:: http

	HTTP/1.1 200 OK
	Content-Type: text/html

	{
   		"query" : {
      				"_id" : 2,
      				"document" : {
         					"age" : 35,
         					"firstname" : "Henrik",
         					"lastname" : "Ingo"
      						}
   				},
   		"sqlstate" : "00000"
	}

:query schema: schema name. default is test.For this example, it is test.
:query table: table name. default is jsonkv. For this example,it is people.


Querying a Single query
-----------------------

.. http:get:: /json
.. code-block:: http
	
	GET /json?schema=test&table=people&query=%7B%22_id%22%20%3A%201%7D%0A HTTP/1.1
	Host: localhost:8086
	Accept: */*

.. code-block:: http
	
	HTTP/1.0 200 OK
	Content-Type: text/html
	
	{
		"query" : {
				"_id" : 1
   			},
   		"result_set" : [
      				{
         				"_id" : 1,
         				"document" : {
            						"age" : 21,
            						"firstname" : "Mohit",
            						"lastname" : "Srivastava"
         						}
      				}
   				],
   		"sqlstate" : "00000"
	}
	
:query schema: schema name. default is test. For this example, it is test.
:query table: table name. default is jsonkv. For this example, it is people.
:query query: JSON query. For this example, it is {"_id" : 1}

Updating a record:
------------------
To update a record ,POST new version of json document with same _id as an already existing record.

Deleting a record:
------------------
 
.. http:delete:: /json

.. code-block:: http
	
	DELETE http://14.139.228.217:8086/json?schema=test&table=people&query=%7B%22_id%22%20%3A%201%7D HTTP/1.1
	Host: localhost:8086
	Accept: */*

.. code-block:: http
	
	HTTP/1.0 200 OK
	Content-Type: text/html

	{
   		"query" : {
      				"_id" : 1
   			},
   		"sqlstate" : "00000"
	}

:query schema: schema name. default is test.For this example, it is test. 
:query table: table name. default is jsonkv. For this example, it is people.
:query query: JSON query. For this example,it is {"_id" : 1}
 
.. _json_server_authors:

Authors
-------

Stewart Smith
Henrik Ingo
Mohit Srivastava

.. _json_server_version:

Version
-------

This documentation applies to **json_server 0.1,0.2**.

To see which version of the plugin a Drizzle server is running, execute:

.. code-block:: mysql

   SELECT MODULE_VERSION FROM DATA_DICTIONARY.MODULES WHERE MODULE_NAME='json_server'

Changelog
---------

v0.1
^^^^
* First release.

v0.2
^^^^
* GET,POST,PUT and DELETE HTTP-JSON request for corresponding sql query.
* Automatic creation of table on first post request. 
