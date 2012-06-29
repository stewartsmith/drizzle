.. _json_server_plugin:

JSON Server
===========

JSON Server implements a simple HTTP server that allows you to access your
Drizzle database with JSON based protocols. Currently two API's are supported:
a SQL-over-HTTP protocol allows you to execute any single statement SQL
transactions and a pure JSON protocol currently supports storing of JSON
documents as blobs in a key-value table.


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

.. option:: --json-server.schema ARG

   :Default: test
   :Variable: :ref:`json_server_schema <json_server_schema>`

   Schema which is used when not explicitly specified in request URI.
   Note: Currently this is in the /json API only.

.. option:: --json-server.table ARG

   :Default:
   :Variable: :ref:`json_server_table <json_server_table>`

   Table which is used when not explicitly specified in request URI.
   Note: Currently this is in the /json API only.

.. option:: --json-server.allow_drop_table ARG

   :Default: OFF
   :Variable: :ref:`json_server_allow_drop_table <json_server_allow_drop_table>`

   When json-server.allow_drop_table is set to ON, it is possible to drop
   a table with a HTTP DELETE request with no _id specified. When set to OFF
   (the default), omitting _id will result in an error.

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

.. _json_server_schema:

* ``json_server_schema``

    :Scope: Global
    :Dynamic: yes

   Schema which is used when not explicitly specified in request URI.
   Note: Currently this is in the /json API only.

.. _json_server_table:

* ``json_server_table``

    :Scope: Global
    :Dynamic: yes

   Table which is used when not explicitly specified in request URI.
   Note: Currently this is in the /json API only.

.. _json_server_allow_drop_table:

* ``json_server_allow_drop_table``

    :Scope: Global
    :Dynamic: yes

   When json-server.allow_drop_table is set to ON, it is possible to drop
   a table with a HTTP DELETE request with no _id specified. When set to OFF
   (the default), omitting _id will result in an error.

.. _json_server_apis:

APIs
----

JSON Server supports a few APIs that offer different functionalities. Each API
is accessed via it's own URI, and parameters can be given in the query string
or in the POST data. 

The APIs are versioned, the version number is prepended to the API name. If 
functionality is added or changed, it will not be available if an API is 
accessed via an earlier version number. Finally, the latest version of each API
is also available from the root, without any version number.

As of this writing, the following APIs exist:

.. code-block:: none

    /0.1/sql
    /0.2/sql
    /sql

Because the SQL API did not change between 0.1 and 0.2, all of the above URIs
are exactly the same.

.. code-block:: none

    /0.2/json
    /json

The pure JSON API did not exist in the 0.1 release, as you can see from above.

.. code-block:: none

    /version
    /

The ``/version`` URI will return the version of Drizzle (in a JSON document, of 
course):

.. code-block:: none

    $ curl http://localhost:8086/version
    {
      "json_server_version" : "0.3"
      "version" : "7.1.31.2451-snapshot"
    }

The root URI / returns a simple HTML GUI that can be used to test both the SQL 
and pure JSON APIs. Just point your browser to http://localhost:8086/ and try 
it!

.. _json_server_sql_api:

The SQL-over-HTTP API: /sql
---------------------------

The first API in JSON Server is the SQL-over-HTTP API. It allows you to execute
almost any SQL and the result is returned as a 2 dimensional JSON array.

On the HTTP level this is a simple API. The method is always ``POST`` and the
functionality is determined by the SQL statement you send.

.. code-block:: none
  
  POST /sql
  
  SELECT * from test.foo;

Returns:

.. code-block:: none

  {
   "query" : "SELECT * from test.foo;\n",
   "result_set" : [
      [ "1", "Hello Drizzle Day Audience!" ],
      [ "2", "this text came in over http" ]
   ],
   "sqlstate" : "00000"
  }

The above corresponds to the following query from a drizzle command line:

.. code-block:: mysql

  drizzle> select * from test.foo;

+----+-----------------------------+
| id | bar                         |
+====+=============================+
|  1 | Hello Drizzle Day Audience! | 
+----+-----------------------------+
|  2 | this text came in over http | 
+----+-----------------------------+


.. _json_server_json_api:

Pure JSON key-value API: /json
------------------------------

The pure JSON key-value API is found at the URI ``/json``. It takes a rather
opposite approach than the ``/sql`` API. Queries are expressed as JSON query 
documents, similar to what is found in Metabase, CouchDB or MongoDB. It is not
possible to use any SQL.

The purpose of the ``/json`` API is to use Drizzle as a key-value document 
storage. This means that the table layout is determined by the JSON Server 
module. Therefore, it is not possible for the user to access arbitrary 
relational tables via the ``/json`` API, rather tables must adhere to the 
format explained further below, and it must contain valid JSON documents in the 
data columns.

If you post (insert) a document to a table that doesn't exist, it will be 
automatically created. For this reason, a user mostly doesn't need to even
know the specific format of a JSON server table. 

.. _json_server_json_parameters:

Parameters
^^^^^^^^^^

Following parameters can be passed in the URI query string:

.. _json_server_json_parameters_id:

``_id``

   :Type: Unsigned integer
   :Mandatory: No
   :Default: 

   Optionally, a user may also specify the _id value which is requested. 
   Typically this is given in the JSON query document instead. If both are given
   the _id value in the query document has precedence.

.. _json_server_json_parameters_query:

``query``

   :Type: JSON query document
   :Mandatory: No
   :Default: 

   A JSON document, the so called *query document*. This document specifies
   which records/documents to return from the database. Currently it is only
   possible to query for a single value by the primary key, which is 
   called ``_id``. Any other fields in the query document will be ignored.

   The query parameter is used for GET, PUT and DELETE where it is passed in 
   URL encoded form in the URI query string. For POST requests the query 
   document is passed as the POST data. (In that case only the query document
   is passed, there is no ``query=`` part, in other words the data is never
   encoded in www-form-urlencoded format.)

   Example query document:

   .. code-block:: none

       { query:{"_id" : 1 }}

