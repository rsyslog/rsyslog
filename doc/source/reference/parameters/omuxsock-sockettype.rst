.. _param-omuxsock-sockettype:
.. _omuxsock.parameter.module.sockettype:

socketType
==========

.. index::
   single: omuxsock; socketType
   single: socketType

.. summary-start

This is the type of the socket (DGRAM, STREAM, SEQPACKET).

.. summary-end

This parameter applies to :doc:`../../configuration/modules/omuxsock`.

:Name: socketType
:Scope: module, action
:Type: string
:Default: module=DGRAM, action=DGRAM
:Required?: no
:Introduced: v8.2512

Description
-----------
This indicates the type of socket.  It can take on the value DGRAM to indicate a datagram socket type (the default) or STREAM to indicate a streaming socket type. When supported by the underlying operating system, it can also have the value SEQPACKET to indicate a sequenced packet socket type.

Module usage
------------
.. _omuxsock.parameter.module.sockettype-usage:

A socketType set at the module level becomes the default socketType for the first unnamed action.  It only applies to a single unnamed action, so it is primarily just a short-hand notation for when only a single omuxsock action is required.

.. code-block:: rsyslog

   module(load="omuxsock" socketType="DGRAM")

Action usage
------------
.. _omuxsock.parameter.action.sockettype-usage:
.. code-block:: rsyslog

   action(type="omuxsock" socketType="DGRAM")
   action(type="omuxsock" socketType="STREAM")
   action(type="omuxsock" socketType="SEQPACKET")

.. note::

   :ref:`param-omuxsock-socketname`, :ref:`param-omuxsock-abstract`, and :ref:`param-omuxsock-sockettype` constitute a logical configuration tuple.  They are applied together as a whole tuple from a module level configuration to an action.  Therefore, all three parameters must be explicitly specified in the action, or none may be specified in the action.  In the latter case, the default is taken from the module.

See also
--------
See also :doc:`../../configuration/modules/omuxsock`.
