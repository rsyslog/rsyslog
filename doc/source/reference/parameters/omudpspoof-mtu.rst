.. _param-omudpspoof-mtu:
.. _omudpspoof.parameter.action.mtu:

mtu
===

.. index::
   single: omudpspoof; mtu
   single: mtu

.. summary-start

Sets the maximum packet length that omudpspoof sends.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/omudpspoof`.

:Name: mtu
:Scope: action
:Type: integer
:Default: 1500
:Required?: no
:Introduced: 5.1.3

Description
-----------
Maximum packet length to send.

Action usage
------------
.. _param-omudpspoof-action-mtu:
.. _omudpspoof.parameter.action.mtu-usage:

.. code-block:: rsyslog

   action(type="omudpspoof" target="192.0.2.1" mtu="1400")

See also
--------
See also :doc:`../../configuration/modules/omudpspoof`.
