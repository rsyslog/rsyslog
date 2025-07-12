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
individually and the resulting tag is stored in a custom variable.

Default labels
--------------

| Label     | Description                                                |
| --------- | ---------------------------------------------------------- |
| NOISE     | Can be ignored, redundant, or irrelevant for most purposes |
| REGULAR   | Normal messages of operational interest                    |
| IMPORTANT | Should be logged and may indicate early signs of issues    |
| CRITICAL  | Indicates immediate or serious problems                    |

Configuration Parameters
=======================

- ``provider``: which backend to use (``gemini`` or ``gemini_mock``)
- ``tag``: message variable name for the classification
- ``model``: AI model identifier (provider specific)
- ``expert.initial_prompt``: optional custom prompt text. If unset, the
  following default is used::

  You are a tool that classifies log messages. Given an array of log
  messages, respond with a JSON array of labels: 'NOISE', 'REGULAR',
  'IMPORTANT', or 'CRITICAL' — one per message, in order. Do not
  include explanations or additional formatting.
- ``inputproperty``: which message property to classify
- ``apikey``: API key for the provider
- ``apikey_file``: file containing the API key

Example
=======

.. code-block:: none

   module(load="mmaitag")
   action(type="mmaitag" provider="gemini" apikey="ABC" tag="$.aitag")


