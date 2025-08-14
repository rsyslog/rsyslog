.. _param-imtcp-keepalive-interval:
.. _imtcp.parameter.module.keepalive-interval:
.. _imtcp.parameter.input.keepalive-interval:

KeepAlive.Interval
==================

.. index::
   single: imtcp; KeepAlive.Interval
   single: KeepAlive.Interval

.. summary-start

Sets the time interval between subsequent keep-alive probes.

.. summary-end

This parameter can be set at both the module and input levels.

:Name: KeepAlive.Interval
:Scope: module, input
:Type: integer
:Default: ``0`` (module), ``module parameter`` (input)
:Required?: no
:Introduced: 8.2106.0 (module), 8.2106.0 (input)

Description
-----------
This parameter defines the interval (in seconds) for subsequent keep-alive packets. A value of ``0`` (the default) means that the operating system's default setting is used.

This parameter only has an effect if TCP keep-alive is enabled via the ``keepAlive`` parameter. The functionality may not be available on all platforms.

This can be set at the module level to define a global default for all ``imtcp`` listeners. It can also be overridden on a per-input basis.

Module usage
------------
.. _imtcp.parameter.module.keepalive-interval-usage:

.. code-block:: rsyslog

   module(load="imtcp" keepAlive="on" keepAlive.interval="15")

Input usage
-----------
.. _imtcp.parameter.input.keepalive-interval-usage:

.. code-block:: rsyslog

   input(type="imtcp" port="514" keepAlive="on" keepAlive.interval="30")

Legacy names (for reference)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~
Historic names/directives for compatibility. Do not use in new configs.

This parameter has no legacy name.

See also
--------
See also :doc:`../../configuration/modules/imtcp`.
