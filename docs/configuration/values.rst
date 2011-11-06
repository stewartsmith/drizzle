.. program:: drizzled

.. _configuration_values:

Values
======

All :ref:`configuration_options` and :ref:`configuration_variables` have
values (NULL values are not allowed), and all values have a generic
type.  Value types are similar to, but simpler than, the SQL language
:doc:`/data_types`.

Currently, options' value types are not documented, but most options clearly
imply their value type.

Value Types
-----------

Numeric
^^^^^^^

Numeric values are positive or negative integers.  Some numeric values
are constrained, which means that the value must be within a specific
range.  Currently, constraints are not documented, but
:ref:`setting a variable <setting_variables>` out-of-range will cause
an error.

String
^^^^^^

String values are any kind of quoted strings.  Since NULL values are
not allowed, empty strings are sometimes used to mean "no value" or
"undefined".

Boolean
^^^^^^^

When :ref:`setting a variable <setting_variables>`, boolean values are
one of the various true and false values listed for the
:doc:`/boolean_data_type`.  When setting :ref:`boolean_options`,
no value is need or allowed.

Size
^^^^

Size values are like numeric values but allow optional size suffixes:
K, M, and G.  The size value ``1k`` is equivalent to the numeric value
``1024``.  Size values are shown as their equivalent numeric values
when you :ref:`query the variables <querying_variables>`.

.. note::

   Only :ref:`configuration_options` can use size values.
   :ref:`configuration_variables` must use numeric values.

Path
^^^^

Path values that are not absolute are prefixed with a path in the
Drizzle installation directory.  For example, if :option:`--pid-file`
is set to :file:`drizzled.pid`, Drizzle prefixes that value with
the :option:`--datadir` value, resulting in a value like
:file:`/opt/drizzle/data/drizzled.pid`.

Default Values
--------------

If an option has a default value, ``drizzled --help`` gives it in parentheses
after ``arg``, like ``(=drizzled-queries.log)``.  The relevant documentation
for an option should also list its default value, if any.
