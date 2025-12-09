.. _param-omrelp-keepalive:
.. _omrelp.parameter.input.keepalive:

KeepAlive
=========

.. index::
   single: omrelp; KeepAlive
   single: KeepAlive

.. summary-start

Enables or disables TCP keep-alive packets for the RELP connection.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/omrelp`.

:Name: KeepAlive
:Scope: input
:Type: boolean
:Default: input=off
:Required?: no
:Introduced: at least 7.0.0, possibly earlier

Description
-----------
Enable or disable keep-alive packets at the TCP socket layer. By default keep-alive is disabled.

Input usage
-----------
.. _param-omrelp-input-keepalive:
.. _omrelp.parameter.input.keepalive-usage:

.. code-block:: rsyslog

   action(type="omrelp" target="centralserv" keepAlive="on")

See also
--------
See also :doc:`../../configuration/modules/omrelp`.
