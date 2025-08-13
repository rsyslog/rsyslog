.. _param-imjournal-defaulttag:
.. _imjournal.parameter.module.defaulttag:

.. meta::
   :tag: module:imjournal
   :tag: parameter:defaultTag

defaultTag
==========

.. index::
   single: imjournal; defaultTag
   single: defaultTag

.. summary-start

Provides a default tag when both ``SYSLOG_IDENTIFIER`` and ``_COMM`` are missing.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/imjournal`.

:Name: defaultTag
:Scope: module
:Type: string (see :doc:`../../rainerscript/constant_strings`)
:Default: module=journal
:Required?: no
:Introduced: 8.2312.0

Description
-----------
Defines the tag value used when neither an identifier string nor process name is
available in the journal entry.

Module usage
------------
.. _param-imjournal-module-defaulttag:
.. _imjournal.parameter.module.defaulttag-usage:
.. code-block:: rsyslog

   module(load="imjournal" defaultTag="...")

Notes
-----
- Earlier documentation incorrectly described this option as ``binary``.

See also
--------
See also :doc:`../../configuration/modules/imjournal`.
