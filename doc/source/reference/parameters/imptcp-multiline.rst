.. _param-imptcp-multiline:
.. _imptcp.parameter.input.multiline:

MultiLine
=========

.. index::
   single: imptcp; MultiLine
   single: MultiLine

.. summary-start

Detects a new message only when LF is followed by '<' or end of input.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/imptcp`.

:Name: MultiLine
:Scope: input
:Type: boolean
:Default: input=off
:Required?: no
:Introduced: at least 5.x, possibly earlier

Description
-----------
Experimental parameter which causes rsyslog to recognize a new message
only if the line feed is followed by a '<' or if there are no more characters.

Input usage
-----------
.. _param-imptcp-input-multiline:
.. _imptcp.parameter.input.multiline-usage:

.. code-block:: rsyslog

   input(type="imptcp" multiLine="...")

See also
--------
See also :doc:`../../configuration/modules/imptcp`.
