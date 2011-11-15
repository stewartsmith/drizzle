.. _js_plugin:

JS
===========

.. code-block:: mysql

	JS(javascript_code [, arg1 [AS arg_name]] [, ...]) 

``JS()`` executes a JavaScript code snippet and returns the value of the last executed statement. Additional arguments are passed to the JavaScript environment and are available in the ``arguments[]`` array. If the optional ``AS arg_name`` is used, the same argument value is made available as a global variable with that name.


.. _js_loading:

Loading
-------

This plugin is loaded by default.

If you want to prevent the loading of this plugin, start :program:`drizzled` with::

   --plugin-remove=js

.. _js_examples:

Examples
--------

The first argument is required and should be a string of valid JavaScript code. The value of the last statement is returned, note that you should not use a ``return`` keyword. This is a top level JavaScript code snippet, not a JavaScript function.

.. code-block:: mysql

	SELECT JS('var d = new Date(); "Drizzle started running JavaScript at: " + d;');

Will output

+----------------------------------------------------------------------------------+
| JS('var d = new Date(); "Drizzle started running JavaScript at: " + d;')         |
+==================================================================================+
| Drizzle started running JavaScript at: Mon Aug 29 2011 00:23:31 GMT+0300 (EEST)  |
+----------------------------------------------------------------------------------+


Additional arguments are passed to the JavaScript environment and are available in the ``arguments[]`` array. 

.. code-block:: mysql

	SELECT JS("arguments[0] + arguments[1] + arguments[2];", 1, 2, 3) AS 'JS(...)';

Will output

+--------------+
| JS(...)      |
+==============+
| 6            |
+--------------+



If the optional ``AS arg_name`` is used, the same argument value is made available as a global variable with that name.

.. code-block:: mysql

	SELECT JS("first + second + third;", 1 AS 'first', 2.0 AS 'second', 3.5 AS 'third') AS 'JS(...)';

Will output

+--------------+
| JS(...)      |
+==============+
| 6.5          |
+--------------+

.. _json_parse:

Using JS() to parse JSON documents
-----------------------------------

JavaScript includes a JSON parser. This means you can use ``JS()`` as a JSON parser, and optionally use JavaScript to manipulate or select fragments of the JSON document. To do this, pass your JSON document as an argument, and use the ``JSON.parse()`` method to return it as a JavaScript object:

.. code-block:: mysql

	SELECT JS("var jsondoc = JSON.parse(arguments[0]); jsondoc['name']['firstname'];", 
	          '{ "name" : {"firstname" : "Henrik", "lastname" : "Ingo"} }') AS 'JS(...)';

Will output

+--------------+
| JS(...)      |
+==============+
| Henrik       |
+--------------+


To return a JSON document from JavaScript, use ``JSON.stringify()``:

.. code-block:: mysql

	SELECT JS("var jsondoc = JSON.parse(arguments[0]); 
	           JSON.stringify(jsondoc['name']);", 
	          '{ "name" : {"firstname" : "Henrik", "lastname" : "Ingo"} }') AS 'JS(...)';


Will output

+------------------------------------------+
| JS(...)                                  |
+==========================================+
| {"firstname":"Henrik","lastname":"Ingo"} |
+------------------------------------------+

Note that since a Drizzle function can only return scalar values, if you want to return arrays or objects from your JavaScript, JSON is a recommended way of doing that.

.. _js_queries:

Using JS in queries, passing columns as arguments
-------------------------------------------------

Naturally, the arguments can also be columns in a query. For instance in the case of JSON data, if you have stored JSON documents as TEXT or BLOB in a table, you can now use ``JSON.parse()`` to select individual fields out of it:

.. code-block:: mysql

	CREATE TABLE t (k INT PRIMARY KEY auto_increment, v TEXT);
	INSERT INTO t (v) VALUES ('{ "person" : { "firstname" : "Roland", "lastname" : "Bouman" } }');
	INSERT INTO t (v) VALUES ('{ "person" : { "firstname" : "Henrik", "lastname" : "Ingo" } }');
	INSERT INTO t (v) VALUES ('{ "person" : { "firstname" : "Brian", "lastname" : "Aker" } }');
	SELECT JS('var person = JSON.parse(jsondoc); person["person"]["firstname"];', 
	          v as jsondoc) AS 'JS(...)' 
	FROM t WHERE k=2;


