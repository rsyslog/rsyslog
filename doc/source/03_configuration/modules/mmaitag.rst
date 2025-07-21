.. index:: ! mmaitag

*********************************
AI-based classification (mmaitag)
*********************************

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

The module uses a pluggable provider interface, allowing different AI backends
to be used without changing the core functionality. Currently supported providers
include Google Gemini and a mock provider for testing.

Default labels
--------------

| Label     | Description                                           |
| --------- | ---------------------------------------------------------- |
| NOISE     | Can be ignored, redundant, or irrelevant for most purposes |
| REGULAR   | Normal messages of operational interest                   |
| IMPORTANT | Should be logged and may indicate early signs of issues    |
| CRITICAL  | Indicates immediate or serious problems                   |

Configuration Parameters
========================

- ``provider``: which backend to use (``gemini`` or ``gemini_mock``)
- ``tag``: message variable name for the classification (default: ``$.aitag``)
- ``model``: AI model identifier (provider specific, default: ``gemini-2.0-flash``)
- ``expert.initial_prompt``: optional custom prompt text. If unset, the
  following default is used:

  .. code-block:: text

     Task: Classify the log message that follows. 
     Output: Exactly one label from this list: NOISE, REGULAR, IMPORTANT, CRITICAL. 
     Restrictions: No other text, explanations, formatting, or newline characters.

- ``inputproperty``: which message property to classify (default: raw message)
- ``apikey``: API key for the provider
- ``apikey_file``: file containing the API key (alternative to ``apikey``)

Performance and Error Handling
==============================

- Messages are processed individually, which may impact performance for high-volume logging
- Network failures or API rate limits may cause classification to fail
- Failed classifications do not prevent message processing - the message continues without the tag
- API keys can be provided directly or read from a file for better security

Example
=======

Basic configuration with Gemini provider:

.. code-block:: rsyslog

    module(load="mmaitag")
    action(type="mmaitag" provider="gemini" apikey="YOUR_API_KEY" tag="$.aitag")

Using a file for the API key:

.. code-block:: rsyslog

    module(load="mmaitag")
    action(type="mmaitag" 
           provider="gemini" 
           apikey_file="/path/to/api_key.txt" 
           tag="$.classification")

Custom prompt and model:

.. code-block:: rsyslog

    module(load="mmaitag")
    action(type="mmaitag" 
           provider="gemini" 
           apikey="YOUR_API_KEY"
           model="gemini-1.5-pro"
           expert.initial_prompt="Classify this log message as LOW, MEDIUM, or HIGH priority"
           tag="$.priority")


