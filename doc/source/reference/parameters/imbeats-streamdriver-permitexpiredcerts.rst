.. _param-imbeats-streamdriver-permitexpiredcerts:
.. _imbeats.parameter.input.streamdriver-permitexpiredcerts:

StreamDriver.PermitExpiredCerts
===============================

.. meta::
   :description: Expired certificate policy for imbeats TLS peers.
   :keywords: rsyslog, imbeats, permitexpiredcerts

.. index::
   single: imbeats; StreamDriver.PermitExpiredCerts
   single: StreamDriver.PermitExpiredCerts

.. summary-start

Control how the imbeats TLS stream driver handles expired peer certificates.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/imbeats`.

:Name: StreamDriver.PermitExpiredCerts
:Scope: input
:Type: string
:Default: input=off
:Required?: no
:Introduced: 8.2604.0

Description
-----------
Control how the imbeats TLS stream driver handles expired peer certificates.

Input usage
-----------
.. _param-imbeats-input-streamdriver-permitexpiredcerts:
.. _imbeats.parameter.input.streamdriver-permitexpiredcerts-usage:

.. code-block:: rsyslog

   input(type="imbeats" port="5044" streamDriver.permitExpiredCerts="off")

See also
--------
See also :doc:`../../configuration/modules/imbeats`.
