.. _param-omrelp-keepalive-probes:
.. _omrelp.parameter.input.keepalive-probes:

KeepAlive.Probes
================

.. index::
   single: omrelp; KeepAlive.Probes
   single: KeepAlive.Probes

.. summary-start

Sets how many keepalive probes are sent before a connection is deemed dead.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/omrelp`.

:Name: KeepAlive.Probes
:Scope: input
:Type: integer
:Default: input=0
:Required?: no
:Introduced: at least 7.0.0, possibly earlier

Description
-----------
The number of keepalive probes to send before considering the connection dead. The default, 0, uses the operating system defaults. This only has an effect if keep-alive is enabled and may not be available on all platforms.

Input usage
-----------
.. _param-omrelp-input-keepalive-probes:
.. _omrelp.parameter.input.keepalive-probes-usage:

.. code-block:: rsyslog

   action(type="omrelp" target="centralserv" keepAliveProbes="5")

See also
--------
See also :doc:`../../configuration/modules/omrelp`.
