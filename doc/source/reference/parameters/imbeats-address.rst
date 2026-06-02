.. _param-imbeats-address:
.. _imbeats.parameter.input.address:

address
========

.. meta::
   :description: Bind address for the imbeats listener.
   :keywords: rsyslog, imbeats, address

.. index::
   single: imbeats; address
   single: address

.. summary-start

Bind the imbeats listener to a specific local address instead of all interfaces.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/imbeats`.

:Name: address
:Scope: input
:Type: string
:Default: input=all interfaces
:Required?: no
:Introduced: 8.2604.0

Description
-----------
Bind the imbeats listener to a specific local address instead of all interfaces.

Input usage
-----------
.. _param-imbeats-input-address:
.. _imbeats.parameter.input.address-usage:

.. code-block:: rsyslog

   input(type="imbeats" port="5044" address="127.0.0.1")

See also
--------
See also :doc:`../../configuration/modules/imbeats`.
