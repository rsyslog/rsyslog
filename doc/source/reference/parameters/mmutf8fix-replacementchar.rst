.. _param-mmutf8fix-replacementchar:
.. _mmutf8fix.parameter.action.replacementchar:

replacementChar
===============

.. index::
   single: mmutf8fix; replacementChar
   single: replacementChar

.. summary-start

Defines the printable character used to substitute invalid sequences.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/mmutf8fix`.

:Name: replacementChar
:Scope: action
:Type: string
:Default: action=" "
:Required?: no
:Introduced: 7.5.4

Description
-----------
This is the character that invalid sequences are replaced by. It must be a printable US-ASCII character.

Action usage
------------
.. _param-mmutf8fix-action-replacementchar:
.. _mmutf8fix.parameter.action.replacementchar-usage:

.. code-block:: rsyslog

   module(load="mmutf8fix")

   action(type="mmutf8fix" replacementChar="#")

See also
--------
See also :doc:`../../configuration/modules/mmutf8fix`.
