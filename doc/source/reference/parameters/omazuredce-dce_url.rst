.. _param-omazuredce-dce_url:
.. _omazuredce.parameter.action.dce_url:

.. meta::
   :description: Reference for the omazuredce dce_url parameter.
   :keywords: rsyslog, omazuredce, dce_url, azure, logs ingestion

dce_url
=======

.. index::
   single: omazuredce; dce_url
   single: dce_url

.. summary-start

Defines the Azure Data Collection Endpoint base URL used for batch submission.

.. summary-end

This parameter applies to :doc:`../../configuration/modules/omazuredce`.

:Name: dce_url
:Scope: action
:Type: string
:Default: none
:Required?: yes
:Introduced: Not specified

Description
-----------
``dce_url`` is the base HTTPS endpoint for the Azure Logs Ingestion API. The
module appends the DCR and stream path segments to this URL when constructing
requests.

Both forms with and without a trailing slash are accepted.

Action usage
------------
.. _omazuredce.parameter.action.dce_url-usage:

.. code-block:: rsyslog

   action(
      type="omazuredce"
      dce_url="https://<dce-name>.<region>.ingest.monitor.azure.com"
      ...
   )

See also
--------
See also :doc:`../../configuration/modules/omazuredce`.
