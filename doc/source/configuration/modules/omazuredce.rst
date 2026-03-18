.. _module-omazuredce:

.. meta::
   :description: Configuration reference for the omazuredce Azure Monitor Logs Ingestion output module.
   :keywords: rsyslog, omazuredce, azure, dce, dcr, monitor, logs ingestion

.. summary-start

``omazuredce`` batches JSON records and sends them to the Azure Monitor Logs
Ingestion API by using a Data Collection Endpoint (DCE), Data Collection Rule
(DCR), and Microsoft Entra client-credentials authentication.

.. summary-end

******************************************************
omazuredce: Azure Monitor Logs Ingestion Output Module
******************************************************

===========================  ===========================================================================
**Module Name:**             **omazuredce**
**Author:**                  Jan Kängsepp
**Available since:**         v8.2604
===========================  ===========================================================================


Purpose
=======

This module provides native support for forwarding log data to the Azure
Monitor Logs Ingestion API.

Each message rendered by the configured template must be a single JSON object.
``omazuredce`` collects those objects into a JSON array, requests an OAuth
access token from Microsoft Entra ID, and posts the resulting batch to the
configured Azure Data Collection Endpoint.


Notable Features
================

- Size-aware batching with a configurable ``max_batch_bytes`` limit
- Timed flushing for partially filled batches via ``flush_timeout_ms``
- Automatic OAuth token acquisition for the
  ``https://monitor.azure.com/.default`` scope
- Retry-friendly error handling that cooperates with rsyslog action suspension


Requirements
============

To use ``omazuredce``, you need the following:

- ``libcurl`` support at build time
- A valid Azure Data Collection Endpoint URL
- A Data Collection Rule ID
- A target stream or table name accepted by the DCR
- A Microsoft Entra application with ``client_id``, ``client_secret``, and
  ``tenant_id`` values that can request tokens for
  ``https://monitor.azure.com/.default``
- A template that renders one valid JSON object per message, or the built-in
  ``StdJSONFmt`` default

The module is built only when ``./configure`` is invoked with
``--enable-omazuredce=yes``.


Configuration Parameters
========================

.. note::

   Parameter names are case-insensitive; snake_case is used in the examples
   because the module's parameter names contain underscores.

.. note::

   This module supports action parameters only.

Action Parameters
-----------------

.. list-table::
   :widths: 30 70
   :header-rows: 1

   * - Parameter
     - Summary
   * - :ref:`param-omazuredce-template`
     - .. include:: ../../reference/parameters/omazuredce-template.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`param-omazuredce-client_id`
     - .. include:: ../../reference/parameters/omazuredce-client_id.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`param-omazuredce-client_secret`
     - .. include:: ../../reference/parameters/omazuredce-client_secret.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`param-omazuredce-tenant_id`
     - .. include:: ../../reference/parameters/omazuredce-tenant_id.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`param-omazuredce-dce_url`
     - .. include:: ../../reference/parameters/omazuredce-dce_url.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`param-omazuredce-dcr_id`
     - .. include:: ../../reference/parameters/omazuredce-dcr_id.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`param-omazuredce-table_name`
     - .. include:: ../../reference/parameters/omazuredce-table_name.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`param-omazuredce-max_batch_bytes`
     - .. include:: ../../reference/parameters/omazuredce-max_batch_bytes.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`param-omazuredce-flush_timeout_ms`
     - .. include:: ../../reference/parameters/omazuredce-flush_timeout_ms.rst
        :start-after: .. summary-start
        :end-before: .. summary-end

.. toctree::
   :hidden:

   ../../reference/parameters/omazuredce-template
   ../../reference/parameters/omazuredce-client_id
   ../../reference/parameters/omazuredce-client_secret
   ../../reference/parameters/omazuredce-tenant_id
   ../../reference/parameters/omazuredce-dce_url
   ../../reference/parameters/omazuredce-dcr_id
   ../../reference/parameters/omazuredce-table_name
   ../../reference/parameters/omazuredce-max_batch_bytes
   ../../reference/parameters/omazuredce-flush_timeout_ms


Batching Behavior
=================

``omazuredce`` builds one JSON array per worker and flushes it when one of the
following happens:

- Adding the next record would exceed ``max_batch_bytes``
- The action queue transaction ends and ``flush_timeout_ms`` is set to ``0``
- The batch has been idle for at least ``flush_timeout_ms`` milliseconds

The internal size check is conservative. It includes both the JSON payload size
and an estimate for HTTP headers before sending the request.


Error Handling
==============

The module obtains an OAuth access token before sending data. If Azure returns
``401 Unauthorized`` for a batch request, the module refreshes the token and
returns ``RS_RET_SUSPENDED``. The failed batch is then retried by rsyslog's
central action engine according to the configured action backoff and retry
settings. Other retryable HTTP failures follow the same ``RS_RET_SUSPENDED``
path.

If a rendered message is too large to fit into an empty batch under the current
``max_batch_bytes`` setting, the module logs an error and drops that record.


Example
=======

The following example renders each event as one JSON object and forwards it to
Azure Monitor Logs Ingestion:

.. code-block:: rsyslog

   module(load="omazuredce")

   template(name="tplAzureDce" type="list" option.jsonf="on") {
      property(outname="TimeGenerated" name="timereported" dateFormat="rfc3339" format="jsonf")
      property(outname="Host" name="hostname" format="jsonf")
      property(outname="AppName" name="app-name" format="jsonf")
      property(outname="Message" name="msg" format="jsonf")
   }

   action(
      type="omazuredce"
      template="tplAzureDce"
      client_id="<application-id>"
      client_secret="<client-secret>"
      tenant_id="<tenant-id>"
      dce_url="https://<dce-name>.<region>.ingest.monitor.azure.com"
      dcr_id="<dcr-id>"
      table_name="Custom-MyTable_CL"
      max_batch_bytes="1048576"
      flush_timeout_ms="2000"
   )
