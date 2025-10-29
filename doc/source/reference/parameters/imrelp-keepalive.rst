.. _param-imrelp-keepalive:
.. _imrelp.parameter.input.keepalive:

KeepAlive
=========

.. index::
   single: imrelp; KeepAlive
   single: KeepAlive

.. summary-start

Toggles TCP keep-alive probes for RELP listener sockets.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/imrelp`.

:Name: KeepAlive
:Scope: input
:Type: boolean
:Default: input=off
:Required?: no
:Introduced: Not documented

Description
-----------
Enable or disable keep-alive packets at the TCP socket layer. By default
keep-alive is disabled.

Input usage
-----------
.. _param-imrelp-input-keepalive:
.. _imrelp.parameter.input.keepalive-usage:

.. code-block:: rsyslog

   input(type="imrelp" port="2514" keepAlive="on")

See also
--------
See also :doc:`../../configuration/modules/imrelp`.
