.. _param-imtcp-networknamespace:
.. _imtcp.parameter.input.networknamespace:

NetworkNamespace
================

.. index::
   single: imtcp; NetworkNamespace
   single: NetworkNamespace

.. summary-start

Sets the value for the ``networknamespace`` property.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/imtcp`.

:Name: NetworkNamespace
:Scope: module, input
:Type: string (see :doc:`../../rainerscript/constant_strings`)
:Default: input=none
:Required?: no
:Introduced: v8.2512

Description
-----------
Sets the network namespace for the listener.

- If not set, the listener operates in the default (startup) network namespace.
- The specified namespace must exist before rsyslogd starts (e.g., created via ``ip netns add <name>``).
- This feature requires rsyslog to be built with ``setns()`` support. An error will be logged if a namespace is configured but support is missing.
- The underlying operating system must also support network namespaces.

Input usage
-----------
.. _param-imtcp-input-networknamespace:
.. _imtcp.parameter.input.networknamespace-usage:

.. code-block:: rsyslog

   input(type="imtcp" port="514" NetworkNamespace="routing")
   input(type="imtcp" port="514" NetworkNamespace="management")

.. code-block:: rsyslog

   module(load="imtcp" NetworkNamespace="routing")
   input(type="imtcp" port="514")
   input(type="imtcp" port="514" NetworkNamespace="management")

See also
--------
See also :doc:`../../configuration/modules/imtcp`.
