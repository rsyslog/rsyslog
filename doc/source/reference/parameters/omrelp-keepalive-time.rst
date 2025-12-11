.. _param-omrelp-keepalive-time:
.. _omrelp.parameter.input.keepalive-time:

KeepAlive.Time
==============

.. index::
   single: omrelp; KeepAlive.Time
   single: KeepAlive.Time

.. summary-start

Specifies the idle time before the first TCP keepalive probe is sent.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/omrelp`.

:Name: KeepAlive.Time
:Scope: input
:Type: integer
:Default: input=0
:Required?: no
:Introduced: at least 7.0.0, possibly earlier

Description
-----------
The idle time before the first keepalive probe is sent. The default, 0, uses the operating system defaults. This only has an effect if keep-alive is enabled and may not be available on all platforms.

Input usage
-----------
.. _param-omrelp-input-keepalive-time:
.. _omrelp.parameter.input.keepalive-time-usage:

.. code-block:: rsyslog

   action(type="omrelp" target="centralserv" keepAliveTime="300")

See also
--------
See also :doc:`../../configuration/modules/omrelp`.
