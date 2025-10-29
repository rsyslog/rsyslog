.. _faq-vespa-ai-integration:

.. meta::
   :description: Configure rsyslog to forward events to Vespa.ai using omhttp and Vespa's Document v1 API.
   :keywords: vespa, omhttp, document api, search, analytics

.. summary-start

Learn how to push rsyslog events into Vespa.ai's search platform with omhttp and Vespa's Document v1 API.

.. summary-end

FAQ: Sending Logs from rsyslog to Vespa.ai
==========================================

`Vespa.ai <https://vespa.ai>`_ is an open-source engine for real-time search, recommendation, and analytics at scale. It ingests structured documents,
indexes them for low-latency search, and executes machine-learned ranking or aggregations directly where the data lives.
Connecting rsyslog to Vespa.ai lets you ship data events into a streaming analytics platform that can keep indexes and features
up-to-date while serving low-latency queries across massive datasets.

How do I send rsyslog messages to Vespa.ai?
-------------------------------------------

Use the :doc:`omhttp <../configuration/modules/omhttp>` output module to POST JSON documents to Vespa's Document v1 API. Start
from the following template, which uses ``omhttp`` to POST individual rsyslog messages as Vespa documents with randomly
generated IDs:

.. code-block:: none

   module(load="omhttp")

   # Template that outputs the rsyslog message field inside a Vespa document
   template(name="json-message" type="list" option.json="on") {
       constant(value="{ \"fields\": { \"message\": \"")
       property(name="msg")
       constant(value="\" }}")
   }

   # Template that generates a unique document path per message
   template(name="vespaDocUrl" type="string"
     string="/document/v1/NAMESPACE/DOCTYPE/docid/%uuid%"
   )

   # Send each message to Vespa using HTTP POST
   action(type="omhttp"
          server="vespa"
          serverport="8080"
          useHttps="off"
          batch="off"
          dynrestpath="on"
          restpath="vespaDocUrl"
          template="json-message")

Replace ``NAMESPACE`` and ``DOCTYPE`` with your Vespa application values. Set ``server`` and ``serverport`` to match the Vespa
container endpoint (``localhost`` and ``8080`` when running Vespa locally). When targeting Vespa Cloud or other TLS-protected
endpoints, switch ``useHttps`` to ``on`` and provide the relevant certificates as described in :doc:`the omhttp module
reference <../configuration/modules/omhttp>`.

Why is this integration useful?
-------------------------------

* **Operational visibility:** Index rsyslog events alongside other Vespa documents so operators can search by message text,
  host metadata, or custom JSON fields in real time.
* **Feature serving:** Vespa can expose recent data-derived features directly to downstream applications without requiring a
  separate streaming or ETL layer.
* **Scalability:** Vespa distributes both storage and query execution, allowing the data pipeline to scale with the same
  cluster that serves analytics queries.

Tips for production use
-----------------------

* Define richer JSON documents by extending the ``json-message`` template with additional fields (for example, structured data
  from :doc:`mmjsonparse <../configuration/modules/mmjsonparse>` or custom message properties).
* Enable :doc:`impstats <../configuration/modules/impstats>` to monitor delivery throughput and retry behavior when Vespa slows
  down or becomes temporarily unavailable.
  Consider :doc:`imhttp <../configuration/modules/imhttp>` if you want to publish a Prometheus metrics endpoint.
* Review Vespa's `Document v1 API guide <https://docs.vespa.ai/en/document-v1-api-guide.html>`_ for additional operations such as
  updates and deletes.
