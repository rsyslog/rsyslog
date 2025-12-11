.. _param-omudpspoof-sourceport-end:
.. _omudpspoof.parameter.action.sourceport-end:

sourcePort.end
==============

.. index::
   single: omudpspoof; sourcePort.end
   single: sourcePort.end

.. summary-start
Sets the ending source port when cycling through spoofed source ports.
.. summary-end

This parameter applies to :doc:`../../configuration/modules/omudpspoof`.

:Name: sourcePort.end
:Scope: action
:Type: integer
:Default: 42000
:Required?: no
:Introduced: 5.1.3

Description
-----------
Specify the end value for circling the source ports. End must be equal to or more than sourcePort.start.

Action usage
------------
.. _param-omudpspoof-action-sourceport-end:
.. _omudpspoof.parameter.action.sourceport-end-usage:

.. code-block:: rsyslog

   action(type="omudpspoof" target="192.0.2.1" sourcePort.start="10000" sourcePort.end="19999")

Legacy names (for reference)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~
Historic names/directives for compatibility. Do not use in new configs.

.. _omudpspoof.parameter.legacy.actionomudpspoofsourceportend:
- $ActionOMUDPSpoofSourcePortEnd — maps to sourcePort.end (status: legacy)

.. index::
   single: omudpspoof; $ActionOMUDPSpoofSourcePortEnd
   single: $ActionOMUDPSpoofSourcePortEnd

See also
--------
See also :doc:`../../configuration/modules/omudpspoof`.
