.. program:: drizzled

.. _configuration_options:

Options
=======

Options configure Drizzle at startup.  When :program:`drizzled`
starts, it reads option values from three sources in this order:

#. Default values
#. :ref:`config_files`
#. :ref:`command_line_options`

Values from :ref:`command_line_options` have the highest precedence;
they override values from :ref:`config_files` which override the defaul
values.  The default values alone are sufficient to start :program:`drizzled`,
but since they provide only the most basic configuration, you will certainly
want to specify other options.

To see which options are available, run ``drizzled --help``.  You can also
see which options a plugin provides by running
``drizzled --plugin-add PLUGIN --help`` where ``PLUGIN`` is the name of any
plugin.  For example:

.. code-block:: none

   $ drizzled --plugin-add query_log --help
   sbin/drizzled  Ver 2011.08.25.2411 for pc-linux-gnu on i686 (Source distribution (trunk))
   Copyright (C) 2010-2011 Drizzle Developers, Copyright (C) 2008 Sun Microsystems
   This software comes with ABSOLUTELY NO WARRANTY. This is free software,
   and you are welcome to modify and redistribute it under the GPL license
   
   ...

   Options used by query_log:
     --query-log.file-enabled                      Enable query logging to file
     --query-log.file arg (=drizzled-queries.log)  Query log file
     --query-log.threshold-execution-time arg (=0) Threshold for logging slow 
                                                   queries, in microseconds

Options listed by ``--help`` can be used as :ref:`command_line_options`.
To use them in :ref:`config_files`, strip the leading ``--``.

.. note::

   Since Drizzle uses many plugins, the available options vary
   depending on which plugins are loaded.  If you cannnot find a
   certain option, ensure that the plugin which provides the option is
   loaded.

.. _setting_options:

Setting Options
---------------

Options are read from one or more of the following sources.  Each source
is optional.

.. _config_files:

Config Files
^^^^^^^^^^^^

Config files contain these types of lines:

.. code-block:: ini

   # comment
   option-name=value
   plugin-name.plugin-option-name=value
   [plugin-name]
   plugin-option-name=value

Blank lines are allowed, and spaces before and after ``=`` are allowed.

The second type of line, ``option-name=value``, specifies
:ref:`drizzled_options` which add and remove plugins and configure
the kernel.

The third type of line, ``plugin-name.plugin-option-name=value``,
specifies an option specific to a plugin.  Drizzle loads many plugins
by default, so many options use this type.  If plugins have the
same ``plugin-option-name``, they are distinguished by different
``plugin-name.`` prefixes.  For example:

.. code-block:: ini

   drizzle-protocol.port=4427
   mysql-protocol.port=3306

Those options are not the same.  The first sets the Drizzle network
protocol port, and the second sets the MySQL network protocol port.

The fourth type of line, ``[plugin-name]``, is a header that specifies
a plugin name to prefix to all the option names that follow.  The previous
example is equivalent to this:

.. code-block:: ini

   [drizzle-protocol]
   port=4427

   [mysql-protocol]
   port=3306

Once a header is declared, it remains in affect until another header
is declared, and the plugin name is prefixed to every option that follows,
so you cannot override the header plugin name by specifying a different
plugin name like this:

.. code-block:: ini

   [drizzle-protocol]
   port=4427
   mysql-protocol.port=3306  # WRONG

That config file is wrong and it will cause an error when Drizzle starts like
"unknown option drizzle-protocol.mysql-protocol.port".

Since the :ref:`drizzled_options` are not part of a plugin, they cannot
be specified after any header.  Therefore, you should specify all
:ref:`drizzled_options` at the start of the config file, or in a separate
config file by using :ref:`multiple_config_files`.

.. _command_line_options:

Command Line Options
^^^^^^^^^^^^^^^^^^^^

Command line options have the form ``--option-name=value`` (the ``=`` is
optional).  This form works for :ref:`drizzled options <drizzled_options>`
and all plugin options.  For example::

   drizzled --basedir=/opt/drizzle --innodb.buffer-pool-size=500M

.. _multiple_config_files:

Multiple Config Files
---------------------

The command line option :option:`--defaults-file` specifies one config file,
but :option:`--config-dir` specifies a directory which can contain multiple
config files.  If a file named :file:`drizzled.cnf` exists in the config dir,
it is read first.
If the config dir contains a directory called :file:`conf.d`, then *every*
file in that directory is read as a config file.  (Even hidden files are read,
including hidden temp files created by your editor while editing config files
in this directory.)

A good strategy for configuring Drizzle with multiple config files is to
put general :ref:`drizzled_options` in :file:`/etc/drizzle/drizzled.cnf`
(:file:`/etc/drizzle` is the default :option:`--config-dir` value)
and any options related to a plugin in a separate config file in
:file:`/etc/drizzle/conf.d/`.  For example:

.. code-block:: bash

   $ ls /etc/drizzle/*
   /etc/drizzle/drizzled.cnf

   /etc/drizzle/conf.d:
   auth-file

.. code-block:: bash

   $ cat /etc/drizzle/drizzled.cnf

.. code-block:: ini

   server-id=42
   core-file

.. code-block:: bash

   $ cat /etc/drizzle/conf.d/auth-file

.. code-block:: ini

   # drizzled option to load the auth_file plugin
   plugin-remove=auth_all
   plugin-add=auth_file
   
   # Options for the plugin itself
   [auth-file]
   users=/etc/drizzle/users

.. _boolean_options:

Boolean Options
---------------

Boolean options do not and cannot take values.
Most boolean options are disabled by default, so specifying them enables them.
For example, ``--transaction-log.enable`` enable the transaction log because
it is disabled by default.  However, some options are *enabled* by default,
so specifying them disables them.  For example, ``--innodb.disable-checksums``
disables InnoDB checkum validation because it is enabled by default.

