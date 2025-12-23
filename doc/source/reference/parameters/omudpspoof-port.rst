.. _param-omudpspoof-port:
.. _omudpspoof.parameter.action.port:

port
====

.. index::
   single: omudpspoof; port
   single: port

.. summary-start

Specifies the destination port used when sending messages.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/omudpspoof`.

:Name: port
:Scope: action
:Type: word
:Default: 514
:Required?: no
:Introduced: 5.1.3

Description
-----------
Remote port that the messages shall be sent to. Default is 514.

Action usage
------------
.. _param-omudpspoof-action-port:
.. _omudpspoof.parameter.action.port-usage:

.. code-block:: rsyslog

   action(type="omudpspoof" target="192.0.2.1" port="10514")

Legacy names (for reference)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~
Historic names/directives for compatibility. Do not use in new configs.

.. _omudpspoof.parameter.legacy.actionomudpspooftargetport:

- $ActionOMUDPSpoofTargetPort — maps to port (status: legacy)

.. index::
   single: omudpspoof; $ActionOMUDPSpoofTargetPort
   single: $ActionOMUDPSpoofTargetPort

See also
--------
See also :doc:`../../configuration/modules/omudpspoof`.
