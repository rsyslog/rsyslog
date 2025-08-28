.. _param-omprog-hup-signal:
.. _omprog.parameter.action.hup-signal:

hup.signal
==========

.. index::
   single: omprog; hup.signal
   single: hup.signal

.. summary-start

Forwards a chosen signal to the program when rsyslog receives HUP.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/omprog`.

:Name: hup.signal
:Scope: action
:Type: word
:Default: action=none
:Required?: no
:Introduced: 8.9.0

Description
-----------
Specifies which signal, if any, is to be forwarded to the external program
when rsyslog receives a HUP signal. Currently, HUP, USR1, USR2, INT, and
TERM are supported. If unset, no signal is sent on HUP. This is the default
and what pre 8.9.0 versions did.

Action usage
------------
.. _param-omprog-action-hup-signal:
.. _omprog.parameter.action.hup-signal-usage:

.. code-block:: rsyslog

   action(type="omprog" hup.signal="HUP")

See also
--------
See also :doc:`../../configuration/modules/omprog`.
