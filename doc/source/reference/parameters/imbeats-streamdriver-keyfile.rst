.. _param-imbeats-streamdriver-keyfile:
.. _imbeats.parameter.input.streamdriver-keyfile:

StreamDriver.KeyFile
====================

.. meta::
   :description: Private key file for imbeats TLS listeners.
   :keywords: rsyslog, imbeats, streamdriver keyfile

.. index::
   single: imbeats; StreamDriver.KeyFile
   single: StreamDriver.KeyFile

.. summary-start

Specify the private key file used by TLS-enabled imbeats listeners.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/imbeats`.

:Name: StreamDriver.KeyFile
:Scope: input
:Type: string
:Default: input=none
:Required?: no
:Introduced: 8.2604.0

Description
-----------
Specify the private key file used by TLS-enabled imbeats listeners.

Input usage
-----------
.. _param-imbeats-input-streamdriver-keyfile:
.. _imbeats.parameter.input.streamdriver-keyfile-usage:

.. code-block:: rsyslog

   input(type="imbeats" port="5044" streamDriver.keyFile="/etc/rsyslog/key.pem")

See also
--------
See also :doc:`../../configuration/modules/imbeats`.
