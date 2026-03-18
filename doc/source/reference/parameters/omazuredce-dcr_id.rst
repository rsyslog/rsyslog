.. _param-omazuredce-dcr_id:
.. _omazuredce.parameter.action.dcr_id:

.. meta::
   :description: Reference for the omazuredce dcr_id parameter.
   :keywords: rsyslog, omazuredce, dcr_id, azure, logs ingestion

dcr_id
======

.. index::
   single: omazuredce; dcr_id
   single: dcr_id

.. summary-start

Specifies the Azure Data Collection Rule immutable ID used in the ingestion URL.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/omazuredce`.

:Name: dcr_id
:Scope: action
:Type: string
:Default: none
:Required?: yes
:Introduced: Not specified

Description
-----------
``dcr_id`` identifies the Data Collection Rule that receives the uploaded log
records. The value is inserted into the request path under
``dataCollectionRules/<dcr_id>``.

Action usage
------------
.. _omazuredce.parameter.action.dcr_id-usage:

.. code-block:: rsyslog

   action(type="omazuredce" dcr_id="<dcr-id>" ...)

See also
--------
See also :doc:`../../configuration/modules/omazuredce`.
