.. _param-omfile-addlf:
.. _omfile.parameter.module.addlf:

addLF
=====

.. index::
   single: omfile; addLF
   single: addLF

.. summary-start

Appends an LF if a rendered message does not already end with one, ensuring
full-line records.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/omfile`.

:Name: addLF
:Scope: module, action
:Type: boolean
:Default: module=on; action=inherits module
:Required?: no
:Introduced: 8.2510.0

Description
-----------

When enabled, the omfile action checks whether a rendered message already ends
with an LF (line feed). If not, omfile appends one before writing, so that every
record is a complete line regardless of the template used.

In text-based logging, a line feed (LF) marks the end of a record. Without it,
the record may appear as an incomplete line when viewed with standard tools
such as ``cat`` or ``tail -f``. The extra byte is added transparently even when
compression or cryptographic providers are active.

If set at the module level, the value becomes the default for all omfile actions
that do not explicitly define ``addLF``.

Notes on default behavior
-------------------------

Since **8.2510.0**, the default has been changed to ``on``.
In earlier releases, messages without a trailing LF were written as incomplete
lines. With the new default, rsyslog ensures that all records end with a line
feed.

This change aligns with common user expectations—especially when writing JSON
logs, where line-separated records are the norm.
If incomplete lines are desired, you can disable the feature either globally at
the module level or for individual actions.

Module usage
------------

.. _param-omfile-module-addlf:
.. _omfile.parameter.module.addlf-usage:

To override the default globally and allow incomplete lines, disable ``addLF``
at the module level:

.. code-block:: rsyslog

   module(load="builtin:omfile" addLF="off")

Action usage
------------

.. _param-omfile-action-addlf:
.. _omfile.parameter.action.addlf:

By default, actions inherit the module setting. No explicit parameter is needed:

.. code-block:: rsyslog

   action(type="omfile" file="/var/log/messages")

In rare cases, you may want to allow incomplete lines only for one action. You
can disable ``addLF`` explicitly at the action level:

.. code-block:: rsyslog

   action(type="omfile" file="/var/log/raw.log" addLF="off")

See also
--------

- :doc:`../../configuration/modules/omfile`

.. meta::
   :keywords: rsyslog, omfile, addLF, line termination, json logging, full line records
