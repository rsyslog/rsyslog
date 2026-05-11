.. _param-imbeats-streamdriver-cafile:
.. _imbeats.parameter.input.streamdriver-cafile:

StreamDriver.CAFile
===================

.. meta::
   :description: CA file for imbeats TLS validation.
   :keywords: rsyslog, imbeats, streamdriver cafile

.. index::
   single: imbeats; StreamDriver.CAFile
   single: StreamDriver.CAFile

.. summary-start

Specify the CA bundle used to validate TLS peers for imbeats.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/imbeats`.

:Name: StreamDriver.CAFile
:Scope: input
:Type: string
:Default: input=none
:Required?: no
:Introduced: 8.2604.0

Description
-----------
Specify the CA bundle used to validate TLS peers for imbeats.

Input usage
-----------
.. _param-imbeats-input-streamdriver-cafile:
.. _imbeats.parameter.input.streamdriver-cafile-usage:

.. code-block:: rsyslog

   input(type="imbeats" port="5044" streamDriver.caFile="/etc/rsyslog/ca.pem")

See also
--------
See also :doc:`../../configuration/modules/imbeats`.
