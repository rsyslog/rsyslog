.. _param-imkubernetes-freshstarttail:
.. _imkubernetes.parameter.module.freshstarttail:

FreshStartTail
==============

.. index::
   single: imkubernetes; FreshStartTail
   single: FreshStartTail

.. summary-start

Start newly discovered files at end-of-file instead of replaying existing
content; default ``off``.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/imkubernetes`.

:Name: FreshStartTail
:Scope: module
:Type: binary
:Default: ``off``
:Required?: no
:Introduced: 8.2604.0

Description
-----------
When enabled, ``imkubernetes`` seeks to the current end of a file the first
time it sees that file. Use this when you only want records appended after
startup.

Module usage
------------

.. code-block:: rsyslog

   module(load="imkubernetes" FreshStartTail="on")

See also
--------
See also :doc:`../../configuration/modules/imkubernetes`.
