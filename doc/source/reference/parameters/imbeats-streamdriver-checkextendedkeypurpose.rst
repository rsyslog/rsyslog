.. _param-imbeats-streamdriver-checkextendedkeypurpose:
.. _imbeats.parameter.input.streamdriver-checkextendedkeypurpose:

StreamDriver.CheckExtendedKeyPurpose
====================================

.. meta::
   :description: Extended key usage enforcement for imbeats TLS peers.
   :keywords: rsyslog, imbeats, extended key usage

.. index::
   single: imbeats; StreamDriver.CheckExtendedKeyPurpose
   single: StreamDriver.CheckExtendedKeyPurpose

.. summary-start

Enable extended key usage checks when validating imbeats TLS certificates.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/imbeats`.

:Name: StreamDriver.CheckExtendedKeyPurpose
:Scope: input
:Type: boolean
:Default: input=off
:Required?: no
:Introduced: 8.2604.0

Description
-----------
Enable extended key usage checks when validating imbeats TLS certificates.

Input usage
-----------
.. _param-imbeats-input-streamdriver-checkextendedkeypurpose:
.. _imbeats.parameter.input.streamdriver-checkextendedkeypurpose-usage:

.. code-block:: rsyslog

   input(type="imbeats" port="5044" streamDriver.checkExtendedKeyPurpose="on")

See also
--------
See also :doc:`../../configuration/modules/imbeats`.
