.. _param-omudpspoof-target:
.. _omudpspoof.parameter.module.target:

Target
======

.. index::
   single: omudpspoof; Target
   single: Target

.. summary-start
Sets the destination host to which omudpspoof sends messages.
.. summary-end

This parameter applies to :doc:`../../configuration/modules/omudpspoof`.

:Name: Target
:Scope: module
:Type: word
:Default: none
:Required?: yes
:Introduced: 5.1.3

Description
-----------
Host that the messages shall be sent to.

Module usage
------------
.. _param-omudpspoof-module-target:
.. _omudpspoof.parameter.module.target-usage:

.. code-block:: rsyslog

   action(type="omudpspoof" target="192.0.2.1")

Legacy names (for reference)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~
Historic names/directives for compatibility. Do not use in new configs.

.. _omudpspoof.parameter.legacy.actionomudpspooftargethost:
- $ActionOMUDPSpoofTargetHost — maps to Target (status: legacy)

.. index::
   single: omudpspoof; $ActionOMUDPSpoofTargetHost
   single: $ActionOMUDPSpoofTargetHost

See also
--------
See also :doc:`../../configuration/modules/omudpspoof`.
