.. _documentation-label:

Contributing Documentation
==========================

Our documentation is written using
`Sphinx Documentation Generator <http://sphinx.pocoo.org/>`_
which uses the `reStructuredText <http://docutils.sf.net/rst.html>`_ format.

All our documentation is stored in the main source reposatory in the docs/
directory, and every merge into trunk triggers a rebuild of our
`documentation site <http://docs.drizzle.org/>`_.

Requirements
------------

We require a minimum of python-sphinx 0.6 to compile the documentation but not
all of it will build correctly in that version.  For correct documentation
version 1.0 is required.

When building the documentation warnings are treated as errors, so the documentation
needs to be warning free in 1.0.

Documentation Files
-------------------

The documentation files have the .rst extension and are stored in the ``docs/``
directory.

Contents
^^^^^^^^

To add a new document to the contents you will need to add it to a relevent
section in the ``docs/index.rst`` file.

Plugins
^^^^^^^

To document plugins you need to create a ``docs/`` subdirectory in your plugin
directory containing an index.rst file.  This will be automatically added during
the ``config/autorun.sh`` phase.

Documentation Format
--------------------

The documentation is in the reStructured text format which is a wiki-like markup
language ideal for documentation.

Headings
^^^^^^^^

Headings take the form of just the text with a diffent style of underline to
signify the level, for example:

.. code-block:: rest
   
   Heading Level 1
   ===============

   Heading Level 2
   ---------------

   Heading Level 3
   ^^^^^^^^^^^^^^^

Tables
^^^^^^

There are two ways of generating a table, the first is basically drawing the
entire table in ascii art as follows:

.. code-block:: rest

   +----+--------+-----+
   | ID | name   | age |
   +====+========+=====+
   | 1  | Fred   | 23  |
   +----+--------+-----+
   | 2  | Joe    | 48  |
   +----+--------+-----+
   | 3  | Sophie | 32  |
   +----+--------+-----+

Which generates:

+----+--------+-----+
| ID | name   | age |
+====+========+=====+
| 1  | Fred   | 23  |
+----+--------+-----+
| 2  | Joe    | 48  |
+----+--------+-----+
| 3  | Sophie | 32  |
+----+--------+-----+

As an alternative there is a simplified form as follows:

.. code-block:: rest

   ----- ----- ------
     A     B   Result
   ===== ===== ======
   False False False
   False True  True
   True  False True
   True  True  False
   ===== ===== =====

Gives this table:

===== ===== ======
  A     B   Result
===== ===== ======
False False False
False True  True
True  False True
True  True  False
===== ===== ======

In this form there must be more than one row to the table and the first column
can only have a single line.

Syntax Highlighting
^^^^^^^^^^^^^^^^^^^

Sphinx supports syntax highlighting using the `Pygments <http://pygments.org/>`_
engine.  For example, with the *mysql* syntax highlighter you can do the
following:

.. code-block:: rest

   .. code-block:: mysql

      SELECT * FROM t1 WHERE a=1;

Which will generate:

.. code-block:: mysql

   SELECT * FROM t1 WHERE a=1;

You can also add line numbers as follows:

.. code-block:: rest

   .. code-block:: c
      :linenos:

      #include <stdio.h>

      main()
      {
        printf("hello world");
      }

Which will generate:

.. code-block:: c
   :linenos:

   #include <stdio.h>

   main()
   {
     printf("hello world");
   }

.. seealso::

   `Pygments Demo <http://pygments.org/demo/>`_ - A demo of the available syntax
   highlighting types.

Footnotes
^^^^^^^^^

Footnotes in their most simple form can be generated using ``[1]_`` in the text
and then a section of the bottom of the page as follows [1]_:

.. code-block:: rest

   .. rubric:: Footnotes

   .. [1] Like this

Which generates:

.. rubric:: Footnotes

.. [1] Like this

Notes, Warnings, Todo and See Also
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

Notes, warnings and todos all take similar forms:

.. code-block:: rest

   .. note::
      This is a note

   .. warning::
      This is a warning

   .. todo::
      This is a todo

   .. seealso::
      This is a See Also

Which generates:

.. note::
   This is a note

.. warning::
   This is a warning

.. todo::
   This is a todo

.. seealso::
   This is a See Also

Summary
^^^^^^^

.. seealso::

   * `Sphinx documentation <http://sphinx.pocoo.org/contents.html>`_
   * `Openalea Sphinx Cheatsheet <http://openalea.gforge.inria.fr/doc/openalea/doc/_build/html/source/sphinx/rest_syntax.html>`_

Building Documentation
----------------------

The documentation is compiled from the ``.rst`` files during ``make``, but to
create HTML files from this you will need to run ``make html``.

To see other possible formats run ``make sphinx-help``.

Committing Documentation
------------------------

Documentation needs to be committed and merged in exactly the same way as
regular development files.  For more information on this please see the
:ref:`contributing code <code-label>` page.
