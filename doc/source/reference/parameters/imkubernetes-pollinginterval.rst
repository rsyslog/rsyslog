.. _param-imkubernetes-pollinginterval:
.. _imkubernetes.parameter.module.pollinginterval:

PollingInterval
===============

.. index::
   single: imkubernetes; PollingInterval
   single: PollingInterval

.. summary-start

Seconds between filesystem scans for matching log files; default ``1``.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/imkubernetes`.

:Name: PollingInterval
:Scope: module
:Type: integer
:Default: ``1``
:Required?: no
:Introduced: 8.2604.0

Description
-----------
Controls how often ``imkubernetes`` refreshes the configured glob and tails
newly discovered files.

Module usage
------------

.. code-block:: rsyslog

   module(load="imkubernetes" PollingInterval="1")

See also
--------
See also :doc:`../../configuration/modules/imkubernetes`.