Will output

+--------------+
| JS(...)      |
+==============+
| Henrik       |
+--------------+


And

.. code-block:: mysql

	SELECT k, JS('var person = JSON.parse(jsondoc); person["person"]["firstname"];', 
	             v as jsondoc) AS 'firstname', 
	          JS('var person = JSON.parse(jsondoc); person["person"]["lastname"];', 
	             v as jsondoc) AS 'lastname' 
	FROM t;

Will break your unstructured JSON data back into a relational table:

+---+-----------+----------+
| k | firstname | lastname |
+===+===========+==========+
| 1 | Roland    | Bouman   |
+---+-----------+----------+
| 2 | Henrik    | Ingo     |
+---+-----------+----------+
| 3 | Brian     | Aker     |
+---+-----------+----------+

.. _js_stored_procedure_surrogate:

Using JS as surrogate for stored procedures:
--------------------------------------------

Especially if the JavaScript you want to use is more complex, it might be a good idea to store the javascript itself in a table in Drizzle, or alternatively a variable. This simplifies queries that use the script:

.. code-block:: mysql

	CREATE TABLE sp (name VARCHAR(255) PRIMARY KEY, script TEXT);
	INSERT INTO sp (name, script) VALUES ('get_person_property', 'var person = JSON.parse(jsondoc); person["person"][property];');
	SELECT k, JS( (SELECT script FROM sp WHERE name='get_person_property'), 
	             v as jsondoc, 'firstname' as 'property') AS 'firstname', 
	          JS( (SELECT script FROM sp WHERE name='get_person_property'), 
	             v as jsondoc, 'lastname' as 'property') AS 'lastname' 
	FROM t;


Will output the same result as above:

+---+-----------+----------+
| k | firstname | lastname |
+===+===========+==========+
| 1 | Roland    | Bouman   |
+---+-----------+----------+
| 2 | Henrik    | Ingo     |
+---+-----------+----------+
| 3 | Brian     | Aker     |
+---+-----------+----------+

.. _js_future_work:

Limitations and future work
---------------------------

The current version of ``JS()`` is complete in the sense that any type of arguments (integer, real, decimal, string, date) can be used, JavaScript code can be of arbitrary complexity and scalar values of any type can be returned. However, apart from the input parameters and the return value, there is no way to interact with Drizzle from the JavaScript environment. The plan is that in a future version ``JS()`` will expose some Drizzle API's, such as the ``Execute()`` API, so that one could query Drizzle tables and call other Drizzle functions from the JavaScript environment. This would essentially make JS() a form of JavaScript stored procedures. Of course, a next step after that could be to actually support ``STORED PROCEDURE`` syntax and permissions.

Values of type ``DECIMAL`` will be passed as JavaScript ``Double`` values. This may lead to loss of precision. If you want to keep the precision, you must explicitly cast ``DECIMAL`` values into ``CHAR`` when you pass them as arguments. Note that this will affect how the JavaScript ``+`` operator works on the value (string concatenation instead of addition).

The current version lacks several obvious performance optimizations. Most importantly the v8 JavaScript engine is single threaded, so heavy use of ``JS()`` on busy production servers is not recommended. A future version will use the v8 Isolate class to run several instances of the single threaded v8 engine.

.. _js_authors:

Authors
-------

Henrik Ingo

Thanks to Roland Bouman for suggesting to use v8 engine instead of just a JSON parser and for review and comments on JavaScript and JSON conventions.

.. _js_version:

Version
-------

This documentation applies to **js 0.9**.

To see which version of the plugin a Drizzle server is running, execute:

.. code-block:: mysql

   SELECT MODULE_VERSION FROM DATA_DICTIONARY.MODULES WHERE MODULE_NAME='js'


Changelog
---------

v0.9
^^^^
* First release. Complete JS() functionality, but no APIs back to Drizzle are exposed yet and several performance optimizations were left for later release.
