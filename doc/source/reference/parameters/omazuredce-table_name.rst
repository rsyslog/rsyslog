.. _param-omazuredce-table_name:
.. _omazuredce.parameter.action.table_name:

.. meta::
   :description: Reference for the omazuredce table_name parameter.
   :keywords: rsyslog, omazuredce, table_name, azure, stream

table_name
==========

.. index::
   single: omazuredce; table_name
   single: table_name

.. summary-start

Sets the stream or table name appended to the Azure ingestion request path.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/omazuredce`.

:Name: table_name
:Scope: action
:Type: string
:Default: none
:Required?: yes
:Introduced: Not specified

Description
-----------
``table_name`` is appended below ``/streams/`` in the final request URL. The
configured value must match a stream accepted by the target Data Collection
Rule.

Action usage
------------
.. _omazuredce.parameter.action.table_name-usage:

.. code-block:: rsyslog

   action(type="omazuredce" table_name="Custom-MyTable_CL" ...)

See also
--------
See also :doc:`../../configuration/modules/omazuredce`.
