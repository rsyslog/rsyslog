.. index:: ! mmaitag

.. meta::
   :description: Configure mmaitag AI classification and its bounded Gemini request behavior.
   :keywords: mmaitag, Gemini, AI classification, response limits, timeouts

********************************
mmaitag: AI-based classification
********************************

================  =======
**Module Name:**  mmaitag
**Author:**       Adiscon
**Available:**    9.0+
================  =======

Purpose
=======

.. summary-start

The **mmaitag** module enriches log messages with classification tags
obtained from an external AI service. Each message is sent to the provider
individually and the resulting tag is stored in a custom variable.

.. summary-end

Gemini response limits
----------------------

The Gemini provider bounds each synchronous request so that a stalled or
malformed provider response cannot consume resources indefinitely. It accepts
at most 1 MiB (1,048,576 bytes) of response data, allows 60 seconds for the
complete transfer and 10 seconds to establish the connection, and aborts a
transfer that remains below one byte per second for 15 seconds.

When a request exceeds a limit or otherwise fails, ``mmaitag`` uses the
``REGULAR`` fallback tag and records an error without copying the provider
response body into rsyslog diagnostics.

Default labels
--------------

=========  ============================================================
**Label**  **Description**
NOISE      Can be ignored, redundant, or irrelevant for most purposes
REGULAR    Normal messages of operational interest
IMPORTANT  Should be logged and may indicate early signs of issues
CRITICAL   Indicates immediate or serious problems
=========  ============================================================


Configuration Parameters
========================

.. note::

   Parameter names are case-insensitive; camelCase is recommended for
   readability.

Action Parameters
-----------------

.. list-table::
   :widths: 30 70
   :header-rows: 1

   * - Parameter
     - Summary
   * - :ref:`param-mmaitag-provider`
     - .. include:: ../../reference/parameters/mmaitag-provider.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`param-mmaitag-tag`
     - .. include:: ../../reference/parameters/mmaitag-tag.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`param-mmaitag-model`
     - .. include:: ../../reference/parameters/mmaitag-model.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`param-mmaitag-expert-initialprompt`
     - .. include:: ../../reference/parameters/mmaitag-expert-initialprompt.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`param-mmaitag-inputproperty`
     - .. include:: ../../reference/parameters/mmaitag-inputproperty.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`param-mmaitag-apikey`
     - .. include:: ../../reference/parameters/mmaitag-apikey.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`param-mmaitag-apikey_file`
     - .. include:: ../../reference/parameters/mmaitag-apikey_file.rst
        :start-after: .. summary-start
        :end-before: .. summary-end

.. toctree::
   :hidden:

   ../../reference/parameters/mmaitag-provider
   ../../reference/parameters/mmaitag-tag
   ../../reference/parameters/mmaitag-model
   ../../reference/parameters/mmaitag-expert-initialprompt
   ../../reference/parameters/mmaitag-inputproperty
   ../../reference/parameters/mmaitag-apikey
   ../../reference/parameters/mmaitag-apikey_file

Example
=======

.. code-block:: rsyslog

    module(load="mmaitag")
    action(type="mmaitag" provider="gemini" apikey="ABC" tag="$.aitag")
