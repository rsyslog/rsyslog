.. _param-imfile-mode:
.. _imfile.parameter.module.mode:

Mode
====

.. index::
   single: imfile; Mode
   single: Mode

.. summary-start

Specifies if imfile shall run in inotify ("inotify") or polling ("polling") mode.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/imfile`.

:Name: Mode
:Scope: module
:Type: word
:Default: module=inotify
:Required?: no
:Introduced: 8.1.5

Description
-----------
.. versionadded:: 8.1.5

This specifies if imfile is shall run in inotify ("inotify") or polling
("polling") mode. Traditionally, imfile used polling mode, which is
much more resource-intense (and slower) than inotify mode. It is
suggested that users turn on "polling" mode only if they experience
strange problems in inotify mode. In theory, there should never be a
reason to enable "polling" mode and later versions will most probably
remove it.

Note: if a legacy "$ModLoad" statement is used, the default is *polling*.
This default was kept to prevent problems with old configurations. It
might change in the future.

.. versionadded:: 8.32.0

On Solaris, the FEN API is used instead of INOTIFY. You can set the mode
to fen or inotify (which is automatically mapped to fen on Solaris OS).
Please note that the FEN is limited compared to INOTIFY. Deep wildcard
matches may not work because of the API limits for now.

Module usage
------------
.. _param-imfile-module-mode:
.. _imfile.parameter.module.mode-usage:
.. code-block:: rsyslog

   module(load="imfile" Mode="...")

See also
--------
See also :doc:`../../configuration/modules/imfile`.
