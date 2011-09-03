Regex-based Authorization
=========================

:program:`regex_policy` is an :doc:`/administration/authorization` plugin
that uses regex patterns to match policies.

.. _regex_policy_loading:

Loading
-------

To load this plugin, start :program:`drizzled` with::

   --plugin-add=regex_policy

Loading the plugin may not enable or configure it.  See the plugin's
:ref:`regex_policy_configuration` and :ref:`regex_policy_variables`.

.. seealso:: :doc:`/options` for more information about adding and removing plugins.

.. _regex_policy_configuration:

Configuration
-------------

These command line options configure the plugin when :program:`drizzled`
is started.  See :doc:`/configuration` for more information about specifying
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

For example::

   # This is a comment line and should be skipped
   .+ schema=DATA_DICTIONARY ACCEPT
   .+ schema=INFORMATION_SCHEMA ACCEPT
   .+ schema=data_dictionary ACCEPT
   .+ schema=information_schema ACCEPT
   root table=.+ ACCEPT
   root schema=.+ ACCEPT
   root process=.+ ACCEPT
   user1 schema=user1 ACCEPT
   user2 schema=user2 ACCEPT
   user1 process=user1 ACCEPT
   user2 process=user2 ACCEPT
   # Default to denying everything
   .+ schema=.+ DENY
   .+ process=.+ DENY

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

This documentation applies to **regex_policy 1.0**.

To see which version of the plugin a Drizzle server is running, execute:

.. code-block:: mysql

   SELECT MODULE_VERSION FROM DATA_DICTIONARY.MODULES WHERE MODULE_NAME='regex_policy'

Changelog
---------

v1.0
^^^^
* First release.
