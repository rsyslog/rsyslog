.. _param-omudpspoof-target:
.. _omudpspoof.parameter.action.target:

target
======

.. index::
   single: omudpspoof; target
   single: target

.. summary-start

Sets the destination host to which omudpspoof sends messages.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/omudpspoof`.

:Name: target
:Scope: action
:Type: word
:Default: none
:Required?: yes
:Introduced: 5.1.3

Description
-----------
Host that the messages shall be sent to.

Action usage
------------
.. _param-omudpspoof-action-target:
.. _omudpspoof.parameter.action.target-usage:

.. code-block:: rsyslog

   action(type="omudpspoof" target="192.0.2.1")

Legacy names (for reference)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~
Historic names/directives for compatibility. Do not use in new configs.

.. _omudpspoof.parameter.legacy.actionomudpspooftargethost:

- $ActionOMUDPSpoofTargetHost — maps to target (status: legacy)

.. index::
   single: omudpspoof; $ActionOMUDPSpoofTargetHost
   single: $ActionOMUDPSpoofTargetHost

See also
--------
See also :doc:`../../configuration/modules/omudpspoof`.
