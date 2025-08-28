.. _param-omprog-begintransactionmark:
.. _omprog.parameter.action.begintransactionmark:

beginTransactionMark
=====================

.. index::
   single: omprog; beginTransactionMark
   single: beginTransactionMark

.. summary-start

Defines the marker sent to denote the start of a transaction.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/omprog`.

:Name: beginTransactionMark
:Scope: action
:Type: string
:Default: action="BEGIN TRANSACTION"
:Required?: no
:Introduced: 8.31.0

Description
-----------
Allows specifying the mark message that rsyslog will send to the external
program to indicate the start of a transaction (batch). This parameter is
ignored if :ref:`param-omprog-usetransactions` is disabled.

Action usage
------------
.. _param-omprog-action-begintransactionmark:
.. _omprog.parameter.action.begintransactionmark-usage:

.. code-block:: rsyslog

   action(type="omprog"
          useTransactions="on"
          beginTransactionMark="BEGIN TRANSACTION")

See also
--------
See also :doc:`../../configuration/modules/omprog`.
