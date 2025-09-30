.. _param-im3195-input3195listenport:
.. _im3195.directive.input3195listenport:

$Input3195ListenPort
====================

.. index::
   single: im3195; $Input3195ListenPort
   single: $Input3195ListenPort

.. summary-start

Sets the TCP port where im3195 listens for RFC 3195 messages.

.. summary-end

This directive applies to :doc:`../../configuration/modules/im3195`.

.. note::

   This is a legacy global directive. The im3195 module does not support
   the modern ``input()`` syntax.

:Directive: $Input3195ListenPort
:Type: integer
:Default: 601
:Required?: no
:Introduced: Not documented

Description
-----------

The default port is 601. This is the IANA-assigned port for the BEEP protocol,
which RFC 3195 is based on.

Since directive names are case-insensitive, ``$input3195listenport`` also
works.

Example
-------

.. code-block:: rsyslog

   $ModLoad im3195
   $Input3195ListenPort 1601

See also
--------
See also :doc:`../../configuration/modules/im3195`.
