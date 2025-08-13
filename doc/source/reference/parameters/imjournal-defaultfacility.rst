.. _param-imjournal-defaultfacility:
.. _imjournal.parameter.module.defaultfacility:

.. meta::
   :tag: module:imjournal
   :tag: parameter:DefaultFacility

DefaultFacility
===============

.. index::
   single: imjournal; DefaultFacility
   single: DefaultFacility

.. summary-start

Fallback facility used if a journal entry lacks ``SYSLOG_FACILITY``.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/imjournal`.

:Name: DefaultFacility
:Scope: module
:Type: word
:Default: module=LOG_USER
:Required?: no
:Introduced: at least 5.x, possibly earlier

Description
-----------
Specifies the facility assigned to messages missing the ``SYSLOG_FACILITY``
field. Values may be given as facility names or numbers.

Module usage
------------
.. _param-imjournal-module-defaultfacility:
.. _imjournal.parameter.module.defaultfacility-usage:
.. code-block:: rsyslog

   module(load="imjournal" DefaultFacility="...")

Legacy names (for reference)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~
.. _imjournal.parameter.legacy.imjournaldefaultfacility:

- $ImjournalDefaultFacility — maps to DefaultFacility (status: legacy)

.. index::
   single: imjournal; $ImjournalDefaultFacility
   single: $ImjournalDefaultFacility

See also
--------
See also :doc:`../../configuration/modules/imjournal`.
