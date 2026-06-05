.. _param-imkubernetes-escapelf:
.. _imkubernetes.parameter.module.escapelf:

EscapeLF
========

.. index::
   single: imkubernetes; EscapeLF
   single: EscapeLF

.. summary-start

Escapes embedded line feeds in the submitted message payload; default ``on``.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/imkubernetes`.

:Name: EscapeLF
:Scope: module
:Type: binary
:Default: ``on``
:Required?: no
:Introduced: 8.2604.0

Description
-----------
When enabled, line-feed characters in the final message payload are escaped
before submission so each rsyslog record remains single-line by default.

Module usage
------------

.. code-block:: rsyslog

   module(load="imkubernetes" EscapeLF="on")

See also
--------
See also :doc:`../../configuration/modules/imkubernetes`.
