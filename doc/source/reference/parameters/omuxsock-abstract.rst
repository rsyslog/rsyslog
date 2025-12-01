.. _param-omuxsock-abstract:
.. _omuxsock.parameter.module.abstract:

abstract
==========

.. index::
   single: omuxsock; abstract
   single: abstract

.. summary-start

This indicates whether the socketName should be used as an abstract socket address or not.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/omuxsock`.

:Name: abstract
:Scope: module, action
:Type: boolean
:Default: module="0", action="0"
:Required?: no
:Introduced: v8.2512

Description
-----------
This indicates whether the socket name is abstract or not.  Abstract socket names are generally only supported under Linux, and differ from non-abstract socket names by not requiring a name in the filesystem. The default is False (0).

Module usage
------------
.. _param-omuxsock-action-abstract:
.. _omuxsock.parameter.action.abstract-usage:

If abstract is set at the module level, then it becomes the default abstract setting for the first unnamed action.  It only applies to a single unnamed action, so it is primarily just a short-hand notation for when only a single omuxsock action is required.


.. code-block:: rsyslog

   module(load="omuxsock" socketName="/tmp/socksample" abstract="0")

Action usage
------------
.. code-block:: rsyslog

   action(type="omuxsock" socketName="/tmp/example" abstract="0")
   action(type="omuxsock" socketName="rsyslogd.msg.handler" abstract="1")

.. note::

   :ref:`param-omuxsock-socketname`, :ref:`param-omuxsock-abstract`, and :ref:`param-omuxsock-sockettype` constitute a logical configuration tuple.  They are applied together as a whole tuple from a module level configuration to an action.  Therefore, all three parameters must be explicitly specified in the action, or none may be specified in the action.  In the latter case, the default is taken from the module.

See also
--------
See also :doc:`../../configuration/modules/omuxsock`.
