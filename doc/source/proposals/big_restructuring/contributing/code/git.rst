Git
===

This document explains some conventions and specificities in the way we manage
the Rsyslog code with Git.

Pull Requests
-------------

Whenever a pull request is merged, all the information contained in the pull
request (including comments) is saved in the repository.

You can easily spot pull request merges as the commit message always follows
this pattern:

.. code-block:: text

    merged branch USER_NAME/BRANCH_NAME (PR #1111)

The PR reference allows you to have a look at the original pull request on
GitHub: https://github.com/rsyslog/rsyslog/pull/1111. But all the information
you can get on GitHub is also available from the repository itself.

The merge commit message contains the original message from the author of the
changes. Often, this can help understand what the changes were about and the
reasoning behind the changes.

Moreover, the full discussion that might have occurred back then is also
stored as a Git note. To get access to these notes, add this line to
your ``.git/config`` file:

.. code-block:: ini

    fetch = +refs/notes/*:refs/notes/*

After a fetch, getting the GitHub discussion for a commit is then a matter of
adding ``--show-notes=github-comments`` to the ``git show`` command:

.. code-block:: bash

    $ git show HEAD --show-notes=github-comments
