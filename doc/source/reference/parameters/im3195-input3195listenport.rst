.. _param-im3195-input3195listenport:
.. _im3195.parameter.module.input3195listenport:

Input3195ListenPort
-------------------

.. index::
   single: im3195; Input3195ListenPort
   single: Input3195ListenPort
   single: im3195; $Input3195ListenPort
   single: $Input3195ListenPort

.. summary-start

The TCP port on which im3195 listens for RFC 3195 messages.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/im3195`.

.. note::

   This is a legacy global directive. The im3195 module does not support
   the modern ``input()`` syntax.

:Name: Input3195ListenPort
:Scope: module
:Type: integer
:Default: 601
:Required?: no
:Introduced: Not documented

Description
~~~~~~~~~~~

This parameter specifies the TCP port for incoming RFC 3195 messages. The
default port is 601, the IANA-assigned port for the BEEP protocol on which RFC
3195 is based.

Since directive names are case-insensitive, the PascalCase form
``$Input3195ListenPort`` is recommended for readability.

Module usage
~~~~~~~~~~~~
.. _im3195.parameter.module.input3195listenport-usage:

.. code-block:: rsyslog

   $ModLoad im3195
   $Input3195ListenPort 1601

See also
~~~~~~~~
See also :doc:`../../configuration/modules/im3195`.
