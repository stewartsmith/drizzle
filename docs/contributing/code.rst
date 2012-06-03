.. _contributing_code:

Contributing Code
=================

We recommend that anyone new to Drizzle who wants to learn the code shoult
see the
`Low hanging fruit <https://bugs.launchpad.net/drizzle/+bugs?field.tag=low-hanging-fruit>`_
list of bugs on Launchpad for possible tasks to take on.

SSH Keys
--------

You will need to add a public SSH key to Launchpad to be able to push branches
up for merging.  To do this:

 #. Generate an SSH key (information on how to do this is in `Launchpad's help pages <https://help.launchpad.net/YourAccount/CreatingAnSSHKeyPair>`_)
 #. Go to `your SSH keys page <https://launchpad.net/people/+me/+editsshkeys>`_
 #. Paste your public key into the text box and click the *Import public key*
    button

Logging into Launchpad
----------------------

You will need to set up your local bzr configuration with your Launchpad account
so that you can push branches for merging.  To do this:

 # bzr launchpad-login username

Getting the Code
----------------

You will need to prepare a directory for working with Drizzle. The following
example will assume you want to do this in ~/repos/drizzle:

.. code-block:: bash

   mkdir ~/repos
   cd ~/repos
   bzr init-repo drizzle

At this stage you now have an initialized a directory named ~/repos/drizzle
for bzr to use.  Revision data will be stored in this directory so that it
does not have to be separately downloaded and stored for every branch.

.. code-block:: bash

   cd drizzle
   bzr branch lp:drizzle trunk

This will take a little while, it is getting all the revisions since the
beginning of the repository.  This should only ever need to be done once though.

.. code-block:: bash

   bzr branch trunk drizzle-bug-NNNNNN
   cd drizzle-bug-NNNNNN

This creates a branch from trunk for you to work on.

Committing Work
---------------

All commits need to have a proper description of the changes made.  This is so
that future developers can read through the bzr log to find out why a certain
change happened.

When committing a bug fix please use:

.. code-block:: bash

   bzr commit --fixes lp:NNNNNN

This will automatically attach the branch to the bug report when the branch is
pushed up.

Coding Standards
----------------

Our coding standards can be found at the
`Drizzle wiki <http://wiki.drizzle.org/Coding_Standards>`_.

Test Cases
----------

.. program:: test-run

Where possible, SQL test cases should be created for your code.  Our test cases
for bug fixes should be in the ``tests/suite/regression`` directory.  For more
information about creating test cases please see the :ref:`test run <test-run-label>`
section of our documentation.

If writing a plugin, the other plugins in trunk have many examples of test cases.

You should attempt to test your code on multiple platforms before publishing
it or proposing it for merging.  Your tests must pass on all
:ref:`supported_platforms` and be idempotent (pass when ran with
:option:`test-run --repeat-test` 2).  You can test on other platforms by
using `VirtualBox <https://www.virtualbox.org/>`_ or another virtualization
server, or by requesting access to the `Drizzle Jenkins server
<http://jenkins.drizzle.org/>`_.

Pushing Work
------------

When you want to push your branch to Launchpad for others to see or for merging
you can use:

.. code-block:: bash

   bzr push lp:~username/drizzle/trunk-bug-NNNNNN

This will create the branch on Launchpad which you will be able to see on the
`code page <https://code.launchpad.net/drizzle>`_.  If you commit more changes
to your branch, just ``bzr push`` to publish the new changes.

Merge Proposals
---------------

Once your work is done and ready for review, go to
`code page <https://code.launchpad.net/drizzle>`_, then click on your branch.
You will see a link labeled *Propose for merging*.

In this screen simply fill in what this branch does and click the
*Propose Merge* button.  Members of the
`Drizzle merge team <https://launchpad.net/~drizzle-merge>`_
will review the branch, usually within a few days.
If approved, it will go though our rigourous testing process, which can
take several hours.  If it needs more work, feedback will always be given to
explain why.
