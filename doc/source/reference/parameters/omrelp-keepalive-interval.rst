.. _param-omrelp-keepalive-interval:
.. _omrelp.parameter.input.keepalive-interval:

KeepAlive.Interval
==================

.. index::
   single: omrelp; KeepAlive.Interval
   single: KeepAlive.Interval

.. summary-start

Configures the time between successive TCP keepalive probes.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/omrelp`.

:Name: KeepAlive.Interval
:Scope: input
:Type: integer
:Default: input=0
:Required?: no
:Introduced: at least 7.0.0, possibly earlier

Description
-----------
The interval between subsequent keepalive probes. The default, 0, uses the operating system defaults. This only has an effect if keep-alive is enabled and may not be available on all platforms.

Input usage
-----------
.. _param-omrelp-input-keepalive-interval:
.. _omrelp.parameter.input.keepalive-interval-usage:

.. code-block:: rsyslog

   action(type="omrelp" target="centralserv" keepAlive.interval="30")

See also
--------
See also :doc:`../../configuration/modules/omrelp`.
