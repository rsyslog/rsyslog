.. _param-omuxsock-networknamespace:
.. _omuxsock.parameter.module.networknamespace:

networkNamespace
================

.. index::
   single: omuxsock; networkNamespace
   single: networkNamespace

.. summary-start

This is the name of the network namespace.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/omuxsock`.

:Name: networkNamespace
:Scope: module, action
:Type: string
:Default: module=none, action=none
:Required?: no
:Introduced: v8.2512

Description
-----------
Sets a name for the ``networknamespace`` property. If no network namespace is set, the local (startup) network namespace is used by default.  The namespace must be created (and interfaces appropriately configured) prior to starting rsyslogd. rsyslogd must be explicitly built with namespace support, otherwise an error will occur when the namespace is subsequently used against an input stream.  The underlying operating system must also support the setns() system call, otherwise the input instance will be unable to bind within the given namespace.

.. note::

   The network namespace should exist as a named file under /var/run/netns/. In particular, the namespace must be created prior to starting rsyslogd. Rsyslogd must be explicitly built with namespace support, otherwise an error will occur when the namespace is subsequently used by an output action.  The underlying operating system must also support the setns() system call, otherwise the output action will be unable to operate within the given namespace.

Module usage
------------
.. _omuxsock.parameter.networknamespace-usage:

If networkNamespace is set at the module level, then it becomes the default networkNamespace setting for all omuxsock actions.

.. code-block:: rsyslog

   module(load="omuxsock" networkNamespace="management")

Action usage
------------
.. _omuxsock.parameter.action.networknamespace:

   An empty string (``""``) can be used at the action level to override a module level ``networkNamespace`` setting and explicitly use the startup network namespace for that action.

.. code-block:: rsyslog

   action(type="omuxsock" socketName="/tmp/example" networkNamespace="routing")
   action(type="omuxsock" socketName="rsyslogd.msg.handler" networkNamespace="")

See also
--------
See also :doc:`../../configuration/modules/omuxsock`.