.. _json_server_json_parameters_schema:

``schema``

   :Type: String
   :Mandatory: No
   :Default: Specified by json_server_schema

   Name of the schema which we are querying. The schema must exist.

.. _json_server_parameters_table:

``table``

   :Type: String
   :Mandatory: No
   :Default: Specified by json_server_table

   Name of the table which we are querying. For POST requests, if the table 
   doesn't exist, it will be automatically created. For other requests the
   table must exist, otherwise an error is returned.

POSTing a document
^^^^^^^^^^^^^^^^^^

.. code-block:: none
  
  POST /json?schema=test&table=people HTTP/1.1

  {
    query:
    {
      "_id" : 2, 
      "document" : { "firstname" : "Henrik", "lastname" : "Ingo", "age" : 35}
    }
  }

Returns:

.. code-block:: none

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


(The use of Content-type: text/html is considered a bug and will be
fixed in a future version.)

Under the hood, this has inserted the following record into a table "jsontable":

.. code-block:: mysql

  drizzle> select * from people where _id=2;

+-----+--------------------------+
| _id | document                 |
+=====+==========================+
|   2 |{                         |
|     |"age" : 35,               |
|     |"firstname" : "Henrik",   |
|     |"lastname" : "Ingo"       |
|     |}                         |
+-----+--------------------------+

The ``_id`` field is always present. If it isn't specified, an auto_increment
value will be generated. If a record with the given ``_id`` already exists in
the table, the record will be updated (using REPLACE INTO).

In addition there are one or more columns of type TEXT.
The column name(s) corresponds to the top level key(s) that were specified in the
POSTed JSON document. You can use any name(s) for the top level key(s), but
the name ``document`` is commonly used as a generic name. The contents of such a
column is the value of the corresponding top level key and has to be valid JSON.

A table of this format is automatically created when the first document is
POSTed to the table. This means that the column names are defined from the top
level key(s) of that first document and future JSON documents must use the same 
top level key(s). Below the top level key(s) the JSON document can be of any 
arbitrary structure. A common practice is to always use ``_id`` and ``document``
as the top level keys, and place the actual JSON document, which can be of
arbitrary structure, under the ``document`` key.


GET a document
^^^^^^^^^^^^^^

The equivalent of an SQL SELECT is HTTP GET.

Below we use the query document ``{ "query" : {"_id" : 1 } }`` in URL encoded form:

.. code-block:: none
  
  GET /json?schema=test&table=people&query={%22query%22%20:%20{%20%22_id%22%20:%201}%20}

Returns

.. code-block:: none
  
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

It is also allowed to specify the ``_id`` as a URI query string parameter and
omit the query document:

.. code-block:: none
  
  GET /json?schema=test&table=people&_id=1

If both are specified, the query document takes precedence.

Finally, it is possible to issue a GET request to a table without specifying
neither the ``_id`` parameter or a query document. In this case all records of 
the whole table is returned.


Updating a record
^^^^^^^^^^^^^^^^^

To update a record, POST new version of json document with same ``_id`` as an 
already existing record.

(PUT is currently not supported, instead POST is used for both inserting and
updating.)

Deleting a record
^^^^^^^^^^^^^^^^^
 
Below we use the query document ``{ "query" : {"_id" : 1 } }`` in URL encoded form:

.. code-block:: none
  
  DELETE /json?schema=test&table=people&query={%22query%22%20:%20{%20%22_id%22%20:%201}%20}

Returns:

.. code-block:: none
  
  HTTP/1.0 200 OK
  Content-Type: text/html

  {
       "query" : {
              "_id" : 1
         },
       "sqlstate" : "00000"
  }

It is also allowed to specify the ``_id`` as a URI query string parameter and
omit the query document:

.. code-block:: none
  
  DELETE /json?schema=test&table=people&_id=1

If both are specified, the query document takes precedence.
 
.. _json_server_limitations:

Limitations
^^^^^^^^^^^

The ``/sql`` and ``/json`` APIs are both feature complete, yet JSON Server is
still an experimental module. There are known crashes, the module is still
single threaded and there is no authentication... and that's just a start! 
These limitations are being worked on. For a full list of the current state of 
JSON Server, please follow 
`this launchpad blueprint <https://blueprints.launchpad.net/drizzle/+spec/json-server>`_.

An inherent limitation is that each HTTP request is its own transaction. While
it would be possible to support maintaining a complex SQL transaction over the
span of multiple HTTP requests, we currently do not plan to support that.

.. _json_server_authors:

Authors
-------

Stewart Smith, Henrik Ingo, Mohit Srivastava

.. _json_server_version:

Version
-------

This documentation applies to **json_server 0.3**.

To see which version of the plugin a Drizzle server is running, execute:

.. code-block:: mysql

   SELECT MODULE_VERSION FROM DATA_DICTIONARY.MODULES WHERE MODULE_NAME='json_server'

Changelog
---------

v0.1
^^^^
* /sql API
* Simple web based GUI at /
* /version API

v0.2
^^^^
* /json API supporting pure JSON key-value operations (POST, GET, DELETE)
* Automatic creation of table on first post. 

v0.3
^^^^
* Test cases for /json API
* Major refactoring of the functionality behind /json
* Changed structure of the query document to be 
  ``{ "query" : <old query document> }`` This is to allow for future 
  extensibility.
* New options json_server.schema, json_server.table and 
  json_server.allow_drop_table