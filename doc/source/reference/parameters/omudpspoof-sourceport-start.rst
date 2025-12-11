.. _param-omudpspoof-sourceport-start:
.. _omudpspoof.parameter.module.sourceport-start:

SourcePort.start
================

.. index::
   single: omudpspoof; SourcePort.start
   single: SourcePort.start

.. summary-start
Sets the starting source port when cycling through spoofed source ports.
.. summary-end

This parameter applies to :doc:`../../configuration/modules/omudpspoof`.

:Name: SourcePort.start
:Scope: module
:Type: integer
:Default: 32000
:Required?: no
:Introduced: 5.1.3

Description
-----------
Specify the start value for circling the source ports. Start must be less than or equal to sourcePort.End.

Module usage
------------
.. _param-omudpspoof-module-sourceport-start:
.. _omudpspoof.parameter.module.sourceport-start-usage:

.. code-block:: rsyslog

   action(type="omudpspoof" target="192.0.2.1" sourcePort.start="10000" sourcePort.end="19999")

Legacy names (for reference)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~
Historic names/directives for compatibility. Do not use in new configs.

.. _omudpspoof.parameter.legacy.actionomudpspoofsourceportstart:
- $ActionOMUDPSpoofSourcePortStart — maps to SourcePort.start (status: legacy)

.. index::
   single: omudpspoof; $ActionOMUDPSpoofSourcePortStart
   single: $ActionOMUDPSpoofSourcePortStart

See also
--------
See also :doc:`../../configuration/modules/omudpspoof`.
