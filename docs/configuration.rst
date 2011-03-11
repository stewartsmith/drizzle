=============
Configuration
=============

--------
Overview
--------

Drizzle can draw its configuration from a number of sources, including the
command line, from configuration files, and from environment variables.

Support is planned for pluggable configuration sources.

----------------
Loading Sequence
----------------

Drizzle first reads the command line options dealing with config file
location. These options may only be given as command line options.
Then, the config files are parsed, for all options. After that,
environment variables are processed, and any value given in them will
override values input from the config files. Finally, values on the command
line will be processed and any options given here take final precedence.

----------------
Format and Rules
----------------

Command line options are of the form `--option-name=value`. There are some
boolean flags, such as `--help` which do not require (nor can accept) an
option value.  See :ref:`options` for all options that :program:`drizzled`
supports.

Environment variables are the same as the command line options, except that
the variable name is prefixed with *DRIZZLED_*, in all caps and all `.` and
`-` are turned into underscores. So the option
`--innodb.buffer_pool_size=10` could be given in the environment variable
*DRIZZLED_INNODB_BUFFER_POOL_SIZE*

The config files contain a set of lines of the form `option-name=value`, one
per line. Due to a bug in Boost.Program_options Boolean values require an argument,
e.g. `console.enable=true`.

Config files support section headers such as `[innodb]` with all options
occuring subsequently being prefixed by the section header. For instance, if
one were do give:

.. code-block:: ini

  [innodb]
  buffer_pool_size=10M
  log_file_size=5M

It would be the same as:

.. code-block:: ini

  innodb.buffer_pool_size=10M
  innodb.log_file_size
  
