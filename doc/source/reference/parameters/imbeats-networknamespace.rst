.. _param-imbeats-networknamespace:
.. _imbeats.parameter.input.networknamespace:

NetworkNamespace
================

.. meta::
   :description: Network namespace for imbeats listener sockets.
   :keywords: rsyslog, imbeats, network namespace

.. index::
   single: imbeats; NetworkNamespace
   single: NetworkNamespace

.. summary-start

Open imbeats listener sockets inside the specified Linux network namespace.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/imbeats`.

:Name: NetworkNamespace
:Scope: input
:Type: string
:Default: input=none
:Required?: no
:Introduced: 8.2604.0

Description
-----------
Open imbeats listener sockets inside the specified Linux network namespace.

Input usage
-----------
.. _param-imbeats-input-networknamespace:
.. _imbeats.parameter.input.networknamespace-usage:

.. code-block:: rsyslog

   input(type="imbeats" port="5044" networkNamespace="ns1")

See also
--------
See also :doc:`../../configuration/modules/imbeats`.
