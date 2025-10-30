.. _param-imrelp-keepalive:
.. _imrelp.parameter.input.keepalive:

keepAlive
=========

.. index::
   single: imrelp; keepAlive
   single: keepAlive

.. summary-start

Toggles TCP keep-alive probes for RELP listener sockets.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/imrelp`.

:Name: keepAlive
:Scope: input
:Type: boolean
:Default: input=off
:Required?: no
:Introduced: at least 8.2108.0, possibly earlier

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
