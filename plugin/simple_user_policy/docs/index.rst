User-based Authorization
========================

Authorization plugin which implements a simple policy-based strategy.

.. _simple_user_policy_loading:

Loading
-------

To load this plugin, start :program:`drizzled` with::

   --plugin-add=simple_user_policy

Loading the plugin may not enable or configure it.  See the plugin's
:ref:`simple_user_policy_configuration` and :ref:`simple_user_policy_variables`.

.. seealso:: :doc:`/options` for more information about adding and removing plugins.

.. _simple_user_policy_configuration:

Configuration
-------------

These command line options configure the plugin when :program:`drizzled`
is started.  See :doc:`/configuration` for more information about specifying
command line options.

.. program:: drizzled

.. _simple_user_policy_variables:

Variables
---------

These variables show the running configuration of the plugin.
See `variables` for more information about querying and setting variables.

.. _simple_user_policy_examples:

Examples
--------

Sorry, there are no examples for this plugin.

.. _simple_user_policy_authors:

Authors
-------

Monty Taylor

.. _simple_user_policy_version:

Version
-------

This documentation applies to **simple_user_policy 1.0**.

To see which version of the plugin a Drizzle server is running, execute:

.. code-block:: mysql

   SELECT MODULE_VERSION FROM DATA_DICTIONARY.MODULES WHERE MODULE_NAME='simple_user_policy'

