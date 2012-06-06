.. _regex_policy_plugin:

Regex-based Authorization
=========================

:program:`regex_policy` is an :doc:`/administration/authorization` plugin
that uses regex patterns to match policies. When :program:`drizzled` is started with  ``--plugin-add=regex_policy``, the regex policy plugin is enabled with the default policy file. Policy file can be specified by either specifying ``--regex-policy.policy=<policy file>`` at the time of server startup or by changing the ``regex_policy_policy`` with ``SET GLOBAL``.

.. _regex_policy_loading:

Loading
-------

To load this plugin, start :program:`drizzled` with::

   --plugin-add=regex_policy

Loading the plugin may not enable or configure it.  See the plugin's
:ref:`regex_policy_configuration` and :ref:`regex_policy_variables`.

.. seealso:: :ref:`drizzled_plugin_options` for more information about adding and removing plugins.

.. _regex_policy_configuration:

Configuration
-------------

These command line options configure the plugin when :program:`drizzled`
is started.  See :ref:`command_line_options` for more information about specifying
command line options.

.. program:: drizzled

.. option:: --regex-policy.policy ARG

   :Default: :file:`drizzle.policy`
   :Variable: :ref:`regex_policy_policy <regex_policy_policy>`

   File to load for regex authorization policies.

.. _regex_policy_variables:

Variables
---------

These variables show the running configuration of the plugin.
See `variables` for more information about querying and setting variables.

.. _regex_policy_policy:

* ``regex_policy_policy``

   :Scope: Global
   :Dynamic: No
   :Option: :option:`--regex-policy.policy`

   File to load for regex authorization policies.

.. _regex_policy_file_format:

Regex Policy File Format
------------------------

The general line format of a regex policy file is::

   USER_PATTERN SCHEMA_OBJECT_PATTERN POLICY
   In Drizzle 7 and Drizzle 7.1 the POLICY values supported were 'ACCEPT' and 'DENY'. Beginning with Drizzle 7.2.0, the values used should be 'ALLOW' and 'DENY'. Although 'ACCEPT' and 'REJECT' are also supported for backward compatibility, but their use is deprecated.

For example::

   # This is a comment line and should be skipped
   .+ schema=DATA_DICTIONARY ALLOW
   .+ schema=INFORMATION_SCHEMA ALLOW
   .+ schema=data_dictionary ALLOW
   .+ schema=information_schema ALLOW
   root table=.+ ALLOW
   root schema=.+ ALLOW
   root process=.+ ALLOW
   user1 schema=user1 ALLOW
   user2 schema=user2 ALLOW
   user1 process=user1 ALLOW
   user2 process=user2 ALLOW
   # Default to denying everything
   .+ schema=.+ DENY
   .+ process=.+ DENY

Changing policy file at runtime
-------------------------------

Policy file can be reloaded by::

   SET GLOBAL regex_policy_policy=@@regex_policy_policy

Moreover, the policy file can be changed by::

   SET GLOBAL regex_policy_policy=/path/to/new/policy/file

Examples
--------

Sorry, there are no examples for this plugin.

.. _regex_policy_authors:

Authors
-------

Clint Byrum

.. _regex_policy_version:

Version
-------

This documentation applies to **regex_policy 2.0**.

To see which version of the plugin a Drizzle server is running, execute:

.. code-block:: mysql

   SELECT MODULE_VERSION FROM DATA_DICTIONARY.MODULES WHERE MODULE_NAME='regex_policy'

Changelog
---------

v2.0
^^^^
* First release.
