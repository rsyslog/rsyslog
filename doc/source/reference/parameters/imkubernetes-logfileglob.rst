.. _param-imkubernetes-logfileglob:
.. _imkubernetes.parameter.module.logfileglob:

LogFileGlob
===========

.. index::
   single: imkubernetes; LogFileGlob
   single: LogFileGlob

.. summary-start

Glob that selects Kubernetes log files to tail; default
``/var/log/pods/*/*/*.log``.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/imkubernetes`.

:Name: LogFileGlob
:Scope: module
:Type: path
:Default: ``/var/log/pods/*/*/*.log``
:Required?: no
:Introduced: 8.2604.0

Description
-----------
Use this to point ``imkubernetes`` at the node-local container log files you
want to follow. Common values are ``/var/log/pods/*/*/*.log`` and
``/var/log/containers/*.log``.

Module usage
------------

.. code-block:: rsyslog

   module(load="imkubernetes" LogFileGlob="/var/log/pods/*/*/*.log")

See also
--------
See also :doc:`../../configuration/modules/imkubernetes`.
