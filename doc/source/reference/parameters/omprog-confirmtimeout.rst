.. _param-omprog-confirmtimeout:
.. _omprog.parameter.action.confirmtimeout:

confirmTimeout
==============

.. index::
   single: omprog; confirmTimeout
   single: confirmTimeout

.. summary-start

Sets the maximum time in milliseconds to wait for message confirmations.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/omprog`.

:Name: confirmTimeout
:Scope: action
:Type: integer
:Default: action=10000
:Required?: no
:Introduced: 8.38.0

Description
-----------
Specifies how long rsyslog must wait for the external program to confirm
each message when :ref:`param-omprog-confirmmessages` is set to "on".
If the program does not send a response within this timeout, it will be restarted
(see :ref:`param-omprog-signalonclose`, :ref:`param-omprog-closetimeout` and
:ref:`param-omprog-killunresponsive` for details on the cleanup sequence).
The value must be expressed in milliseconds and must be greater than zero.

.. seealso::

   `Interface between rsyslog and external output plugins
   <https://github.com/rsyslog/rsyslog/blob/master/plugins/external/INTERFACE.md>`_

Action usage
------------
.. _param-omprog-action-confirmtimeout:
.. _omprog.parameter.action.confirmtimeout-usage:

.. code-block:: rsyslog

   action(type="omprog"
          confirmMessages="on"
          confirmTimeout="20000")

See also
--------
See also :doc:`../../configuration/modules/omprog`.
