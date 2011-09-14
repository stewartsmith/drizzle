PAM Authenication
=================

:program:`auth_pam` is an authentication plugin that authentication connections
using :abbr:`PAM (Pluggable Authentication Module)`.
PAM is effectively your current Linux based user security. [1]_ 

.. note:: Unload the :doc:`/plugins/auth_all/index` plugin before using this plugin.
.. seealso:: :doc:`/administration/authentication` 

.. _auth_pam_loading:

Loading
-------

To load this plugin, start :program:`drizzled` with::

   --plugin-add=auth_pam

Loading the plugin may not enable or configure it.  See the plugin's
:ref:`auth_pam_configuration` and :ref:`auth_pam_variables`.

.. seealso:: :doc:`/options` for more information about adding and removing plugins.

.. _auth_pam_configuration:

Configuration
-------------

This plugin does not have any command line options.

.. _auth_pam_variables:

Variables
---------

This plugin does not register any variables.

.. _auth_pam_examples:

Examples
--------

Sorry, there are no examples for this plugin.

.. _auth_pam_authors:

Authors
-------

Brian Aker

.. _auth_pam_version:

Version
-------

This documentation applies to **auth_pam 0.1**.

To see which version of the plugin a Drizzle server is running, execute:

.. code-block:: mysql

   SELECT MODULE_VERSION FROM DATA_DICTIONARY.MODULES WHERE MODULE_NAME='auth_pam'

Changelog
---------

v0.1
^^^^
* First release.

-------------------------------------------------------------------------------

.. rubric:: Footnotes

.. [1] `Understanding Drizzle user authentication options – Part 1 <http://ronaldbradford.com/blog/understanding-drizzle-authentication-options-part-1-2010-03-12/>`_

