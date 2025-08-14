.. _param-imtcp-maxframesize:
.. _imtcp.parameter.module.maxframesize:
.. _imtcp.parameter.input.maxframesize:

MaxFrameSize
============

.. index::
   single: imtcp; MaxFrameSize
   single: MaxFrameSize

.. summary-start

Specifies the maximum allowed size for a syslog message frame.

.. summary-end

This parameter can be set at both the module and input levels.

:Name: MaxFrameSize
:Scope: module, input
:Type: integer
:Default: ``200000`` (module), ``module parameter`` (input)
:Required?: no
:Introduced: at least 5.x, possibly earlier (module), 8.2106.0 (input)

Description
-----------
When using octet-counted framing, the frame size is given at the beginning of the message. This parameter specifies the maximum size this frame can have. If a frame is received that is larger than the configured max size, rsyslog will switch to octet-stuffing mode for that message. The maximum value for this parameter is 200,000,000 to prevent integer overflows.

This can be set at the module level to define a global default for all ``imtcp`` listeners. It can also be overridden on a per-input basis.

Module usage
------------
.. _imtcp.parameter.module.maxframesize-usage:

.. code-block:: rsyslog

   module(load="imtcp" maxFrameSize="204800")

Input usage
-----------
.. _imtcp.parameter.input.maxframesize-usage:

.. code-block:: rsyslog

   input(type="imtcp" port="514" maxFrameSize="10240")

Legacy names (for reference)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~
Historic names/directives for compatibility. Do not use in new configs.

This parameter has no legacy name.

See also
--------
See also :doc:`../../configuration/modules/imtcp`.
