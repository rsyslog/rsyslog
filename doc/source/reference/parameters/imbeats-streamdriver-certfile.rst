.. _param-imbeats-streamdriver-certfile:
.. _imbeats.parameter.input.streamdriver-certfile:

StreamDriver.CertFile
=====================

.. meta::
   :description: Certificate file for imbeats TLS listeners.
   :keywords: rsyslog, imbeats, streamdriver certfile

.. index::
   single: imbeats; StreamDriver.CertFile
   single: StreamDriver.CertFile

.. summary-start

Specify the local certificate presented by TLS-enabled imbeats listeners.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/imbeats`.

:Name: StreamDriver.CertFile
:Scope: input
:Type: string
:Default: input=none
:Required?: no
:Introduced: 8.2604.0

Description
-----------
Specify the local certificate presented by TLS-enabled imbeats listeners.

Input usage
-----------
.. _param-imbeats-input-streamdriver-certfile:
.. _imbeats.parameter.input.streamdriver-certfile-usage:

.. code-block:: rsyslog

   input(type="imbeats" port="5044" streamDriver.certFile="/etc/rsyslog/cert.pem")

See also
--------
See also :doc:`../../configuration/modules/imbeats`.
