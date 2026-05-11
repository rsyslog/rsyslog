.. _param-imbeats-permittedpeer:
.. _imbeats.parameter.input.permittedpeer:

PermittedPeer
=============

.. meta::
   :description: Allowed TLS peer names for imbeats.
   :keywords: rsyslog, imbeats, permittedpeer

.. index::
   single: imbeats; PermittedPeer
   single: PermittedPeer

.. summary-start

Restrict accepted TLS peers to the configured certificate names.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/imbeats`.

:Name: PermittedPeer
:Scope: input
:Type: array
:Default: input=none
:Required?: no
:Introduced: 8.2604.0

Description
-----------
Restrict accepted TLS peers to the configured certificate names.

Input usage
-----------
.. _param-imbeats-input-permittedpeer:
.. _imbeats.parameter.input.permittedpeer-usage:

.. code-block:: rsyslog

   input(type="imbeats" port="5044" permittedPeer=["beat.example.net"])

See also
--------
See also :doc:`../../configuration/modules/imbeats`.
