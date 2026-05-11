.. _param-imbeats-port:
.. _imbeats.parameter.input.port:

port
====

.. meta::
   :description: Listening port for imbeats.
   :keywords: rsyslog, imbeats, port

.. index::
   single: imbeats; port
   single: port

.. summary-start

Set the TCP port on which the imbeats listener accepts Lumberjack v2 clients.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/imbeats`.

:Name: port
:Scope: input
:Type: string
:Default: input=none
:Required?: yes
:Introduced: 8.2604.0

Description
-----------
Set the TCP port on which the imbeats listener accepts Lumberjack v2 clients.

Input usage
-----------
.. _param-imbeats-input-port:
.. _imbeats.parameter.input.port-usage:

.. code-block:: rsyslog

   input(type="imbeats" port="5044")

See also
--------
See also :doc:`../../configuration/modules/imbeats`.
