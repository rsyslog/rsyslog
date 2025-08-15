.. _param-imtcp-keepalive:
.. _imtcp.parameter.module.keepalive:

KeepAlive
=========

.. index::
   single: imtcp; KeepAlive
   single: KeepAlive

.. summary-start

Enables TCP keep-alive packets on connections.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/imtcp`.

:Name: KeepAlive
:Scope: module
:Type: boolean
:Default: module=off
:Required?: no
:Introduced: at least 5.x, possibly earlier

Description
-----------
Enable or disable keep-alive packets at the tcp socket layer. The
default is to disable them.

The same-named input parameter can override this module setting.


Module usage
------------
.. _param-imtcp-module-keepalive:
.. _imtcp.parameter.module.keepalive-usage:

.. code-block:: rsyslog

   module(load="imtcp" keepAlive="on")

Legacy names (for reference)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~
Historic names/directives for compatibility. Do not use in new configs.

.. _imtcp.parameter.legacy.inputtcpserverkeepalive:

- $InputTCPServerKeepAlive — maps to KeepAlive (status: legacy)

.. index::
   single: imtcp; $InputTCPServerKeepAlive
   single: $InputTCPServerKeepAlive

See also
--------
See also :doc:`../../configuration/modules/imtcp`.

