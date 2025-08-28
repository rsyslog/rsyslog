.. _param-imptcp-failonchownfailure:
.. _imptcp.parameter.input.failonchownfailure:

FailOnChOwnFailure
==================

.. index::
   single: imptcp; FailOnChOwnFailure
   single: FailOnChOwnFailure

.. summary-start

Controls startup failure if changing socket owner, group, or mode fails.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/imptcp`.

:Name: FailOnChOwnFailure
:Scope: input
:Type: boolean
:Default: input=on
:Required?: no
:Introduced: at least 5.x, possibly earlier

Description
-----------
Rsyslog will not start if this is on and changing the file owner, group,
or access permissions fails. Disable this to ignore these errors.

Input usage
-----------
.. _param-imptcp-input-failonchownfailure:
.. _imptcp.parameter.input.failonchownfailure-usage:

.. code-block:: rsyslog

   input(type="imptcp" failOnChOwnFailure="...")

See also
--------
See also :doc:`../../configuration/modules/imptcp`.
