.. _param-omuxsock-socketname:
.. _omuxsock.parameter.module.socketname:

socketName
==========

.. index::
   single: omuxsock; socketName
   single: socketName

.. summary-start

This is the name of the socket.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/omuxsock`.

:Name: socketName
:Scope: module, action
:Type: string
:Default: action=none
:Required?: yes
:Introduced: v8.2512

Description
-----------
This is the name of the socket.  There is no default.  Every socket **must** be named.

Module usage
------------
.. _param-omuxsock-module-socketname:
.. _omuxsock.parameter.module.socketname-usage:

A socketName set at the module level becomes the default socketName for the first unnamed action.  It only applies to a single unnamed action, so it is primarily just a short-hand notation for when only a single omuxsock action is required.

.. code-block:: rsyslog

   module(load="omuxsock" socketName="/tmp/socksample")

Action usage
------------
.. _param-omuxsock-action-socketname:
.. _omuxsock.parameter.action.socketname-usage:

.. code-block:: rsyslog

   action(type="omuxsock" socketName="/tmp/socksample")

.. note::

   :ref:`param-omuxsock-socketname`, :ref:`param-omuxsock-abstract`, and :ref:`param-omuxsock-sockettype` constitute a logical configuration tuple.  They are applied together as a whole tuple from a module level configuration to an action.  Therefore, all three parameters must be explicitly specified in the action, or none may be specified in the action.  In the latter case, the default is taken from the module.

See also
--------
See also :doc:`../../configuration/modules/omuxsock`.
