.. _param-omprog-usetransactions:
.. _omprog.parameter.action.usetransactions:

useTransactions
===============

.. index::
   single: omprog; useTransactions
   single: useTransactions

.. summary-start

Enables batch processing with begin and commit transaction markers.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/omprog`.

:Name: useTransactions
:Scope: action
:Type: boolean
:Default: action=off
:Required?: no
:Introduced: 8.31.0

Description
-----------
Specifies whether the external program processes the messages in
:doc:`batches <../../development/dev_oplugins>` (transactions). When this
switch is enabled, the logs sent to the program are grouped in transactions.
At the start of a transaction, rsyslog sends a special mark message to the
program (see :ref:`param-omprog-begintransactionmark`). At the end of the
transaction, rsyslog sends another mark message (see
:ref:`param-omprog-committransactionmark`).

If :ref:`param-omprog-confirmmessages` is also set to "on", the program must
confirm both the mark messages and the logs within the transaction. The mark
messages must be confirmed by returning ``OK``, and the individual messages by
returning ``DEFER_COMMIT`` (instead of ``OK``). Refer to the link below for details.

.. seealso::

   `Interface between rsyslog and external output plugins
   <https://github.com/rsyslog/rsyslog/blob/master/plugins/external/INTERFACE.md>`_

.. warning::

   This feature is currently **experimental**. It could change in future releases
   without keeping backwards compatibility with existing configurations or the
   specified interface. There is also a `known issue
   <https://github.com/rsyslog/rsyslog/issues/2420>`_ with the use of
   transactions together with ``confirmMessages=on``.

Action usage
------------
.. _param-omprog-action-usetransactions:
.. _omprog.parameter.action.usetransactions-usage:

.. code-block:: rsyslog

   action(type="omprog" useTransactions="on")

Notes
-----
- Legacy documentation referred to the type as ``binary``; this maps to ``boolean``.

See also
--------
See also :doc:`../../configuration/modules/omprog`.
