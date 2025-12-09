.. _param-omrelp-port:
.. _omrelp.parameter.input.port:

Port
====

.. index::
   single: omrelp; Port
   single: Port

.. summary-start

Specifies the TCP port number or service name for the RELP connection.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/omrelp`.

:Name: Port
:Scope: input
:Type: string
:Default: input=514
:Required?: no
:Introduced: at least 7.0.0, possibly earlier

Description
-----------
Name or numerical value of TCP port to use when connecting to target.

Input usage
-----------
.. _param-omrelp-input-port:
.. _omrelp.parameter.input.port-usage:

.. code-block:: rsyslog

   action(type="omrelp" target="centralserv" port="2514")

See also
--------
See also :doc:`../../configuration/modules/omrelp`.
