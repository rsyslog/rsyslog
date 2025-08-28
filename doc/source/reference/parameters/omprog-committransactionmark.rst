.. _param-omprog-committransactionmark:
.. _omprog.parameter.action.committransactionmark:

commitTransactionMark
======================

.. index::
   single: omprog; commitTransactionMark
   single: commitTransactionMark

.. summary-start

Defines the marker sent to denote the end of a transaction.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/omprog`.

:Name: commitTransactionMark
:Scope: action
:Type: string
:Default: action="COMMIT TRANSACTION"
:Required?: no
:Introduced: 8.31.0

Description
-----------
Allows specifying the mark message that rsyslog will send to the external
program to indicate the end of a transaction (batch). This parameter is
ignored if :ref:`param-omprog-usetransactions` is disabled.

Action usage
------------
.. _param-omprog-action-committransactionmark:
.. _omprog.parameter.action.committransactionmark-usage:

.. code-block:: rsyslog

   action(type="omprog"
          useTransactions="on"
          commitTransactionMark="COMMIT TRANSACTION")

See also
--------
See also :doc:`../../configuration/modules/omprog`.
