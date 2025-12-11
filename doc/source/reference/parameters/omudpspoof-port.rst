.. _param-omudpspoof-port:
.. _omudpspoof.parameter.module.port:

Port
====

.. index::
   single: omudpspoof; Port
   single: Port

.. summary-start
Specifies the destination port used when sending messages.
.. summary-end

This parameter applies to :doc:`../../configuration/modules/omudpspoof`.

:Name: Port
:Scope: module
:Type: word
:Default: 514
:Required?: no
:Introduced: 5.1.3

Description
-----------
Remote port that the messages shall be sent to. Default is 514.

Module usage
------------
.. _param-omudpspoof-module-port:
.. _omudpspoof.parameter.module.port-usage:

.. code-block:: rsyslog

   action(type="omudpspoof" target="192.0.2.1" port="10514")

Legacy names (for reference)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~
Historic names/directives for compatibility. Do not use in new configs.

.. _omudpspoof.parameter.legacy.actionomudpspooftargetport:
- $ActionOMUDPSpoofTargetPort — maps to Port (status: legacy)

.. index::
   single: omudpspoof; $ActionOMUDPSpoofTargetPort
   single: $ActionOMUDPSpoofTargetPort

See also
--------
See also :doc:`../../configuration/modules/omudpspoof`.
