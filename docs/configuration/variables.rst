.. program:: drizzled

.. _configuration_variables:

Variables
=========

Variables reflect the running configuration of Drizzle.
Variables allow you to configure (or reconfigure) certain values at runtime,
whereas :ref:`configuration_options` configure Drizzle at startup and cannot
be changed without restarting Drizzle.

Each variable has three aspects:

1. Corresponding option
2. Dynamic or static
3. Global or session

Most variables have a corresponding option which
initialize the variable.  For example, :option:`--datadir` corresponds to
and initializes the variable ``datadir``.  The variable name is derived
from the option name by stripping the option's leading ``--`` and changing
all ``-`` (hyphens) and ``.`` (periods) to ``_`` (underscores).

Dynamic variables can be changed at runtime and the change takes affect
immediately. Most variables, however, are static which means that they
only reflect the value of their corresponding option.  To change a static
variable, you must change its corresponding option, then restart Drizzle.
Certain variables are static and do not have a corresponding option because
they are purely informational, like ``version`` and ``version_comment``.

All variables are global, but each connection receives a copy of all global
variables called session variables.  A connection can change dynamic session
variables without affecting the rest of the server or other sessions, or it
can potentially change dynamic global variables which affects all connections.
Changes to dynamic session variables are lost when the connection is closed,
and changes to dynamic global variables remain in affect until changed again.

.. note:: Configuration variables and :ref:`user_defined_variables` are different.  :ref:`user_defined_variables` do not affect the configuration of Drizzle, and they are always dynamic, session variables.

.. _setting_variables:

Setting Variables
-----------------

The ``SET`` command sets global and session variables:

.. code-block:: mysql

   -- Set global variable
   SET GLOBAL variable=value;

   -- Set sesion variable
   SET variable=value
   SET SESSION variable=value

If setting the variable succeeds, the command finishes silently like:

.. code-block:: mysql

   drizzle> SET SESSION max_allowed_packet=10485760;
   Query OK, 0 rows affected (0.001475 sec)

Else, an error occurs if the variable cannot be changed:

.. code-block:: mysql

   drizzle> SET tmpdir="/tmp";
   ERROR 1238 (HY000): Variable 'tmpdir' is a read only variable

.. _querying_variables:

Querying Variables
------------------

The ``DATA_DICTIONARY.GLOBAL_VARIABLES`` and
``DATA_DICTIONARY.SESSION_VARIABLES`` are views for querying the
global and session variables respectively.  For example:

.. code-block:: mysql

   drizzle> SELECT * FROM DATA_DICTIONARY.GLOBAL_VARIABLES WHERE VARIABLE_NAME LIKE 'max%';
   +--------------------------+----------------+
   | VARIABLE_NAME            | VARIABLE_VALUE |
   +--------------------------+----------------+
   | max_allowed_packet       | 67108864       | 
   | max_error_count          | 64             | 
   | max_heap_table_size      | 16777216       | 
   | max_join_size            | 2147483647     | 
   | max_length_for_sort_data | 1024           | 
   | max_seeks_for_key        | 4294967295     | 
   | max_sort_length          | 1024           | 
   | max_write_lock_count     | -1             | 
   +--------------------------+----------------+

