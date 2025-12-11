.. _param-omudpspoof-mtu:
.. _omudpspoof.parameter.module.mtu:

MTU
===

.. index::
   single: omudpspoof; MTU
   single: MTU

.. summary-start
Sets the maximum packet length that omudpspoof sends.
.. summary-end

This parameter applies to :doc:`../../configuration/modules/omudpspoof`.

:Name: MTU
:Scope: module
:Type: integer
:Default: 1500
:Required?: no
:Introduced: 5.1.3

Description
-----------
Maximum packet length to send.

Module usage
------------
.. _param-omudpspoof-module-mtu:
.. _omudpspoof.parameter.module.mtu-usage:

.. code-block:: rsyslog

   action(type="omudpspoof" target="192.0.2.1" mtu="1400")

See also
--------
See also :doc:`../../configuration/modules/omudpspoof`.
