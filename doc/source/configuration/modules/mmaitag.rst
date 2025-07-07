.. index:: ! mmaitag

****************************
AI-based classification (mmaitag)
****************************

================  ================================
**Module Name:**  mmaitag
**Author:**       Adiscon
**Available:**    9.0+
================  ================================

Purpose
=======

The **mmaitag** module enriches log messages with classification tags
obtained from an external AI service. Each message is sent to the provider
in batches and the resulting tag is stored in a custom variable.

Configuration Parameters
=======================

- ``provider``: which backend to use (``gemini`` or ``gemini_mock``)
- ``tag``: message variable name for the classification
- ``model``: AI model identifier (provider specific)
- ``prompt``: custom prompt text
- ``inputproperty``: which message property to classify
- ``apikey``: API key for the provider
- ``apikey_file``: file containing the API key

Example
=======

.. code-block:: none

   module(load="mmaitag")
   action(type="mmaitag" provider="gemini" apikey="ABC" tag="$.aitag")


